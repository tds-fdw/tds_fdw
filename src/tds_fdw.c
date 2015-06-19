/*------------------------------------------------------------------
*
*				Foreign data wrapper for TDS (Sybase and Microsoft SQL Server)
*
* Author: Geoff Montee
* Name: tds_fdw
* File: tds_fdw/src/tds_fdw.c
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



#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* postgres headers */

#include "postgres.h"
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
#include "utils/memutils.h"

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

#include "tds_fdw.h"
#include "options.h"

PG_FUNCTION_INFO_V1(tds_fdw_handler);
PG_FUNCTION_INFO_V1(tds_fdw_validator);

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
	TdsFdwOptionSet option_set;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tds_fdw_validator")
			));
	#endif
	
	tdsValidateOptions(options_list, catalog, &option_set);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tds_fdw_validator")
			));
	#endif
	
	PG_RETURN_VOID();
}

/* set up connection */

int tdsSetupConnection(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS **dbproc)
{
	char* conn_string;
	RETCODE erc;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsSetupConnection")
			));
	#endif		
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting login user to %s", option_set->username)
			));
	#endif
	
	DBSETLUSER(login, option_set->username);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting login password to %s", option_set->password)
			));
	#endif
	
	DBSETLPWD(login, option_set->password);	
	
	if (option_set->character_set)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Setting login character set to %s", option_set->character_set)
				));
		#endif
	
		DBSETLCHARSET(login, option_set->character_set);
	}
	
	if (option_set->language)
	{
		DBSETLNATLANG(login, option_set->language);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Setting login language to %s", option_set->language)
				));
		#endif
	}
	
	if (option_set->tds_version)
	{
		BYTE tds_version = DBVERSION_UNKNOWN;
		
		if (strcmp(option_set->tds_version, "4.2") == 0)
		{
			tds_version = DBVER42;
		}
		
		else if (strcmp(option_set->tds_version, "5.0") == 0)
		{
			tds_version = DBVERSION_100;
		}
		
		else if (strcmp(option_set->tds_version, "7.0") == 0)
		{
			tds_version = DBVER60;
		}
		
		#ifdef DBVERSION_71
		else if (strcmp(option_set->tds_version, "7.1") == 0)
		{
			tds_version = DBVERSION_71;
		}
		#endif
		
		#ifdef DBVERSION_72
		else if (strcmp(option_set->tds_version, "7.2") == 0)
		{
			tds_version = DBVERSION_72;
		}
		#endif

		#ifdef DBVERSION_73
		else if (strcmp(option_set->tds_version, "7.3") == 0)
		{
			tds_version = DBVERSION_73;
		}
		#endif
		
		if (tds_version == DBVERSION_UNKNOWN)
		{
			ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Unknown tds version: %s.", option_set->tds_version)
				));
		}
		
		dbsetlversion(login, tds_version);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Setting login tds version tp  %s", option_set->tds_version)
				));
		#endif
	}
	
	if (option_set->database && !option_set->dbuse)
	{
		DBSETLDBNAME(login, option_set->database);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Setting login database to %s", option_set->database)
				));
		#endif	
	}
	
	if ((conn_string = palloc((strlen(option_set->servername) + 10) * sizeof(char))) == NULL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
				errmsg("Failed to allocate memory for connection string")
			));
			
		return -1;
	}
	
	if (option_set->port)
	{
		sprintf(conn_string, "%s:%i", option_set->servername, option_set->port);
	}
	
	else
	{
		sprintf(conn_string, "%s", option_set->servername);
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Connection string is %s", conn_string)
			));
		ereport(NOTICE,
			(errmsg("Connecting to server")
			));
	#endif
	
	if ((*dbproc = dbopen(login, conn_string)) == NULL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
				errmsg("Failed to connect using connection string %s with user %s", conn_string, option_set->username)
			));
			
		return -1;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Connected successfully")
			));
	#endif
	
	pfree(conn_string);
	
	if (option_set->database && option_set->dbuse)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Selecting database %s", option_set->database)
				));
		#endif
		
		if ((erc = dbuse(*dbproc, option_set->database)) == FAIL)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
					errmsg("Failed to select database %s", option_set->database)
				));
				
			return -1;
		}
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Selected database")
				));
		#endif
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Getting query")
			));
	#endif
	
	if (option_set->query)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Query is explicitly set")
				));
		#endif
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
		
		len = strlen(query_prefix) + strlen(option_set->table) + 1;
		
		if ((option_set->query = palloc(len * sizeof(char))) == NULL)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
					errmsg("Failed to allocate memory for query")
				));
				
			return -1;
		}
		
		if (snprintf(option_set->query, len, "%s%s", query_prefix, option_set->table) < 0)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
					errmsg("Failed to build query")
				));
				
			return -1;
		}
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Value of query is %s", option_set->query)
			));
	#endif
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsSetupConnection")
			));
	#endif	
	
	return 0;
}

