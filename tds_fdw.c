/*------------------------------------------------------------------
*
*				Foreign data wrapper for TDS (Sybase and Microsoft SQL Server)
*
* Author: Geoff Montee
* Name: tds_fdw
* File: tds_fdw/tds_fdw.c
*
* Description:
* This is a PostgreSQL foreign data wrapper for use to connect to databases that use TDS,
* such as Sybase databases and Microsoft SQL server.
*
* This foreign data wrapper requires requires a library that uses the DB-Library interface,
* such as FreeTDS (http://www.freetds.org/). This has been tested with FreeTDS, but not
* the proprietary implementations of DB-Library.
*----------------------------------------------------------------------------
*/

#include "postgres.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "funcapi.h"
#include "access/reloptions.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "mb/pg_wchar.h"
#include "optimizer/cost.h"
#include "storage/fd.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/rel.h"

#if (PG_VERSION_NUM >= 90200)
#include "optimizer/pathnode.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/planmain.h"
#endif


/* DB-Library headers (e.g. FreeTDS */
#include <sybfront.h>
#include <sybdb.h>

/*#define DEBUG*/

PG_MODULE_MAGIC;

/* valid options follow this format */

typedef struct TdsFdwOption
{
	const char *optname;
	Oid optcontext;
} TdsFdwOption;

/* these are valid options */

static struct TdsFdwOption valid_options[] =
{
	{ "servername",		ForeignServerRelationId },
	{ "port",			ForeignServerRelationId },
	{ "username",		UserMappingRelationId },
	{ "password",		UserMappingRelationId },
	{ "database",		ForeignTableRelationId },
	{ "query", 			ForeignTableRelationId },
	{ "table",			ForeignTableRelationId },
	{ NULL,				InvalidOid }
};

/* a column */

typedef struct COL
{
	char *name;
	char *buffer;
	int type, size, status;
} COL;

/* this maintains state */

typedef struct TdsFdwExecutionState
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	char *query;
	int first;
	COL *columns;
	int ncols;
	int row;
} TdsFdwExecutionState;

/* functions called via SQL */

extern Datum tds_fdw_handler(PG_FUNCTION_ARGS);
extern Datum tds_fdw_validator(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(tds_fdw_handler);
PG_FUNCTION_INFO_V1(tds_fdw_validator);

/* FDW callback routines */

static void tdsExplainForeignScan(ForeignScanState *node, ExplainState *es);
static void tdsBeginForeignScan(ForeignScanState *node, int eflags);
static TupleTableSlot* tdsIterateForeignScan(ForeignScanState *node);
static void tdsReScanForeignScan(ForeignScanState *node);
static void tdsEndForeignScan(ForeignScanState *node);

/* routines for 9.2.0+ */
#if (PG_VERSION_NUM >= 90200)
static void tdsGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static void tdsEstimateCosts(PlannerInfo *root, RelOptInfo *baserel, Cost *startup_cost, Cost *total_cost, Oid foreigntableid);
static void tdsGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
static bool tdsAnalyzeForeignTable(Relation relation, AcquireSampleRowsFunc *func, BlockNumber *totalpages);
static ForeignScan* tdsGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses);
/* routines for versions older than 9.2.0 */
#else
static FdwPlan* tdsPlanForeignScan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel);
#endif

/* Helper functions */

static bool tdsIsValidOption(const char *option, Oid context);
static void tdsGetOptions(Oid foreigntableid, char **address, int *port, char **username, char **password, char **database, char **query, char **table);

/* Helper functions for DB-Library API */

int tds_err_handler(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
int tds_msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *svr_name, char *proc_name, int line);

/* default IP address */

static const char *DEFAULT_SERVERNAME = "127.0.0.1";

Datum tds_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdwroutine = makeNode(FdwRoutine);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tds_fdw_handler")
			));
	#endif
	
	#if (PG_VERSION_NUM >= 90200)
	fdwroutine->GetForeignRelSize = tdsGetForeignRelSize;
	fdwroutine->GetForeignPaths = tdsGetForeignPaths;
	fdwroutine->AnalyzeForeignTable = tdsAnalyzeForeignTable;
	fdwroutine->GetForeignPlan = tdsGetForeignPlan;
	#else
	fdwroutine->PlanForeignScan = tdsPlanForeignScan;
	#endif
	
	fdwroutine->ExplainForeignScan = tdsExplainForeignScan;
	fdwroutine->BeginForeignScan = tdsBeginForeignScan;
	fdwroutine->IterateForeignScan = tdsIterateForeignScan;
	fdwroutine->ReScanForeignScan = tdsReScanForeignScan;
	fdwroutine->EndForeignScan = tdsEndForeignScan;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tds_fdw_handler")
			));
	#endif
	
	PG_RETURN_POINTER(fdwroutine);
}