int tdsGetRowCountShowPlanAll(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc)
{
	int rows = 0;
	RETCODE erc;
	int ret_code;
	char* show_plan_query = "SET SHOWPLAN_ALL ON";
	char* show_plan_query_off = "SET SHOWPLAN_ALL OFF";
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetRowCountShowPlanAll")
			));
	#endif	

	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting database command to %s", show_plan_query)
			));
	#endif
	
	if ((erc = dbcmd(dbproc, show_plan_query)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to set current query to %s", show_plan_query)
			));		

		goto cleanup;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Executing the query")
			));
	#endif
	
	if ((erc = dbsqlexec(dbproc)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to execute query %s", show_plan_query)
			));	

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
	
	erc = dbresults(dbproc);
	
	if (erc == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to get results from query %s", show_plan_query)
			));
			
		goto cleanup_after_show_plan;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting database command to %s", option_set->query)
			));
	#endif
	
	if ((erc = dbcmd(dbproc, option_set->query)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to set current query to %s", option_set->query)
			));		

		goto cleanup_after_show_plan;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Executing the query")
			));
	#endif
	
	if ((erc = dbsqlexec(dbproc)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to execute query %s", option_set->query)
			));	

		goto cleanup_after_show_plan;
	}

	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Query executed correctly")
			));
		ereport(NOTICE,
			(errmsg("Getting results")
			));				
	#endif

	erc = dbresults(dbproc);
	
	if (erc == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to get results from query %s", option_set->query)
			));
			
		goto cleanup_after_show_plan;
	}
	
	else if (erc == NO_MORE_RESULTS)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("There appears to be no results from query %s", option_set->query)
				));
		#endif
		
		goto cleanup_after_show_plan;
	}
	
	else if (erc == SUCCEED)
	{
		int ncol;
		int ncols;
		int parent = 0;
		int estimate_rows = 0;
		
		ncols = dbnumcols(dbproc);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("%i columns", ncols)
				));
		#endif
		
		for (ncol = 0; ncol < ncols; ncol++)
		{
			char *col_name;

			col_name = dbcolname(dbproc, ncol + 1);
			
			if (strcmp(col_name, "Parent") == 0)
			{
				#ifdef DEBUG
					ereport(NOTICE,
						(errmsg("Binding column %s (%i)", col_name, ncol + 1)
						));
				#endif
				
				erc = dbbind(dbproc, ncol + 1, INTBIND, sizeof(int), (BYTE *)&parent);
				
				if (erc == FAIL)
				{
					ereport(ERROR,
						(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
							errmsg("Failed to bind results for column %s to a variable.", col_name)
						));
						
					goto cleanup_after_show_plan;
				}
			}
			
			if (strcmp(col_name, "EstimateRows") == 0)
			{
				#ifdef DEBUG
					ereport(NOTICE,
						(errmsg("Binding column %s (%i)", col_name, ncol + 1)
						));
				#endif
				
				erc = dbbind(dbproc, ncol + 1, INTBIND, sizeof(int), (BYTE *)&estimate_rows);
				
				if (erc == FAIL)
				{
					ereport(ERROR,
						(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
							errmsg("Failed to bind results for column %s to a variable.", col_name)
						));
						
					goto cleanup_after_show_plan;
				}				
			}
		}
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Successfully got results")
				));
		#endif
		
		while ((ret_code = dbnextrow(dbproc)) != NO_MORE_ROWS)
		{
			switch (ret_code)
			{
				case REG_ROW:

					#ifdef DEBUG
						ereport(NOTICE,
							(errmsg("Parent is %i. EstimateRows is %i.", parent, estimate_rows)
						));
					#endif

					if (parent == 0)
					{
						rows += estimate_rows;
					}
						
					break;
					
				case BUF_FULL:
					ereport(ERROR,
						(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
							errmsg("Buffer filled up while getting plan for query")
						));					

					goto cleanup_after_show_plan;
						
				case FAIL:
					ereport(ERROR,
						(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
							errmsg("Failed to get row while getting plan for query")
						));				

					goto cleanup_after_show_plan;
				
				default:
					ereport(ERROR,
						(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
							errmsg("Failed to get plan for query. Unknown return code.")
						));					

					goto cleanup_after_show_plan;
			}
		}
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("We estimated %i rows.", rows)
				));
		#endif		
	}
	
	else
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Unknown return code getting results from query %s", option_set->query)
			));		
	}
	