Datum tds_fdw_validator(PG_FUNCTION_ARGS)
{
	List *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid catalog = PG_GETARG_OID(1);
	char *svr_servername = NULL;
	int svr_port = 0;
	char *svr_username = NULL;
	char *svr_password = NULL;
	char *svr_database = NULL;
	char *svr_query = NULL;
	char *svr_table = NULL;
	ListCell *cell;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tds_fdw_validator")
			));
	#endif
	
	foreach (cell, options_list)
	{
		DefElem *def = (DefElem *) lfirst(cell);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Working on option %s", def->defname)
			));
		#endif
		
		if (!tdsIsValidOption(def->defname, catalog))
		{
			TdsFdwOption *opt;
			StringInfoData buf;
			
			initStringInfo(&buf);
			for (opt = valid_options; opt->optname; opt++)
			{
				if (catalog == opt->optcontext)
					appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "", opt->optname);
			}
			
			ereport(ERROR,
				(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					errmsg("Invalid option \"%s\"", def->defname),
					errhint("Valid options in this context are: %s", buf.len ? buf.data : "<none>")
				));
		}
		
		if (strcmp(def->defname, "servername") == 0)
		{
			if (svr_servername)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Redundant option: servername (%s)", defGetString(def))
					));
					
			svr_servername = defGetString(def);	
		}
		
		else if (strcmp(def->defname, "port") == 0)
		{
			if (svr_port)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Redundant option: port (%s)", defGetString(def))
					));
					
			svr_port = atoi(defGetString(def));	
		}
		
		else if (strcmp(def->defname, "username") == 0)
		{
			if (svr_username)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Redundant option: username (%s)", defGetString(def))
					));
					
			svr_username = defGetString(def);	
		}
		
		else if (strcmp(def->defname, "password") == 0)
		{
			if (svr_password)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Redundant option: password (%s)", defGetString(def))
					));
					
			svr_password = defGetString(def);
		}
		
		else if (strcmp(def->defname, "database") == 0)
		{
			if (svr_database)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Redundant option: database (%s)", defGetString(def))
					));
					
			svr_database = defGetString(def);
		}
		
		else if (strcmp(def->defname, "query") == 0)
		{
			if (svr_table)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Conflicting options: query cannot be used with table")
					));
					
			if (svr_query)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Redundant option: query (%s)", defGetString(def))
					));
					
			svr_query = defGetString(def);
		}
		
		else if (strcmp(def->defname, "table") == 0)
		{
			if (svr_query)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Conflicting options: table cannot be used with query")
					));
					
			if (svr_table)
				ereport(ERROR,
					(errcode(ERRCODE_SYNTAX_ERROR),
						errmsg("Redundant option: table (%s)", defGetString(def))
					));
					
			svr_table = defGetString(def);
		}
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tds_fdw_validator")
			));
	#endif
	
	PG_RETURN_VOID();
}

/* validate options for FOREIGN TABLE and FOREIGN SERVER objects using this module */

static bool tdsIsValidOption(const char *option, Oid context)
{
	TdsFdwOption *opt;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsIdValidOption")
			));
	#endif
	
	for (opt = valid_options; opt->optname; opt++)
	{
		if (context == opt->optcontext && strcmp(opt->optname, option) == 0)
			return true;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsIdValidOption")
			));
	#endif
	
	return false;
}

/* get options for FOREIGN TABLE and FOREIGN SERVER objects using this module */

static void tdsGetOptions(Oid foreigntableid, char **servername, int *port, 
	char **username, char **password, char **database, char **query, char **table)
{
	ForeignTable *f_table;
	ForeignServer *f_server;
	UserMapping *f_mapping;
	List *options;
	ListCell *lc;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetOptions")
			));
	#endif
	
	f_table = GetForeignTable(foreigntableid);
	f_server = GetForeignServer(f_table->serverid);
	f_mapping = GetUserMapping(GetUserId(), f_table->serverid);
	
	options = NIL;
	options = list_concat(options, f_table->options);
	options = list_concat(options, f_server->options);
	options = list_concat(options, f_mapping->options);
	
	foreach (lc, options)
	{
		DefElem *def = (DefElem *) lfirst(lc);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Working on option %s", def->defname)
				));
		#endif
		
		if (strcmp(def->defname, "servername") == 0)
		{
			*servername = defGetString(def);
			
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Servername is %s", *servername)
					));
			#endif
		}
		
		else if (strcmp(def->defname, "port") == 0)
		{
			*port = atoi(defGetString(def));
			
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Port is %i", *port)
					));
			#endif
		}
		
		else if (strcmp(def->defname, "username") == 0)
		{
			*username = defGetString(def);
			
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Username is %s", *username)
					));
			#endif
		}
		
		else if (strcmp(def->defname, "password") == 0)
		{
			*password = defGetString(def);
			
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Password is %s", *password)
					));
			#endif
		}

		else if (strcmp(def->defname, "database") == 0)
		{
			*database = defGetString(def);
			
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Database is %s", *database)
					));
			#endif
		}

		else if (strcmp(def->defname, "query") == 0)
		{
			*query = defGetString(def);
			
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Query is %s", *query)
					));
			#endif
		}

		else if (strcmp(def->defname, "table") == 0)
		{
			*table = defGetString(def);
			
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Table is %s", *table)
					));
			#endif
		}		
	}
	
	/* Default values, if not set */
	
	if (!*servername)
	{
		if ((*servername = palloc((strlen(DEFAULT_SERVERNAME) + 1) * sizeof(char))) == NULL)
        	{
                	ereport(ERROR,
                        	(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                                	errmsg("Failed to allocate memory for connection string")
                        	));
        	}

		sprintf(*servername, "%s", DEFAULT_SERVERNAME);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Set servername to default: %s", *servername)
				));
		#endif
	}
	
	/* Check required options */
	
	if (!*table && !*query)
	{
		ereport(ERROR,
			(errcode(ERRCODE_SYNTAX_ERROR),
				errmsg("Either a table or a query must be specified")
			));
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetOptions")
			));
	#endif
}

/* get output for EXPLAIN */

static void tdsExplainForeignScan(ForeignScanState *node, ExplainState *es)
{
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsExplainForeignScan")
			));
	#endif
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsExplainForeignScan")
			));
	#endif
}

/* initiate access to foreign server and database */

static void tdsBeginForeignScan(ForeignScanState *node, int eflags)
{
	char *svr_servername = NULL;
	int svr_port = 0;
	char *svr_username = NULL;
	char *svr_password = NULL;
	char *svr_database = NULL;
	char *svr_query = NULL;
	char *svr_table = NULL;
	LOGINREC *login;
	DBPROCESS *dbproc;
	RETCODE erc;
	TdsFdwExecutionState *festate;
	char *conn_string;
	char *query;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsBeginForeignScan")
			));
	#endif
	
	tdsGetOptions(RelationGetRelid(node->ss.ss_currentRelation), &svr_servername, 
		&svr_port, &svr_username, &svr_password, &svr_database, &svr_query, &svr_table);
		
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Initiating DB-Library")
			));
	#endif
	
	if (dbinit() == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
				errmsg("Failed to initialize DB-Library environment")
			));
	}
	
	dberrhandle(tds_err_handler);
	dbmsghandle(tds_msg_handler);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Getting login structure")
			));
	#endif
	
	if ((login = dblogin()) == NULL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
				errmsg("Failed to initialize DB-Library login structure")
			));
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting login user to %s", svr_username)
			));
	#endif
	
	DBSETLUSER(login, svr_username);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting login password to %s", svr_password)
			));
	#endif
	
	DBSETLPWD(login, svr_password);	
	
	if ((conn_string = palloc((strlen(svr_servername) + 10) * sizeof(char))) == NULL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
				errmsg("Failed to allocate memory for connection string")
			));
	}
	
	if (svr_port)
	{
		sprintf(conn_string, "%s:%i", svr_servername, svr_port);
	}
	
	else
	{
		sprintf(conn_string, "%s", svr_servername);
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Connection string is %s", conn_string)
			));
		ereport(NOTICE,
			(errmsg("Connecting to server")
			));
	#endif
	
	if ((dbproc = dbopen(login, conn_string)) == NULL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
				errmsg("Failed to connect using connection string %s with user %s", conn_string, svr_username)
			));
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Connected successfully")
			));
	#endif
	
	pfree(conn_string);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Selecting database %s", svr_database)
			));
	#endif
	
	if ((erc = dbuse(dbproc, svr_database)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
				errmsg("Failed to select database %s", svr_database)
			));
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Selected database")
			));
		ereport(NOTICE,
			(errmsg("Getting query")
			));
	#endif
	
	if (svr_query)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Query is explicitly set")
				));
		#endif
		
		query = svr_query;
	}
	
	else
	{
		size_t len;
		static const char *query_prefix = "SELECT * FROM ";
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Building query using table")
				));
		#endif
		
		len = strlen(query_prefix) + strlen(svr_table) + 1;
		
		if ((query = palloc(len * sizeof(char))) == NULL)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
					errmsg("Failed to allocate memory for query")
				));
		}
		
		if (snprintf(query, len, "%s%s", query_prefix, svr_table) < 0)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
					errmsg("Failed to build query")
				));
		}
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Value of query is %s", query)
			));
	#endif	
	
	if ((festate = (TdsFdwExecutionState *) palloc(sizeof(TdsFdwExecutionState))) == NULL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
				errmsg("Failed to allocate memory for execution state")
			));
	}
	
	node->fdw_state = (void *) festate;
	festate->login = login;
	festate->dbproc = dbproc;
	festate->query = query;
	festate->first = 1;
	festate->row = 0;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsBeginForeignScan")
			));
	#endif
}