cleanup_after_show_plan:
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting database command to %s", show_plan_query_off)
			));
	#endif
	
	if ((erc = dbcmd(dbproc, show_plan_query_off)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to set current query to %s", show_plan_query_off)
			));		

		goto cleanup;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Executing the query")
			));
	#endif
	
	if ((erc = dbsqlexec(dbproc)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to execute query %s", show_plan_query_off)
			));	

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
	
	erc = dbresults(dbproc);
	
	if (erc == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to get results from query %s", show_plan_query)
			));
			
		goto cleanup_after_show_plan;
	}	

cleanup:
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetRowCountShowPlanAll")
			));
	#endif		
	

	return rows;
}

/* get the number of rows returned by a query */

int tdsGetRowCountExecute(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc)
{
	int rows_report = 0;
	int rows_increment = 0;
	RETCODE erc;
	int ret_code;
	int iscount = 0;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetRowCountExecute")
			));
	#endif		
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Setting database command to %s", option_set->query)
			));
	#endif
	
	if ((erc = dbcmd(dbproc, option_set->query)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to set current query to %s", option_set->query)
			));		

		goto cleanup;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Executing the query")
			));
	#endif
	
	if ((erc = dbsqlexec(dbproc)) == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to execute query %s", option_set->query)
			));	

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

	erc = dbresults(dbproc);
	
	if (erc == FAIL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Failed to get results from query %s", option_set->query)
			));
			
		goto cleanup;
	}
	
	else if (erc == NO_MORE_RESULTS)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("There appears to be no results from query %s", option_set->query)
				));
		#endif
		
		goto cleanup;
	}
	
	else if (erc == SUCCEED)
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Successfully got results")
				));
		#endif
		
		while ((ret_code = dbnextrow(dbproc)) != NO_MORE_ROWS)
		{
			switch (ret_code)
			{
				case REG_ROW:
					rows_increment++;
					break;
					
				case BUF_FULL:
					ereport(ERROR,
						(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
							errmsg("Buffer filled up while getting plan for query")
						));					

					goto cleanup;
						
				case FAIL:
					ereport(ERROR,
						(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
							errmsg("Failed to get row while getting plan for query")
						));				

					goto cleanup;
				
				default:
					ereport(ERROR,
						(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
							errmsg("Failed to get plan for query. Unknown return code.")
						));					

					goto cleanup;
			}
		}
		
		rows_report = DBCOUNT(dbproc);
		iscount = dbiscount(dbproc);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("We counted %i rows, and dbcount says %i rows.", rows_increment, rows_report)
				));
			ereport(NOTICE,
				(errmsg("dbiscount says %i.", iscount)
				));
		#endif		
	}
	
	else
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				errmsg("Unknown return code getting results from query %s", option_set->query)
			));		
	}
	
cleanup:	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetRowCountExecute")
			));
	#endif		
	
	if (iscount)
	{
		return rows_report;
	}
	
	else
	{
		return rows_increment;
	}
}