/* get next row from foreign table */

static TupleTableSlot* tdsIterateForeignScan(ForeignScanState *node)
{
	RETCODE erc;
	int ret_code;
	HeapTuple tuple;
	TdsFdwExecutionState *festate = (TdsFdwExecutionState *) node->fdw_state;
	TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
	
	/* Cleanup */
	ExecClearTuple(slot);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsIterateForeignScan")
			));
	#endif
	
	if (festate->first)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("This is the first iteration")
				));
			ereport(NOTICE,
				(errmsg("Setting database command to %s", festate->query)
				));
		#endif
		
		festate->first = 0;
		
		if ((erc = dbcmd(festate->dbproc, festate->query)) == FAIL)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
					errmsg("Failed to set current query to %s", festate->query)
				));
		}
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Executing the query")
				));
		#endif
		
		if ((erc = dbsqlexec(festate->dbproc)) == FAIL)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
					errmsg("Failed to execute query %s", festate->query)
				));
		}

		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Query executed correctly")
				));
			ereport(NOTICE,
				(errmsg("Getting results")
				));				
		#endif

		erc = dbresults(festate->dbproc);
		
		if (erc == FAIL)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
					errmsg("Failed to get results from query %s", festate->query)
				));
		}
		
		else if (erc == NO_MORE_RESULTS)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
					errmsg("There appears to be no results from query %s", festate->query)
				));
		}
		
		else if (erc == SUCCEED)
		{
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Successfully got results")
					));
			#endif
		}
		
		else
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
					errmsg("Unknown return code getting results from query %s", festate->query)
				));
		}
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Fetching next row")
			));
	#endif
	
	if ((ret_code = dbnextrow(festate->dbproc)) != NO_MORE_ROWS)
	{
		int ncols, ncol;
		char **values;
		
		switch (ret_code)
		{
			case REG_ROW:
				festate->row++;
				
				#ifdef DEBUG
					ereport(NOTICE,
						(errmsg("Row %i fetched", festate->row)
						));
				#endif
				
				ncols = dbnumcols(festate->dbproc);
				
				#ifdef DEBUG
					ereport(NOTICE,
						(errmsg("%i columns", ncols)
						));
				#endif
				
				if ((values = palloc(ncols * sizeof(char *))) == NULL)
				{
					ereport(ERROR,
						(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
						errmsg("Failed to allocate memory for column array")
						));
				}
				
				for (ncol = 0; ncol < ncols; ncol++)
				{
					int col_type;
					char *col_name;
					
					col_name = dbcolname(festate->dbproc, ncol + 1);
					
					#ifdef DEBUG
						ereport(NOTICE,
							(errmsg("Fetching column %i (%s)", ncol, col_name)
							));
					#endif
					
					col_type = dbcoltype(festate->dbproc, ncol + 1);
					
					#ifdef DEBUG
						ereport(NOTICE,
							(errmsg("Type is %i", col_type)
							));
					#endif
					
					if (dbwillconvert(col_type, SYBCHAR) != FALSE)
					{
						DBINT data_len;
						BYTE* data;
						
						data_len = dbdatlen(festate->dbproc, ncol + 1);
						data = dbdata(festate->dbproc, ncol + 1);
						
						if (data_len == 0)
						{
							#ifdef DEBUG
								ereport(NOTICE,
									(errmsg("Column value is NULL")
									));
							#endif	
							
							values[ncol] = NULL;
						}
						
						else if (data == NULL)
						{
							#ifdef DEBUG
								ereport(NOTICE,
									(errmsg("Column value pointer is NULL, but probably shouldn't be")
									));
							#endif	
							
							values[ncol] = NULL;
						}
						
						else
						{
							char *value;
							int ret_value;
							int dest_len = 1000; /* any portable way to get real length of converted value? */
							
							if ((value = palloc(dest_len * sizeof(char))) == NULL)
							{
								ereport(ERROR,
									(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
									errmsg("Failed to allocate memory for tmp column value")
									));
							}
							
							ret_value = dbconvert(festate->dbproc, col_type, data, data_len, SYBCHAR, (BYTE *) value, -1);
							
							if (ret_value == FAIL)
							{
								#ifdef DEBUG
									ereport(NOTICE,
										(errmsg("Failed to convert value to SYBCHAR")
										));
								#endif	
							
								values[ncol] = NULL;
							}
							
							else if (ret_value == -1)
							{
								#ifdef DEBUG
									ereport(NOTICE,
										(errmsg("Failed to convert value to SYBCHAR. Could have been a NULL pointer or bad data type.")
										));
								#endif	
							
								values[ncol] = NULL;
							}
							
							else
							{
								int str_len = strlen(value);
								
								#ifdef DEBUG
									ereport(NOTICE,
										(errmsg("Column value: %s", value)
										));
								#endif
								
								if ((values[ncol] = palloc((str_len + 1) * sizeof(char))) == NULL)
								{
									ereport(ERROR,
										(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
										errmsg("Failed to allocate memory for column value")
										));
								}
								
								strncpy(values[ncol], value, str_len);
								values[ncol][str_len] = '\0';
								
								#ifdef DEBUG
									ereport(NOTICE,
										(errmsg("values[%i]: %s", ncol, values[ncol])
										));
								#endif
							}
						}
					}
					
					else
					{
						#ifdef DEBUG
							ereport(NOTICE,
								(errmsg("Column cannot be converted to SYBCHAR")
								));
						#endif
						
						values[ncol] = NULL;
					}
				}
				
				#ifdef DEBUG
					ereport(NOTICE,
						(errmsg("Printing all %i values", ncols)
						));
								
					for (ncol = 0; ncol < ncols; ncol++)
					{
						if (values[ncol] != NULL)
						{
							ereport(NOTICE,
								(errmsg("values[%i]: %s", ncol, values[ncol])
								));
						}
						
						else
						{
							ereport(NOTICE,
								(errmsg("values[%i]: NULL", ncol)
								));
						}
					}
				#endif
				
				tuple = BuildTupleFromCStrings(TupleDescGetAttInMetadata(node->ss.ss_currentRelation->rd_att), values);
				ExecStoreTuple(tuple, slot, InvalidBuffer, false);
				break;
				
			case BUF_FULL:
				ereport(ERROR,
					(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
					errmsg("Buffer filled up during query")
					));
				break;
					
			case FAIL:
				ereport(ERROR,
					(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
					errmsg("Failed to get row during query")
					));
				break;
			
			default:
				ereport(ERROR,
					(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
					errmsg("Failed to get row during query. Unknown return code.")
					));
		}
	}
	
	else
	{
		ereport(NOTICE,
			(errmsg("No more rows")
			));
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsIterateForeignScan")
			));
	#endif

	return slot;
}

/* rescan foreign table */

static void tdsReScanForeignScan(ForeignScanState *node)
{
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsReScanForeignScan")
			));
	#endif
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsReScanForeignScan")
			));
	#endif
}

/* cleanup objects related to scan */

static void tdsEndForeignScan(ForeignScanState *node)
{
	TdsFdwExecutionState *festate = (TdsFdwExecutionState *) node->fdw_state;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsEndForeignScan")
			));
	#endif
	
	if (festate->query)
	{
		pfree(festate->query);
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Closing database connection")
			));
	#endif
	
	dbclose(festate->dbproc);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Freeing login structure")
			));
	#endif	
	
	dbloginfree(festate->login);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Closing DB-Library")
			));
	#endif
	
	dbexit();
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsEndForeignScan")
			));
	#endif
}

/* routines for 9.2.0+ */
#if (PG_VERSION_NUM >= 90200)

static void tdsGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
	char *svr_address = NULL;
	int svr_port = 0;
	char *svr_username = NULL;
	char *svr_password = NULL;
	char *svr_database = NULL;
	char *svr_query = NULL;
	char *svr_table = NULL;
	LOGINREC *login;
	DBPROCESS *dbproc;
	RETCODE erc;
	int ret_code;
	char *conn_string;
	char *query;
	char err_str[1000];
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetForeignRelSize")
			));
	#endif
	
	tdsGetOptions(foreigntableid, &svr_address, 
		&svr_port, &svr_username, &svr_password, &svr_database, &svr_query, &svr_table);
		
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Initiating DB-Library")
			));
	#endif
	
	if (dbinit() == FAIL)
	{
		sprintf(err_str, "Failed to initialize DB-Library environment");
		goto cleanup_before_init;
	}
	
	dberrhandle(tds_err_handler);
	dbmsghandle(tds_msg_handler);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Getting login structure")
			));
	#endif
	
	if ((login = dblogin()) == NULL)
	{
		sprintf(err_str, "Failed to initialize DB-Library login structure");
		goto cleanup_before_login;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting login user to %s", svr_username)
			));
	#endif
	
	DBSETLUSER(login, svr_username);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting login password to %s", svr_password)
			));
	#endif
	
	DBSETLPWD(login, svr_password);	
	
	if ((conn_string = palloc((strlen(svr_address) + 10) * sizeof(char))) == NULL)
	{
		sprintf(err_str, "Failed to allocate memory for connection string");
		goto cleanup_before_open;
	}
	
	if (svr_port)
	{
		sprintf(conn_string, "%s:%i", svr_address, svr_port);
	}
	
	else
	{
		sprintf(conn_string, "%s", svr_address);
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Connection string is %s", conn_string)
			));
		ereport(NOTICE,
			(errmsg("Connecting to server")
			));
	#endif
	
	if ((dbproc = dbopen(login, conn_string)) == NULL)
	{
		pfree(conn_string);
		sprintf(err_str, "Failed to connect using connection string %s with user %s", conn_string, svr_username);
		goto cleanup_before_open;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Connected successfully")
			));
	#endif
	
	pfree(conn_string);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Selecting database %s", svr_database)
			));
	#endif
	
	if ((erc = dbuse(dbproc, svr_database)) == FAIL)
	{
		sprintf(err_str, "Failed to select database %s", svr_database);
		goto cleanup_before_use;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Selected database")
			));
		ereport(NOTICE,
			(errmsg("Getting query")
			));
	#endif
	
	if (svr_query)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Query is explicitly set")
				));
		#endif
		
		query = svr_query;
	}
	
	else
	{
		size_t len;
		static const char *query_prefix = "SELECT * FROM ";
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Building query using table")
				));
		#endif
		
		len = strlen(query_prefix) + strlen(svr_table) + 1;
		
		if ((query = palloc(len * sizeof(char))) == NULL)
		{
			sprintf(err_str, "Failed to allocate memory for query");
			goto cleanup;
		}
		
		if (snprintf(query, len, "%s%s", query_prefix, svr_table) < 0)
		{
			sprintf(err_str, "Failed to build query");
			goto cleanup;
		}
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Value of query is %s", query)
			));
		ereport(NOTICE,
			(errmsg("Setting database command to %s", query)
			));
	#endif
	
	if ((erc = dbcmd(dbproc, query)) == FAIL)
	{
		pfree(query);
		sprintf(err_str, "Failed to set current query to %s", query);
		goto cleanup;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Executing the query")
			));
	#endif
	
	if ((erc = dbsqlexec(dbproc)) == FAIL)
	{
		pfree(query);
		sprintf(err_str, "Failed to execute query %s", query);
		goto cleanup;
	}

	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Query executed correctly")
			));
		ereport(NOTICE,
			(errmsg("Getting results")
			));				
	#endif
	
	pfree(query);

	erc = dbresults(dbproc);
	
	if (erc == FAIL)
	{
		sprintf(err_str, "Failed to get results from query %s", query);
		goto cleanup;
	}
	
	else if (erc == NO_MORE_RESULTS)
	{
		sprintf(err_str, "There appears to be no results from query %s", query);
		goto cleanup;
	}
	
	else if (erc == SUCCEED)
	{
		int rows = 0;
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Successfully got results")
				));
		#endif
		
		baserel->rows = 0;
		
		if ((ret_code = dbnextrow(dbproc)) != NO_MORE_ROWS)
		{
			switch (ret_code)
			{
				case REG_ROW:
					rows++;
					break;
				case BUF_FULL:
					sprintf(err_str, "Buffer filled up getting plan for query");
					goto cleanup;
					break;
						
				case FAIL:
					sprintf(err_str, "Failed to get row getting plan for query");
					goto cleanup;
					break;
				
				default:
					sprintf(err_str, "Failed to get plan for query. Unknown return code.");
					goto cleanup;
			}
		}
		
		baserel->rows = DBCOUNT(dbproc);
		baserel->tuples = baserel->rows;
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("We counted %i rows, and dbcount says %i rows", rows, (int) baserel->rows)
				));
		#endif		
	}
	
	else
	{
		sprintf(err_str, "Unknown return code getting results from query %s", query);
		goto cleanup;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetForeignRelSize")
			));
	#endif
	