int tdsGetRowCount(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc)
{
	int rows = 0;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetRowCount")
			));
	#endif		
	
	if (strcmp(option_set->row_estimate_method, "execute") == 0)
	{
		rows = tdsGetRowCountExecute(option_set, login, dbproc);
	}
	
	else if (strcmp(option_set->row_estimate_method, "showplan_all") == 0)
	{
		rows = tdsGetRowCountShowPlanAll(option_set, login, dbproc);
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetRowCount")
			));
	#endif	

	return rows;
}

/* get the startup cost for the query */

int tdsGetStartupCost(TdsFdwOptionSet* option_set)
{
	int startup_cost;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetStartupCost")
			));
	#endif	
	
	if (strcmp(option_set->servername, "127.0.0.1") == 0 || strcmp(option_set->servername, "localhost") == 0)
		startup_cost = 0;
	else
		startup_cost = 25;
		
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetStartupCost")
			));
	#endif		
		
	return startup_cost;
}

char* tdsConvertToCString(DBPROCESS* dbproc, int srctype, const BYTE* src, DBINT srclen)
{
	char* dest = NULL;
	int real_destlen;
	DBINT destlen;
	int desttype;
	int ret_value;
	
	switch(srctype)
	{
		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			real_destlen = srclen + 1; /* the size of the array */
			destlen = -1; /* the size to pass to dbconvert (-1 means to null terminate it) */
			desttype = SYBCHAR;
			break;
		case SYBBINARY:
		case SYBVARBINARY:
			real_destlen = srclen;
			destlen = srclen;
			desttype = SYBBINARY;
			break;
		default:
			real_destlen = 1000; /* Probably big enough */
			destlen = -1; 
			desttype = SYBCHAR;
			break;
	}
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("Source type is %i. Destination type is %i", srctype, desttype)
			));
		ereport(NOTICE,
			(errmsg("Source length is %i. Destination length is %i. Real destination length is %i", srclen, destlen, real_destlen)
			));
	#endif	
	
	if (dbwillconvert(srctype, desttype) != FALSE)
	{
		if ((dest = palloc(real_destlen * sizeof(char))) == NULL)
		{
			ereport(ERROR,
				(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
				errmsg("Failed to allocate memory for column value")
				));
		}
		
		ret_value = dbconvert(dbproc, srctype, src, srclen, desttype, (BYTE *) dest, destlen);
		
		if (ret_value == FAIL)
		{
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Failed to convert column")
					));
			#endif	
		}
		
		else if (ret_value == -1)
		{
			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Failed to convert column. Could have been a NULL pointer or bad data type.")
					));
			#endif	
		}
	}
	
	else
	{
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Column cannot be converted to this type.")
				));
		#endif
	}
	
	return dest;
	
}

/* get output for EXPLAIN */

void tdsExplainForeignScan(ForeignScanState *node, ExplainState *es)
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

void tdsBeginForeignScan(ForeignScanState *node, int eflags)
{
	TdsFdwOptionSet option_set;
	LOGINREC *login;
	DBPROCESS *dbproc;
	TdsFdwExecutionState *festate;
	EState *estate = node->ss.ps.state;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsBeginForeignScan")
			));
	#endif
	
	tdsGetForeignTableOptionsFromCatalog(RelationGetRelid(node->ss.ss_currentRelation), &option_set);
		
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
	
	if (option_set.msg_handler)
	{
		if (strcmp(option_set.msg_handler, "notice") == 0)
		{
			dbmsghandle(tds_notice_msg_handler);
		}
		
		else if (strcmp(option_set.msg_handler, "blackhole") == 0)
		{
			dbmsghandle(tds_blackhole_msg_handler);
		}
		
		else
		{
			ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Unknown msg handler: %s.", option_set.msg_handler)
				));
		}
	}
	
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
	
	if (tdsSetupConnection(&option_set, login, &dbproc) != 0)
	{
		goto cleanup;
	}
	
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
	festate->query = option_set.query;
	festate->first = 1;
	festate->row = 0;
	festate->mem_cxt = AllocSetContextCreate(estate->es_query_cxt,
											   "tds_fdw data",
											   ALLOCSET_DEFAULT_MINSIZE,
											   ALLOCSET_DEFAULT_INITSIZE,
											   ALLOCSET_DEFAULT_MAXSIZE);
	