cleanup:
	
cleanup_before_use:
	dbclose(dbproc);
	
cleanup_before_open:
	dbloginfree(login);
		
cleanup_before_login:
	dbexit();
	
cleanup_before_init:
	;
}

static void tdsEstimateCosts(PlannerInfo *root, RelOptInfo *baserel, Cost *startup_cost, Cost *total_cost, Oid foreigntableid)
{
	char *svr_address = NULL;
	int svr_port = 0;
	char *svr_username = NULL;
	char *svr_password = NULL;
	char *svr_database = NULL;
	char *svr_query = NULL;
	char *svr_table = NULL;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsEstimateCosts")
			));
	#endif
	
	tdsGetOptions(foreigntableid, &svr_address, 
		&svr_port, &svr_username, &svr_password, &svr_database, &svr_query, &svr_table);	
	
	
	if (strcmp(svr_address, "127.0.0.1") == 0 || strcmp(svr_address, "localhost") == 0)
		*startup_cost = 0;
	else
		*startup_cost = 25;
		
	*total_cost = baserel->rows + *startup_cost;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsEstimateCosts")
			));
	#endif
}

static void tdsGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
	Cost startup_cost;
	Cost total_cost;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetForeignPaths")
			));
	#endif
	
	tdsEstimateCosts(root, baserel, &startup_cost, &total_cost, foreigntableid);
	
	add_path(baserel, 
		(Path *) create_foreignscan_path(root, baserel, baserel->rows, startup_cost, total_cost,
			NIL, NULL, NIL));
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetForeignPaths")
			));
	#endif
}