cleanup:
	;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsBeginForeignScan")
			));
	#endif
}

void tdsGetColumnMetadata(TdsFdwExecutionState *festate)
{
	int ncol;
	
	if ((festate->columns = palloc(festate->ncols * sizeof(COL))) == NULL)
	{
		ereport(ERROR,
			(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
			errmsg("Failed to allocate memory for column metadata array")
			));
	}

	for (ncol = 0; ncol < festate->ncols; ncol++)
	{	
		COL* column;
		
		column = &festate->columns[ncol];
		column->name = dbcolname(festate->dbproc, ncol + 1);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Fetching column %i (%s)", ncol, column->name)
				));
		#endif
		
		column->srctype = dbcoltype(festate->dbproc, ncol + 1);
		
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("Type is %i", column->srctype)
				));
		#endif
		
	}
}

/* get next row from foreign table */

TupleTableSlot* tdsIterateForeignScan(ForeignScanState *node)
{
	MemoryContext old_cxt;
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
	
	old_cxt = MemoryContextSwitchTo(festate->mem_cxt);
	
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

			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("Getting column info")
					));
			#endif

			festate->ncols = dbnumcols(festate->dbproc);

			#ifdef DEBUG
				ereport(NOTICE,
					(errmsg("%i columns", festate->ncols)
					));
			#endif
			
			MemoryContextReset(festate->mem_cxt);
		
			tdsGetColumnMetadata(festate);
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
		int ncol;
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
				
				if ((values = palloc(festate->ncols * sizeof(char *))) == NULL)
				{
					ereport(ERROR,
						(errcode(ERRCODE_FDW_OUT_OF_MEMORY),
						errmsg("Failed to allocate memory for column array")
						));
				}
				
				for (ncol = 0; ncol < festate->ncols; ncol++)
				{
					COL* column;
					DBINT srclen;
					BYTE* src;
					
					column = &festate->columns[ncol];
			
					srclen = dbdatlen(festate->dbproc, ncol + 1);
					
					#ifdef DEBUG
						ereport(NOTICE,
							(errmsg("Data length is %i", srclen)
							));
					#endif					
					
					src = dbdata(festate->dbproc, ncol + 1);

					if (srclen == 0 && src == NULL)
					{
						#ifdef DEBUG
							ereport(NOTICE,
								(errmsg("Column value is NULL")
								));
						#endif	
						
						values[ncol] = NULL;						
					}				
					
					else if (src == NULL)
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
						values[ncol] = tdsConvertToCString(festate->dbproc, column->srctype, src, srclen);
					}
				}
				
				#ifdef DEBUG
					ereport(NOTICE,
						(errmsg("Printing all %i values", festate->ncols)
						));
								
					for (ncol = 0; ncol < festate->ncols; ncol++)
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
		#ifdef DEBUG
			ereport(NOTICE,
				(errmsg("No more rows")
				));
		#endif
	}
	
	MemoryContextSwitchTo(old_cxt);
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsIterateForeignScan")
			));
	#endif

	return slot;
}

/* rescan foreign table */

void tdsReScanForeignScan(ForeignScanState *node)
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

void tdsEndForeignScan(ForeignScanState *node)
{
	MemoryContext old_cxt;
	TdsFdwExecutionState *festate = (TdsFdwExecutionState *) node->fdw_state;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsEndForeignScan")
			));
	#endif
	
	old_cxt = MemoryContextSwitchTo(festate->mem_cxt);

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

	MemoryContextSwitchTo(old_cxt);
	MemoryContextReset(festate->mem_cxt);
}

/* routines for 9.2.0+ */
#if (PG_VERSION_NUM >= 90200)

void tdsGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
	TdsFdwOptionSet option_set;
	LOGINREC *login;
	DBPROCESS *dbproc;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsGetForeignRelSize")
			));
	#endif
	
	tdsGetForeignTableOptionsFromCatalog(foreigntableid, &option_set);
		
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
		goto cleanup_before_init;
	}
	
	dberrhandle(tds_err_handler);
	
	if (option_set.msg_handler)
	{
		if (strcmp(option_set.msg_handler, "notice") == 0)
		{
			dbmsghandle(tds_notice_msg_handler);
		}
		
		else if (strcmp(option_set.msg_handler, "blackhole") == 0)
		{
			dbmsghandle(tds_blackhole_msg_handler);
		}
		
		else
		{
			ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Unknown msg handler: %s.", option_set.msg_handler)
				));
		}
	}
	
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
		goto cleanup_before_login;
	}
	
	if (tdsSetupConnection(&option_set, login, &dbproc) != 0)
	{
		goto cleanup;
	}
		
	baserel->rows = tdsGetRowCount(&option_set, login, dbproc);
	baserel->tuples = baserel->rows;
	
cleanup:
	dbclose(dbproc);
	dbloginfree(login);
		
cleanup_before_login:
	dbexit();
	
cleanup_before_init:
	;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsGetForeignRelSize")
			));
	#endif	
}

void tdsEstimateCosts(PlannerInfo *root, RelOptInfo *baserel, Cost *startup_cost, Cost *total_cost, Oid foreigntableid)
{
	TdsFdwOptionSet option_set;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsEstimateCosts")
			));
	#endif
	
	tdsGetForeignTableOptionsFromCatalog(foreigntableid, &option_set);	
	
	*startup_cost = tdsGetStartupCost(&option_set);
		
	*total_cost = baserel->rows + *startup_cost;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsEstimateCosts")
			));
	#endif
}

void tdsGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
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

bool tdsAnalyzeForeignTable(Relation relation, AcquireSampleRowsFunc *func, BlockNumber *totalpages)
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

ForeignScan* tdsGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, 
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

FdwPlan* tdsPlanForeignScan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel)
{
	FdwPlan *fdwplan;
	TdsFdwOptionSet option_set;
	LOGINREC *login;
	DBPROCESS *dbproc;
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tdsPlanForeignScan")
			));
	#endif
	
	fdwplan = makeNode(FdwPlan);
	
	tdsGetForeignTableOptionsFromCatalog(foreigntableid, &option_set);	
	
	fdwplan->startup_cost = tdsGetStartupCost(&option_set);
		
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
		goto cleanup_before_init;
	}
	
	dberrhandle(tds_err_handler);
	
	if (option_set.msg_handler)
	{
		if (strcmp(option_set.msg_handler, "notice") == 0)
		{
			dbmsghandle(tds_notice_msg_handler);
		}
		
		else if (strcmp(option_set.msg_handler, "blackhole") == 0)
		{
			dbmsghandle(tds_blackhole_msg_handler);
		}
		
		else
		{
			ereport(ERROR,
				(errcode(ERRCODE_SYNTAX_ERROR),
					errmsg("Unknown msg handler: %s.", option_set.msg_handler)
				));
		}
	}
	
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
		goto cleanup_before_login;
	}
	
	if (tdsSetupConnection(&option_set, login, &dbproc) != 0)
	{
		goto cleanup;
	}
		
	baserel->rows = tdsGetRowCount(&option_set, login, dbproc);
	baserel->tuples = baserel->rows;
	fdwplan->total_cost = baserel->rows + fdwplan->startup_cost;
	fdwplan->fdw_private = NIL;
	
cleanup:
	dbclose(dbproc);
	dbloginfree(login);
		
cleanup_before_login:
	dbexit();
	
cleanup_before_init:
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tdsPlanForeignScan")
			));
	#endif	
	
	return fdwplan;
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

int tds_notice_msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *svr_name, char *proc_name, int line)
{
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tds_notice_msg_handler")
			));
	#endif
	
	ereport(NOTICE,
		(errmsg("DB-Library notice: Msg #: %ld, Msg state: %i, Msg: %s, Server: %s, Process: %s, Line: %i, Level: %i",
			(long)msgno, msgstate, msgtext, svr_name, proc_name, line, severity)
		));		
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tds_notice_msg_handler")
			));
	#endif

	return 0;
}

int tds_blackhole_msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *svr_name, char *proc_name, int line)
{
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> starting tds_blackhole_msg_handler")
			));
	#endif	
	
	#ifdef DEBUG
		ereport(NOTICE,
			(errmsg("----> finishing tds_blackhole_msg_handler")
			));
	#endif

	return 0;
}