static bool tdsAnalyzeForeignTable(Relation relation, AcquireSampleRowsFunc *func, BlockNumber *totalpages)
{
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsAnalyzeForeignTable")
			));
	#endif
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsAnalyzeForeignTable")
			));
	#endif
	
	return false;
}

static ForeignScan* tdsGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, 
	Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses)
{
	Index scan_relid = baserel->relid;
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetForeignPlan")
			));
	#endif
	
	scan_clauses = extract_actual_clauses(scan_clauses, false);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetForeignPlan")
			));
	#endif
	
	return make_foreignscan(tlist, scan_clauses, scan_relid, NIL, NIL);
}

/* routines for versions older than 9.2.0 */
#else

static FdwPlan* tdsPlanForeignScan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel)
{
	FdwPlan *fdwplan;
	char *svr_address = NULL;
	int svr_port = 0;
	char *svr_username = NULL;
	char *svr_password = NULL;
	char *svr_database = NULL;
	char *svr_query = NULL;
	char *svr_table = NULL;
	LOGINREC *login;
	DBPROCESS *dbproc;
	RETCODE erc;
	char *conn_string;
	char *query;
	char error_str[1000];
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsPlanForeignScan")
			));
	#endif
	
	fdwplan = makeNode(FdwPlan);
	
	tdsGetOptions(foreigntableid, &svr_address, 
		&svr_port, &svr_username, &svr_password, &svr_database, &svr_query, %svr_table);	
	
	
	if (strcmp(svr_address, "127.0.0.1") == 0 || strcmp(svr_address, "localhost") == 0)
		fdwplan->startup_cost = 0;
	else
		fdwplan->startup_cost = 25;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Initiating DB-Library")
			));
	#endif
	
	if (dbinit() == FAIL)
	{
		sprintf(err_str, "Failed to initialize DB-Library environment");
		goto cleanup_before_init;
	}
	
	dberrhandle(tds_err_handler);
	dbmsghandle(tds_msg_handler);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Getting login structure")
			));
	#endif
	
	if ((login = dblogin()) == NULL)
	{
		sprintf(err_str, "Failed to initialize DB-Library login structure");
		goto cleanup_before_login;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting login user to %s", svr_username)
			));
	#endif
	
	DBSETLUSER(login, svr_username);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting login password to %s", svr_password)
			));
	#endif
	
	DBSETLPWD(login, svr_password);	
	
	if ((conn_string = palloc((strlen(svr_address) + 10) * sizeof(char))) == NULL)
	{
		sprintf(err_str, "Failed to allocate memory for connection string");
		goto cleanup_before_open;
	}
	
	if (svr_port)
	{
		sprintf(conn_string, "%s:%i", svr_address, svr_port);
	}
	
	else
	{
		sprintf(conn_string, "%s", svr_address);
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Connection string is %s", conn_string)
			));
		ereport(NOTICE,
			(errmsg("Connecting to server")
			));
	#endif
	
	if ((dbproc = dbopen(login, conn_string)) == NULL)
	{
		pfree(conn_string);
		sprintf(err_str, "Failed to connect using connection string %s with user %s", conn_string, svr_username);
		goto cleanup_before_open;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Connected successfully")
			));
	#endif
	
	pfree(conn_string);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Selecting database %s", svr_database)
			));
	#endif
	
	if ((erc = dbuse(dbproc, svr_database)) == FAIL)
	{
		sprintf(err_str, "Failed to select database %s", svr_database);
		goto cleanup_before_use;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Selected database")
			));
		ereport(NOTICE,
			(errmsg("Getting query")
			));
	#endif
	
	if (svr_query)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Query is explicitly set")
				));
		#endif
		
		query = svr_query;
	}
	
	else
	{
		size_t len;
		static const char *query_prefix = "SELECT * FROM ";
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Building query using table")
				));
		#endif
		
		len = strlen(query_prefix) + strlen(svr_table) + 1;
		
		if ((query = palloc(len * sizeof(char))) == NULL)
		{
			sprintf(err_str, "Failed to allocate memory for query");
			goto cleanup;
		}
		
		if (snprintf(query, len, "%s%s", query_prefix, svr_table) < 0)
		{
			sprintf(err_str, "Failed to build query");
			goto cleanup;
		}
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Value of query is %s", query)
			));
		ereport(NOTICE,
			(errmsg("Setting database command to %s", query)
			));
	#endif
	
	if ((erc = dbcmd(dbproc, query)) == FAIL)
	{
		pfree(query);
		sprintf(err_str, "Failed to set current query to %s", query);
		goto cleanup;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Executing the query")
			));
	#endif
	
	if ((erc = dbsqlexec(dbproc)) == FAIL)
	{
		pfree(query);
		sprintf(err_str, "Failed to execute query %s", query);
		goto cleanup;
	}

	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Query executed correctly")
			));
		ereport(NOTICE,
			(errmsg("Getting results")
			));				
	#endif
	
	pfree(query);

	erc = dbresults(dbproc);
	
	if (erc == FAIL)
	{
		sprintf(err_str, "Failed to get results from query %s", query);
		goto cleanup;
	}
	
	else if (erc == NO_MORE_RESULTS)
	{
		sprintf(err_str, "There appears to be no results from query %s", query);
		goto cleanup;
	}
	
	else if (erc == SUCCEED)
	{
		int rows = 0;
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Successfully got results")
				));
		#endif
		
		baserel->rows = 0;
		
		if ((ret_code = dbnextrow(festate->dbproc)) != NO_MORE_ROWS)
		{
			int ncols, ncol;
			char **values;
		
			switch (ret_code)
			{
				case REG_ROW:
					row++;
					break;
				case BUF_FULL:
					sprintf(err_str, "Buffer filled up getting plan for query");
					goto cleanup;
					break;
						
				case FAIL:
					sprintf(err_str, "Failed to get row getting plan for query");
					goto cleanup;
					break;
				
				default:
					sprintf(err_str, "Failed to get plan for query. Unknown return code.");
					goto cleanup;
			}
		}
		
		baserel->rows = DBCOUNT(dbproc);
		baserel->tuples = baserel->rows;
		fdwplan->total_cost = baserel->rows + fdwplan->startup_cost;
		fdwplan->fdw_private = NIL;
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("We counted %i rows, and dbcount says %i rows", row, (int) baserel->rows);
				));
		#endif		
	}
	
	else
	{
		sprintf(err_str, "Unknown return code getting results from query %s", query);
		goto cleanup;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			errmsg("----> finishing tdsPlanForeignScan")
			));
	#endif
	
cleanup:
	
cleanup_before_use:
	dbclose(dbproc);
	
cleanup_before_open:
	dbloginfree(login);
		
cleanup_before_login:
	dbexit();
	
cleanup_before_init:
	;
	
	ereport(ERROR,
		(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
		errmsg(err_str)
		));	
}

#endif

int tds_err_handler(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tds_err_handler")
			));
	#endif
	
	ereport(ERROR,
		(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
		errmsg("DB-Library error: DB #: %i, DB Msg: %s, OS #: %i, OS Msg: %s, Level: %i",
			dberr, dberrstr, oserr, oserrstr, severity)
		));	
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tds_err_handler")
			));
	#endif

	return INT_CANCEL;
}

int tds_msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *svr_name, char *proc_name, int line)
{
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tds_msg_handler")
			));
	#endif
	
	ereport(NOTICE,
		(errmsg("DB-Library notice: Msg #: %ld, Msg state: %i, Msg: %s, Server: %s, Process: %s, Line: %i, Level: %i",
			(long)msgno, msgstate, msgtext, svr_name, proc_name, line, severity)
		));		
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tds_msg_handler")
			));
	#endif

	return 0;
}