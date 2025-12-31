/*------------------------------------------------------------------
*
*               Foreign data wrapper for TDS (Sybase and Microsoft SQL Server)
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

/* Override PGDLLEXPORT for visibility */

#include "visibility.h"

/* postgres headers */

#include "postgres.h"
#include "funcapi.h"
#include "access/reloptions.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_user_mapping.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#if PG_VERSION_NUM < 180000
#include "commands/explain.h"
#else
#include "commands/explain_format.h"
#include "commands/explain_state.h"
#endif
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
#include "mb/pg_wchar.h"
#include "libpq/pqsignal.h"
#include "optimizer/cost.h"
#include "optimizer/paths.h"
#include "optimizer/prep.h"
#if (PG_VERSION_NUM < 120000)
#include "optimizer/var.h"
#else
#include "optimizer/optimizer.h"
#endif
#include "storage/fd.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/memutils.h"
#include "utils/guc.h"
#include "utils/timestamp.h"

#if (PG_VERSION_NUM >= 90300)
#include "access/htup_details.h"
#else
#include "access/htup.h"
#endif

#include "optimizer/pathnode.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/planmain.h"

/* DB-Library headers (e.g. FreeTDS */
#include <sybfront.h>
#include <sybdb.h>

/* #define DEBUG */

PG_MODULE_MAGIC;

#include "tds_fdw.h"
#include "options.h"
#include "deparse.h"

/* run on module load */

extern PGDLLEXPORT void _PG_init(void);

static const bool DEFAULT_SHOW_FINISHED_MEMORY_STATS = false;
static bool show_finished_memory_stats = false;

static const bool DEFAULT_SHOW_BEFORE_ROW_MEMORY_STATS = false;
static bool show_before_row_memory_stats = false;

static const bool DEFAULT_SHOW_AFTER_ROW_MEMORY_STATS = false;
static bool show_after_row_memory_stats = false;

static const double DEFAULT_FDW_SORT_MULTIPLIER=1.2;

/* error handling */

static char* last_error_message = NULL;

static int tds_err_capture(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
static char *tds_err_msg(int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);

/* signal handling */
static volatile bool interrupt_flag = false;
static void tds_signal_handler(int signum);
static void tds_clear_signals(void);
static int tds_chkintr_func(void* vdbproc);
static int tds_hndlintr_func(void* vdbproc);

/* Executes server query */
static bool
tdsExecuteQuery(char *query, DBPROCESS *dbproc);

/*
 * Checks database vendor being either Microsoft or Sybase.
 * Returns 1 in case the connected instance is SQL Server.
 */
static bool tdsIsSqlServer(DBPROCESS *dbproc);

/*
 * Internal helper to set ANSI compatible server-side settings for SQL Server
 * in case foreign server was configured with sqlserver_ansi_mode 'true'.
 */
static void tdsSetSqlServerAnsiMode(DBPROCESS **dbproc);

/*
 * Indexes of FDW-private information stored in fdw_private lists.
 *
 * We store various information in ForeignScan.fdw_private to pass it from
 * planner to executor.  Currently we store:
 *
 * 1) SELECT statement text to be sent to the remote server
 * 2) Integer list of attribute numbers retrieved by the SELECT
 *
 * These items are indexed with the enum FdwScanPrivateIndex, so an item
 * can be fetched with list_nth().  For example, to get the SELECT statement:
 *      sql = strVal(list_nth(fdw_private, FdwScanPrivateSelectSql));
 */
enum FdwScanPrivateIndex
{
    /* SQL statement to execute remotely (as a String node) */
    FdwScanPrivateSelectSql,
    /* Integer list of attribute numbers retrieved by the SELECT */
    FdwScanPrivateRetrievedAttrs
};

PG_FUNCTION_INFO_V1(tds_fdw_handler);
PG_FUNCTION_INFO_V1(tds_fdw_validator);

PGDLLEXPORT Datum tds_fdw_handler(PG_FUNCTION_ARGS)
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

#ifdef IMPORT_API
    fdwroutine->ImportForeignSchema = tdsImportForeignSchema;
#endif  /* IMPORT_API */

    pqsignal(SIGINT, tds_signal_handler);

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tds_fdw_handler")
            ));
    #endif
    
    PG_RETURN_POINTER(fdwroutine);
}

PGDLLEXPORT Datum tds_fdw_validator(PG_FUNCTION_ARGS)
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

void _PG_init(void)
{
    DefineCustomBoolVariable("tds_fdw.show_finished_memory_stats",
        "Show finished memory stats",
        "Set to true to show memory stats after a query finishes",
        &show_finished_memory_stats,
        DEFAULT_SHOW_FINISHED_MEMORY_STATS,
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL);
        
    DefineCustomBoolVariable("tds_fdw.show_before_row_memory_stats",
        "Show before row memory stats",
        "Set to true to show memory stats before fetching each row",
        &show_before_row_memory_stats,
        DEFAULT_SHOW_BEFORE_ROW_MEMORY_STATS,
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL);
        
    DefineCustomBoolVariable("tds_fdw.show_after_row_memory_stats",
        "Show after row memory stats",
        "Set to true to show memory stats after fetching each row",
        &show_after_row_memory_stats,
        DEFAULT_SHOW_AFTER_ROW_MEMORY_STATS,
        PGC_SUSET,
        0,
        NULL,
        NULL,
        NULL);
}

/*
 * Find an equivalence class member expression, all of whose Vars, come from
 * the indicated relation.
 */
Expr * find_em_expr_for_rel(EquivalenceClass *ec, RelOptInfo *rel)
{
    ListCell   *lc_em;

    foreach(lc_em, ec->ec_members)
    {
        EquivalenceMember *em = lfirst(lc_em);

        if (bms_equal(em->em_relids, rel->relids))
        {
            /*
             * If there is more than one equivalence member whose Vars are
             * taken entirely from this relation, we'll be content to choose
             * any one of those.
             */
            return em->em_expr;
        }
    }

    /* We didn't find any suitable equivalence class expression */
    return NULL;
}

/* This is used for JOIN pushdowns, so it is only needed on 9.5+ */
#if (PG_VERSION_NUM >= 90500)
/*
 * Detect whether we want to process an EquivalenceClass member.
 *
 * This is a callback for use by generate_implied_equalities_for_column.
 */
static bool
ec_member_matches_foreign(PlannerInfo *root, RelOptInfo *rel,
                          EquivalenceClass *ec, EquivalenceMember *em,
                          void *arg)
{
    ec_member_foreign_arg *state = (ec_member_foreign_arg *) arg;
    Expr       *expr = em->em_expr;

    /*
     * If we've identified what we're processing in the current scan, we only
     * want to match that expression.
     */
    if (state->current != NULL)
        return equal(expr, state->current);

    /*
     * Otherwise, ignore anything we've already processed.
     */
    if (list_member(state->already_used, expr))
        return false;

    /* This is the new target to process. */
    state->current = expr;
    return true;
}
#endif

/* build query that gets sent to remote server */

void tdsBuildForeignQuery(PlannerInfo *root, RelOptInfo *baserel, TdsFdwOptionSet* option_set, 
    Bitmapset* attrs_used, List** retrieved_attrs, List* remote_conds, List* remote_join_conds, 
    List* pathkeys)
{
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsBuildForeignQuery")
            ));
    #endif  
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Getting query")
        ));
    
    if (option_set->query)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: Query is explicitly set")
            ));
        
        if (option_set->match_column_names)
        {
            /* do this, so that retrieved_attrs is filled in */
            
            StringInfoData sql; 

            initStringInfo(&sql);
            deparseSelectSql(&sql, root, baserel, attrs_used,
                             retrieved_attrs, option_set);
        }
    }
    
    else
    {
        StringInfoData sql; 

        initStringInfo(&sql);
        deparseSelectSql(&sql, root, baserel, attrs_used,
                         retrieved_attrs, option_set);
        if (remote_conds)
            appendWhereClause(&sql, root, baserel, remote_conds,
                              true, NULL);
        if (remote_join_conds)
            appendWhereClause(&sql, root, baserel, remote_join_conds,
                              (remote_conds == NIL), NULL);

        if (pathkeys)
            appendOrderByClause(&sql, root, baserel, pathkeys);
        
        /*
         * Add FOR UPDATE/SHARE if appropriate.  We apply locking during the
         * initial row fetch, rather than later on as is done for local tables.
         * The extra roundtrips involved in trying to duplicate the local
         * semantics exactly don't seem worthwhile (see also comments for
         * RowMarkType).
         *
         * Note: because we actually run the query as a cursor, this assumes that
         * DECLARE CURSOR ... FOR UPDATE is supported, which it isn't before 8.3.
         */
        if (baserel->relid == root->parse->resultRelation &&
            (root->parse->commandType == CMD_UPDATE ||
             root->parse->commandType == CMD_DELETE))
        {
            /* Relation is UPDATE/DELETE target, so use FOR UPDATE */
            appendStringInfoString(&sql, " FOR UPDATE");
        }
        #if (PG_VERSION_NUM >= 90500)
        else
        {
            PlanRowMark *rc = get_plan_rowmark(root->rowMarks, baserel->relid);

            if (rc)
            {
                /*
                 * Relation is specified as a FOR UPDATE/SHARE target, so handle
                 * that.  (But we could also see LCS_NONE, meaning this isn't a
                 * target relation after all.)
                 *
                 * For now, just ignore any [NO] KEY specification, since (a) it's
                 * not clear what that means for a remote table that we don't have
                 * complete information about, and (b) it wouldn't work anyway on
                 * older remote servers.  Likewise, we don't worry about NOWAIT.
                 */
                switch (rc->strength)
                {
                    case LCS_NONE:
                        /* No locking needed */
                        break;
                    case LCS_FORKEYSHARE:
                    case LCS_FORSHARE:
                        appendStringInfoString(&sql, " FOR SHARE");
                        break;
                    case LCS_FORNOKEYUPDATE:
                    case LCS_FORUPDATE:
                        appendStringInfoString(&sql, " FOR UPDATE");
                        break;
                }
            }
        }
        #endif      
        
        /* now copy it to option_set->query */

        if ((option_set->query = palloc((sql.len + 1) * sizeof(char))) == NULL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                errmsg("Failed to allocate memory for query")
                ));
        }
        
        strcpy(option_set->query, sql.data);
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Value of query is %s", option_set->query)
        ));
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsBuildForeignQuery")
            ));
    #endif  
}

/* helper function, check database vendor is Microsoft or not */
bool tdsIsSqlServer(DBPROCESS *dbproc)
{
    char *check_vendor_query = "SELECT CHARINDEX('Microsoft', @@version) AS is_sql_server";
    bool result = true;

    if (!tdsExecuteQuery(check_vendor_query, dbproc))
        ereport(ERROR,
            (errcode(ERRCODE_FDW_ERROR),
                errmsg("Failed to check server version")
            ));
    else
    {
        RETCODE     erc;
        int         ret_code,
                    is_sql_server_pos;

        erc = dbbind(dbproc, 1, INTBIND, sizeof(int), (BYTE *) &is_sql_server_pos);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"is_sql_server\" to a variable.")
                ));
        }

        /* Process result */
        ret_code = dbnextrow(dbproc);
        if (ret_code == NO_MORE_ROWS)
            ereport(ERROR,
                (errcode(ERRCODE_FDW_ERROR),
                    errmsg("Failed to check server version")
                ));

        switch (ret_code)
        {
            case REG_ROW:
                ereport(DEBUG3,
                    (errmsg("tds_fdw: is_sql_server %d", is_sql_server_pos)
                    ));

                if (is_sql_server_pos == 0)
                    result = false;

                break;

            case BUF_FULL:
                ereport(ERROR,
                    (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                        errmsg("Buffer filled up while getting plan for query")
                    ));
                break;

            case FAIL:
                ereport(ERROR,
                    (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                        errmsg("Failed to get row while getting plan for query")
                    ));
                break;

            default:
                ereport(ERROR,
                    (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                        errmsg("Failed to get plan for query. Unknown return code.")
                    ));
        }
    }

    return result;
}

/* helper function to set ANSI options */
void tdsSetSqlServerAnsiMode(DBPROCESS **dbproc)
{
    char *set_ansi_options_query = "SET CONCAT_NULL_YIELDS_NULL, "
        "ANSI_NULLS, "
        "ANSI_WARNINGS, "
        "QUOTED_IDENTIFIER, "
        "ANSI_PADDING, "
        "ANSI_NULL_DFLT_ON ON";
    RETCODE erc;

    elog(DEBUG3, "tds_fdw: checking for SQL Server");

    if (!tdsIsSqlServer(*dbproc))
    {
        ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                 errmsg("tds_fdw: option sqlserver_ansi_mode only supported for SQL Server"),
                 errdetail("The foreign server is configured with sqlserver_ansi_mode=true"),
                 errhint("use ALTER SERVER ... OPTIONS(DROP sqlserver_ansi_mode)")));
    }

    elog(DEBUG3, "tds_fdw: enabling ansi settings");

    if ((erc = dbcmd(*dbproc, set_ansi_options_query)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to set %s", set_ansi_options_query)
            ));
    }

    ereport(DEBUG3,
            (errmsg("tds_fdw: Executing the query \"%s\"", set_ansi_options_query)));

    if ((erc = dbsqlexec(*dbproc)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to execute query %s", set_ansi_options_query)
            ));
    }

}

/* set up connection */

int tdsSetupConnection(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS **dbproc)
{
    char *servers;
    RETCODE erc;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsSetupConnection")
            ));
    #endif      
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Setting login user to %s", option_set->username)
        ));
    
    DBSETLUSER(login, option_set->username);
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Setting login password to %s", option_set->password)
        ));
    
    DBSETLPWD(login, option_set->password); 
    
    if (option_set->character_set)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: Setting login character set to %s", option_set->character_set)
            ));
    
        DBSETLCHARSET(login, option_set->character_set);
    }
    
    if (option_set->language)
    {
        DBSETLNATLANG(login, option_set->language);
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Setting login language to %s", option_set->language)
            ));
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

                #ifdef DBVERSION_74
                else if (strcmp(option_set->tds_version, "7.4") == 0)
                {
                        tds_version = DBVERSION_74;
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
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Setting login tds version to %s", option_set->tds_version)
            ));
    }
    
    if (option_set->database && !option_set->dbuse)
    {
        DBSETLDBNAME(login, option_set->database);
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Setting login database to %s", option_set->database)
            )); 
    }
    
    /* set an error handler that does not abort */
    dberrhandle(tds_err_capture);

    /* try all server names until we find a good one */
    servers = option_set->servername;
    last_error_message = NULL;
    while (servers != NULL)
    {
        /* find the length of the next server name */
        char *next_server = strchr(servers, ',');
        int server_len = next_server == NULL ? strlen(servers) : next_server - servers;

        /* construct a connect string */
        char *conn_string = palloc(server_len + 10);
        strncpy(conn_string, servers, server_len);
        if (option_set->port)
            sprintf(conn_string + server_len, ":%i", option_set->port);
        else
            conn_string[server_len] = '\0';

        ereport(DEBUG3,
            (errmsg("tds_fdw: Connection string is %s", conn_string)
            ));
        ereport(DEBUG3,
            (errmsg("tds_fdw: Connecting to server")
            ));

        /* try to connect */
        if ((*dbproc = dbopen(login, conn_string)) == NULL)
        {
            /* failure, will continue with the next server */
            ereport(DEBUG3,
                (errmsg("Failed to connect using connection string %s with user %s", conn_string, option_set->username)
                ));

            pfree(conn_string);
        }
        else
        {
            /* success, break the loop */
            ereport(DEBUG3,
                (errmsg("tds_fdw: Connected successfully")
                ));

            pfree(conn_string);
            break;
        }

        /* skip the comma if appropriate */
        servers = next_server ? next_server + 1 : NULL;
    }

    /* report an error if all connections fail */
    if (*dbproc == NULL)
    {
        if (last_error_message)
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
                    errmsg("%s", last_error_message)
                ));
        else
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
                    errmsg("Failed to connect to server %s with user %s", option_set->servername, option_set->username)
                ));
    }

    /* set the normal error handler again */
    dberrhandle(tds_err_handler);

    /* set a signal handler that cancels now that dbopen() is complete */
    dbsetinterrupt(*dbproc, tds_chkintr_func, tds_hndlintr_func);
    
    if (option_set->database && option_set->dbuse)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: Selecting database %s", option_set->database)
            ));
        
        if ((erc = dbuse(*dbproc, option_set->database)) == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
                    errmsg("Failed to select database %s", option_set->database)
                ));
        }
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Selected database")
            ));
    }

    /* Enable ANSI mode if requested */
    if (option_set->sqlserver_ansi_mode) {
        tdsSetSqlServerAnsiMode(dbproc);
    }
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsSetupConnection")
            ));
    #endif  

    return 0;
}

double tdsGetRowCountShowPlanAll(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc)
{
    double rows = 0;
    RETCODE erc;
    int ret_code;
    char* show_plan_query = "SET SHOWPLAN_ALL ON";
    char* show_plan_query_off = "SET SHOWPLAN_ALL OFF";
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetRowCountShowPlanAll")
            ));
    #endif  

    ereport(DEBUG3,
        (errmsg("tds_fdw: Setting database command to %s", show_plan_query)
        ));
    
    if ((erc = dbcmd(dbproc, show_plan_query)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to set current query to %s", show_plan_query)
            ));     
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Executing the query")
        ));
    
    if ((erc = dbsqlexec(dbproc)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to execute query %s", show_plan_query)
            )); 
    }

    ereport(DEBUG3,
        (errmsg("tds_fdw: Query executed correctly")
        ));         
    ereport(DEBUG3,
        (errmsg("tds_fdw: Getting results")
        ));                 
    
    erc = dbresults(dbproc);
    
    if (erc == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to get results from query %s", show_plan_query)
            ));
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Setting database command to %s", option_set->query)
        ));
    
    if ((erc = dbcmd(dbproc, option_set->query)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to set current query to %s", option_set->query)
            ));     
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Executing the query")
        ));
    
    if ((erc = dbsqlexec(dbproc)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to execute query %s", option_set->query)
            )); 
    }

    ereport(DEBUG3,
        (errmsg("tds_fdw: Query executed correctly")
        ));
    ereport(DEBUG3,
        (errmsg("tds_fdw: Getting results")
        ));             

    erc = dbresults(dbproc);
    
    if (erc == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to get results from query %s", option_set->query)
            ));
    }
    
    else if (erc == NO_MORE_RESULTS)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: There appears to be no results from query %s", option_set->query)
            ));
        
        goto cleanup_after_show_plan;
    }
    
    else if (erc == SUCCEED)
    {
        int ncol;
        int ncols;
        int parent = 0;
        double estimate_rows = 0;
        
        ncols = dbnumcols(dbproc);
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: %i columns", ncols)
            ));
        
        for (ncol = 0; ncol < ncols; ncol++)
        {
            char *col_name;

            col_name = dbcolname(dbproc, ncol + 1);
            
            if (strcmp(col_name, "Parent") == 0)
            {
                ereport(DEBUG3,
                    (errmsg("tds_fdw: Binding column %s (%i)", col_name, ncol + 1)
                    ));
                
                erc = dbbind(dbproc, ncol + 1, INTBIND, sizeof(int), (BYTE *)&parent);
                
                if (erc == FAIL)
                {
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to bind results for column %s to a variable.", col_name)
                        ));
                }
            }
            
            if (strcmp(col_name, "EstimateRows") == 0)
            {
                ereport(DEBUG3,
                    (errmsg("tds_fdw: Binding column %s (%i)", col_name, ncol + 1)
                    ));
                
                erc = dbbind(dbproc, ncol + 1, FLT8BIND, sizeof(double), (BYTE *)&estimate_rows);
                
                if (erc == FAIL)
                {
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to bind results for column %s to a variable.", col_name)
                        ));
                }               
            }
        }
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Successfully got results")
            ));
        
        while ((ret_code = dbnextrow(dbproc)) != NO_MORE_ROWS)
        {
            switch (ret_code)
            {
                case REG_ROW:

                    ereport(DEBUG3,
                        (errmsg("tds_fdw: Parent is %i. EstimateRows is %g.", parent, estimate_rows)
                    ));
                    
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
                    break;
                        
                case FAIL:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to get row while getting plan for query")
                        ));
                    break;
                
                default:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to get plan for query. Unknown return code.")
                        ));                 
            }
        }
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: We estimated %g rows.", rows)
            )); 
    }
    
    else
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Unknown return code getting results from query %s", option_set->query)
            ));     
    }
    
cleanup_after_show_plan:
        ereport(DEBUG3,
            (errmsg("tds_fdw: Setting database command to %s", show_plan_query_off)
            ));
    
    if ((erc = dbcmd(dbproc, show_plan_query_off)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to set current query to %s", show_plan_query_off)
            ));     
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Executing the query")
        ));
    
    if ((erc = dbsqlexec(dbproc)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to execute query %s", show_plan_query_off)
            )); 
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Query executed correctly")
        ));
    ereport(DEBUG3,
        (errmsg("tds_fdw: Getting results")
        ));             
    
    erc = dbresults(dbproc);
    
    if (erc == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to get results from query %s", show_plan_query)
            ));
    }   

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsGetRowCountShowPlanAll")
            ));
    #endif      
    

    return rows;
}

/* get the number of rows returned by a query */

double tdsGetRowCountExecute(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc)
{
    int rows_report = 0;
    long long int rows_increment = 0;
    RETCODE erc;
    int ret_code;
    int iscount = 0;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetRowCountExecute")
            ));
    #endif      
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Setting database command to %s", option_set->query)
        ));
    
    if ((erc = dbcmd(dbproc, option_set->query)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to set current query to %s", option_set->query)
            ));     
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Executing the query")
        ));
    
    if ((erc = dbsqlexec(dbproc)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to execute query %s", option_set->query)
            )); 
    }

    ereport(NOTICE,
        (errmsg("tds_fdw: Query executed correctly")
        ));
    ereport(NOTICE,
        (errmsg("tds_fdw: Getting results")
        ));             

    erc = dbresults(dbproc);
    
    if (erc == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to get results from query %s", option_set->query)
            ));
    }
    
    else if (erc == NO_MORE_RESULTS)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: There appears to be no results from query %s", option_set->query)
            ));
        
        goto cleanup;
    }
    
    else if (erc == SUCCEED)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: Successfully got results")
            ));
        
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
                    break;
                        
                case FAIL:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to get row while getting plan for query")
                        ));
                    break;
                
                default:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to get plan for query. Unknown return code.")
                        ));                 
            }
        }
        
        rows_report = DBCOUNT(dbproc);
        iscount = dbiscount(dbproc);
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: We counted %lli rows, and dbcount says %i rows.", rows_increment, rows_report)
            ));
        ereport(DEBUG3,
            (errmsg("tds_fdw: dbiscount says %i.", iscount)
            )); 
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

double tdsGetRowCount(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc)
{
    double rows = 0;
    
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

double tdsGetStartupCost(TdsFdwOptionSet* option_set)
{
    double startup_cost;
    
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

#if (PG_VERSION_NUM >= 90400)
int tdsDatetimeToDatum(DBPROCESS *dbproc, DBDATETIME *src, Datum *datetime_out)
{
    DBDATEREC datetime_in;
    RETCODE erc = dbdatecrack(dbproc, &datetime_in, src);
            
    if (erc == SUCCEED)
    {
        float8 seconds;
                
        #ifdef MSDBLIB
            seconds = (float8)datetime_in.second + ((float8)datetime_in.millisecond/1000);
                    
            ereport(DEBUG3,
                (errmsg("tds_fdw: Datetime value: year=%i, month=%i, day=%i, hour=%i, minute=%i, second=%i, millisecond=%i, timezone=%i,",
                    datetime_in.year, datetime_in.month, datetime_in.day, 
                    datetime_in.hour, datetime_in.minute, datetime_in.second,
                    datetime_in.millisecond, datetime_in.tzone)
                 ));
            ereport(DEBUG3,
                (errmsg("tds_fdw: Seconds=%f", seconds)
                 ));
                    
            *datetime_out = DirectFunctionCall6(make_timestamp, 
                 Int64GetDatum(datetime_in.year), Int64GetDatum(datetime_in.month), Int64GetDatum(datetime_in.day), 
                 Int64GetDatum(datetime_in.hour), Int64GetDatum(datetime_in.minute), Float8GetDatum(seconds));
        #else
            seconds = (float8)datetime_in.datesecond + ((float8)datetime_in.datemsecond/1000);
                    
            ereport(DEBUG3,
                (errmsg("tds_fdw: Datetime value: year=%i, month=%i, day=%i, hour=%i, minute=%i, second=%i, millisecond=%i, timezone=%i,",
                    datetime_in.dateyear, datetime_in.datemonth + 1, datetime_in.datedmonth, 
                    datetime_in.datehour, datetime_in.dateminute, datetime_in.datesecond,
                    datetime_in.datemsecond, datetime_in.datetzone)
                 ));
            ereport(DEBUG3,
                (errmsg("tds_fdw: Seconds=%f", seconds)
                 ));
                    
            /* Sybase uses different field names, and it uses 0-11 for the month */
            *datetime_out = DirectFunctionCall6(make_timestamp, 
                 Int64GetDatum(datetime_in.dateyear), Int64GetDatum(datetime_in.datemonth + 1), Int64GetDatum(datetime_in.datedmonth), 
                 Int64GetDatum(datetime_in.datehour), Int64GetDatum(datetime_in.dateminute), Float8GetDatum(seconds));
        #endif
    }

    return erc;
}
#endif

char* tdsConvertToCString(DBPROCESS* dbproc, int srctype, const BYTE* src, DBINT srclen)
{
    char* dest = NULL;
    int real_destlen = 0;
    DBINT destlen = 0;
    int desttype = -1;
    int ret_value;
    #if (PG_VERSION_NUM >= 90400)
    Datum datetime_out;
    RETCODE erc;
    #endif
    int use_tds_conversion = 1;

    switch(srctype)
    {
        case SYBCHAR:
            __attribute__ ((fallthrough));
        case SYBVARCHAR:
            __attribute__ ((fallthrough));
        case SYBTEXT:
            real_destlen = srclen + 1; /* the size of the array */
            destlen = -2; /* the size to pass to dbconvert (-2 means to null terminate it) */
            desttype = SYBCHAR;
            break;
        case SYBBINARY:
            __attribute__ ((fallthrough));
        case SYBVARBINARY:
            real_destlen = srclen;
            destlen = srclen;
            desttype = SYBBINARY;
            break;

        #if (PG_VERSION_NUM >= 90400)
        case SYBDATETIME:
            erc = tdsDatetimeToDatum(dbproc, (DBDATETIME *)src, &datetime_out);
            
            if (erc == SUCCEED)
            {
                const char *datetime_str = timestamptz_to_str(DatumGetTimestamp(datetime_out));
                
                dest = palloc(strlen(datetime_str) * sizeof(char));
                strcpy(dest, datetime_str);

                use_tds_conversion = 0;
                destlen = strlen(datetime_str);
                real_destlen = destlen;
                desttype = SYBCHAR;
            }

            break;

        #endif

        default:
            real_destlen = 1000; /* Probably big enough */
            destlen = -2; 
            desttype = SYBCHAR;
            break;
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Source type is %i. Destination type is %i", srctype, desttype)
        ));
    ereport(DEBUG3,
        (errmsg("tds_fdw: Source length is %i. Destination length is %i. Real destination length is %i", srclen, destlen, real_destlen)
        ));
    
    if (use_tds_conversion)
    {
        if (dbwillconvert(srctype, desttype) != FALSE)
        {
            dest = palloc(real_destlen * sizeof(char));
            ret_value = dbconvert(dbproc, srctype, src, srclen, desttype, (BYTE *) dest, destlen);
            
            if (ret_value == FAIL)
            {
                ereport(DEBUG3,
                    (errmsg("tds_fdw: Failed to convert column")
                    ));
            }
            
            else if (ret_value == -1)
            {
                ereport(DEBUG3,
                    (errmsg("tds_fdw: Failed to convert column. Could have been a NULL pointer or bad data type.")
                    )); 
            }
        }
        
        else
        {
            ereport(DEBUG3,
                (errmsg("tds_fdw: Column cannot be converted to this type.")
                ));
        }
    }
    
    return dest;
    
}

/* get output for EXPLAIN */

void tdsExplainForeignScan(ForeignScanState *node, ExplainState *es)
{
    TdsFdwExecutionState *festate = (TdsFdwExecutionState *) node->fdw_state;
    TdsFdwOptionSet option_set;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsExplainForeignScan")
            ));
    #endif

    tdsGetForeignTableOptionsFromCatalog(RelationGetRelid(node->ss.ss_currentRelation), &option_set);

    if (es->verbose)
    {
        ExplainPropertyText("Remote query", festate->query, es);
        ExplainPropertyBool("Use remote estimate", option_set.use_remote_estimate, es);
        ExplainPropertyInteger("Local tuple estimate", NULL, option_set.local_tuple_estimate, es);
        ExplainPropertyText("Row estimate method", option_set.row_estimate_method, es);
    }
    
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
    ForeignScan *fsplan = (ForeignScan *) node->ss.ps.plan;
    EState *estate = node->ss.ps.state;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsBeginForeignScan")
            ));
    #endif
    
    tds_clear_signals();
    
    tdsGetForeignTableOptionsFromCatalog(RelationGetRelid(node->ss.ss_currentRelation), &option_set);
        
    ereport(DEBUG3,
        (errmsg("tds_fdw: Initiating DB-Library")
        ));
    
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
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Getting login structure")
        ));
    
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
    
    festate = (TdsFdwExecutionState *) palloc(sizeof(TdsFdwExecutionState));
    node->fdw_state = (void *) festate;
    festate->login = login;
    festate->dbproc = dbproc;
    festate->query = strVal(list_nth(fsplan->fdw_private,
                                     FdwScanPrivateSelectSql));
    festate->retrieved_attrs = (List *) list_nth(fsplan->fdw_private,
                                               FdwScanPrivateRetrievedAttrs);
    festate->first = 1;
    festate->row = 0;
    festate->mem_cxt = AllocSetContextCreate(estate->es_query_cxt,
                                               "tds_fdw data",
                                               ALLOCSET_DEFAULT_SIZES);
    
cleanup:
    ;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsBeginForeignScan")
            ));
    #endif
}

void tdsGetColumnMetadata(ForeignScanState *node, TdsFdwOptionSet *option_set)
{
    MemoryContext old_cxt;
    int ncol;
    char* local_columns_found = NULL;
    TdsFdwExecutionState *festate = (TdsFdwExecutionState *)node->fdw_state;
    int num_retrieved_attrs = list_length(festate->retrieved_attrs);
    Oid relOid = RelationGetRelid(node->ss.ss_currentRelation);

    old_cxt = MemoryContextSwitchTo(festate->mem_cxt);

    festate->attinmeta = TupleDescGetAttInMetadata(node->ss.ss_currentRelation->rd_att);

    if (option_set->match_column_names && festate->ncols < num_retrieved_attrs)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_INCONSISTENT_DESCRIPTOR_INFORMATION),
            errmsg("Table definition mismatch: Foreign source returned %d column(s),"
                " but query expected %d column(s)",
                festate->ncols,
                num_retrieved_attrs)
            ));
    }
    
    else if (!option_set->match_column_names && festate->ncols < festate->attinmeta->tupdesc->natts)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_INCONSISTENT_DESCRIPTOR_INFORMATION),
            errmsg("Table definition mismatch: Foreign source returned %d column(s),"
                " but local table has %d column(s)",
                festate->ncols,
                festate->attinmeta->tupdesc->natts)
            ));
    }

    festate->columns = palloc(festate->ncols * sizeof(COL));
    festate->datums =  palloc(festate->attinmeta->tupdesc->natts * sizeof(*festate->datums));
    festate->isnull =  palloc(festate->attinmeta->tupdesc->natts * sizeof(*festate->isnull));

    if (option_set->match_column_names)
    {
        local_columns_found = palloc0(festate->attinmeta->tupdesc->natts);
    }

    for (ncol = 0; ncol < festate->ncols; ncol++)
    {   
        COL* column;
        
        column = &festate->columns[ncol];
        column->name = dbcolname(festate->dbproc, ncol + 1);
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Fetching column %i (%s)", ncol, column->name)
            ));
        
        column->srctype = dbcoltype(festate->dbproc, ncol + 1);
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Type is %i", column->srctype)
            ));

        if (option_set->match_column_names)
        {
            ListCell   *lc;
        
            ereport(DEBUG3,
                (errmsg("tds_fdw: Matching foreign column with local column by name.")
                ));
            
            column->local_index = -1;

            //for (local_ncol = 0; local_ncol < festate->attinmeta->tupdesc->natts; local_ncol++)
            foreach(lc, festate->retrieved_attrs)
            {
                /* this is indexed starting from 1, not 0 */
                int local_ncol = lfirst_int(lc) - 1;
                char* local_name = NULL;
                List *options;
                ListCell *inner_lc;
                
                ereport(DEBUG3,
                    (errmsg("tds_fdw: Comparing it to the following retrived column: %i", local_ncol)
                    ));
                
                options = GetForeignColumnOptions(relOid, local_ncol + 1);
                
                foreach(inner_lc, options)
                {
                    DefElem    *def = (DefElem *) lfirst(inner_lc);
                    
                    ereport(DEBUG3,
                        (errmsg("tds_fdw: Checking if column_name is set as an option:%s => %s", def->defname, defGetString(def))
                        ));

                    if (strcmp(def->defname, "column_name") == 0 
                        && strncmp(defGetString(def), column->name, NAMEDATALEN) == 0)
                    {
                        ereport(DEBUG3,
                            (errmsg("tds_fdw: It matches!")
                            ));
                        
                        local_name = defGetString(def);
                        column->local_index = local_ncol;
                        column->attr_oid = TupleDescAttr(festate->attinmeta->tupdesc, local_ncol)->atttypid;
                        local_columns_found[local_ncol] = 1;
                        break;
                    }
                }
                
                if (!local_name)
                {
                
                    local_name = TupleDescAttr(festate->attinmeta->tupdesc, local_ncol)->attname.data;
                    
                    ereport(DEBUG3,
                        (errmsg("tds_fdw: Comparing retrieved column name to the following local column name: %s", local_name)
                        ));

                    if (strncmp(local_name, column->name, NAMEDATALEN) == 0)
                    {
                        ereport(DEBUG3,
                            (errmsg("tds_fdw: It matches!")
                            ));
                        
                        column->local_index = local_ncol;
                        column->attr_oid = TupleDescAttr(festate->attinmeta->tupdesc, local_ncol)->atttypid;
                        local_columns_found[local_ncol] = 1;
                        break;
                    }
                }
            }

            if (column->local_index == -1)
            {
                ereport(WARNING,
                    (errcode(ERRCODE_FDW_INCONSISTENT_DESCRIPTOR_INFORMATION),
                    errmsg("Table definition mismatch: Foreign source has column named %s,"
                    " but target table does not. Column will be ignored.",
                    column->name)
                ));
            }
        }

        else
        {
        
            ereport(DEBUG3,
                (errmsg("tds_fdw: Matching foreign column with local column by position.")
                ));
            
            column->local_index = ncol;
            column->attr_oid = TupleDescAttr(festate->attinmeta->tupdesc, ncol)->atttypid;
        }
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Local index = %i, local type OID = %i", column->local_index, column->attr_oid)
            ));
    }

    if (option_set->match_column_names)
    {
        for (ncol = 0; ncol < festate->attinmeta->tupdesc->natts; ncol++)
        {
            if (local_columns_found[ncol] == 0)
            {
                ereport(DEBUG3,
                    (errcode(ERRCODE_FDW_INCONSISTENT_DESCRIPTOR_INFORMATION),
                    errmsg("Table definition mismatch: Could not match local column %s"
                    " with column from foreign table. It probably was not selected.",
                    TupleDescAttr(festate->attinmeta->tupdesc, ncol)->attname.data)
                ));

                /* pretend this is NULL, so Pg won't try to access an invalid Datum */
                festate->isnull[ncol] = 1;
            }
        }

        pfree(local_columns_found);
    }

    MemoryContextSwitchTo(old_cxt);
}

/* get next row from foreign table */

TupleTableSlot* tdsIterateForeignScan(ForeignScanState *node)
{
    TdsFdwOptionSet option_set;
    RETCODE erc;
    int ret_code;
    HeapTuple tuple;
    TdsFdwExecutionState *festate = (TdsFdwExecutionState *) node->fdw_state;
    EState *estate = node->ss.ps.state;
    TupleTableSlot *slot = node->ss.ss_ScanTupleSlot;
    int ncol;

    /* Cleanup */
    ExecClearTuple(slot);
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsIterateForeignScan")
            ));
    #endif
    
    if (festate->first)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: This is the first iteration")
            ));
        ereport(DEBUG3,
            (errmsg("tds_fdw: Setting database command to %s", festate->query)
            ));
        
        festate->first = 0;

        /* the following option is needed to get a proper size for blobs */
        if ((erc = dbsetopt(festate->dbproc, DBTEXTSIZE, "2147483647", -1)) == FAIL)
        {
            ereport(WARNING,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to set DBTEXTLIMIT server option, blob sizes may be truncated!")
                ));
        }

        if ((erc = dbcmd(festate->dbproc, festate->query)) == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to set current query to %s", festate->query)
                ));
        }
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Executing the query")
            ));
        
        if ((erc = dbsqlexec(festate->dbproc)) == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to execute query %s", festate->query)
                ));
        }

        ereport(DEBUG3,
            (errmsg("tds_fdw: Query executed correctly")
            ));
        ereport(DEBUG3,
            (errmsg("tds_fdw: Getting results")
            ));             

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
            Oid relOid;

            ereport(DEBUG3,
                (errmsg("tds_fdw: Successfully got results")
                ));

            ereport(DEBUG3,
                (errmsg("tds_fdw: Getting column info")
                ));

            festate->ncols = dbnumcols(festate->dbproc);

            ereport(DEBUG3,
                (errmsg("tds_fdw: %i columns", festate->ncols)
                ));

            MemoryContextReset(festate->mem_cxt);
            
            relOid = RelationGetRelid(node->ss.ss_currentRelation);
            
            ereport(DEBUG3,
                (errmsg("tds_fdw: Table OID is %i", relOid)
                ));
            
            tdsGetForeignTableOptionsFromCatalog(relOid, &option_set);  
            tdsGetColumnMetadata(node, &option_set);

            for (ncol = 0; ncol < festate->ncols; ncol++) {
                COL* column = &festate->columns[ncol];
                const int srctype = column->srctype;
                const Oid attr_oid = column->attr_oid;

                if (column->local_index == -1)
                {
                    continue;
                }

                erc = SUCCEED;
                column->useraw = false;
                
                ereport(DEBUG3,
                    (errmsg("tds_fdw: The foreign type is %i. The local type is %i.", srctype, attr_oid)
                    )); 

                if (srctype == SYBINT2 && attr_oid == INT2OID)
                    {
                    erc = dbbind(festate->dbproc, ncol + 1, SMALLBIND, sizeof(DBSMALLINT), (BYTE *)(&column->value.dbsmallint));
                    column->useraw = true;
                }
                else if (srctype == SYBINT4 && attr_oid == INT4OID)
                {
                    erc = dbbind(festate->dbproc, ncol + 1, INTBIND, sizeof(DBINT), (BYTE *)(&column->value.dbint));
                    column->useraw = true;
                }
                else if (srctype == SYBINT8 && attr_oid == INT8OID)
                {
                    erc = dbbind(festate->dbproc, ncol + 1, BIGINTBIND, sizeof(DBBIGINT), (BYTE *)(&column->value.dbbigint));
                    column->useraw = true;
                }
                else if (srctype == SYBREAL && attr_oid == FLOAT4OID)
                {
                    erc = dbbind(festate->dbproc, ncol + 1, REALBIND, sizeof(DBREAL), (BYTE *)(&column->value.dbreal));
                    column->useraw = true;
                }
                else if (srctype == SYBFLT8 && attr_oid == FLOAT8OID)
                {
                    erc = dbbind(festate->dbproc, ncol + 1, FLT8BIND, sizeof(DBFLT8), (BYTE *)(&column->value.dbflt8));
                    column->useraw = true;
                }
                else if ((srctype == SYBCHAR || srctype == SYBVARCHAR || srctype == SYBTEXT) &&
                     (attr_oid == TEXTOID))
                {
                    column->useraw = true;
                }
                else if ((srctype == SYBBINARY || srctype == SYBVARBINARY || srctype == SYBIMAGE) &&
                     (attr_oid == BYTEAOID))
                {
                    column->useraw = true;
                }
                #if (PG_VERSION_NUM >= 90400)
                else if (srctype == SYBDATETIME && attr_oid == TIMESTAMPOID)
                {
                    column->useraw = true;
                }
                #endif

                if (erc == FAIL)
                {
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                         errmsg("Failed to bind results for column %s to a variable.",
                            dbcolname(festate->dbproc, ncol + 1))));
                }
            }
        }
        
        else
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Unknown return code getting results from query %s", festate->query)
                ));
        }
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Fetching next row")
        ));
    
    if ((ret_code = dbnextrow(festate->dbproc)) != NO_MORE_ROWS)
    {
        switch (ret_code)
        {
            case REG_ROW:
                festate->row++;
                
                ereport(DEBUG3,
                    (errmsg("tds_fdw: Row %i fetched", festate->row)
                    ));
                
                if (show_before_row_memory_stats)
                {
                    fprintf(stderr,"Showing memory statistics before row %d.\n", festate->row);
                        
                    MemoryContextStats(estate->es_query_cxt);
                }

                for (ncol = 0; ncol < festate->ncols; ncol++)
                {
                    COL* column;
                    DBINT srclen;
                    BYTE* src;
                    char *cstring;
                    Oid attr_oid;
                    bytea *bytes;

                    column = &festate->columns[ncol];
                    attr_oid = column->attr_oid;

                    if (column->local_index == -1)
                    {
                        ereport(DEBUG3,
                            (errmsg("tds_fdw: Skipping column %s because it is not present in local table", column->name)
                        ));

                        continue;
                    }

                    srclen = dbdatlen(festate->dbproc, ncol + 1);
                    
                    ereport(DEBUG3,
                        (errmsg("tds_fdw: %s: Data length is %i", column->name, srclen)
                        ));             
                    
                    src = dbdata(festate->dbproc, ncol + 1);

                    if (srclen == 0)
                    {
                        ereport(DEBUG3,
                            (errmsg("tds_fdw: %s: Column value is NULL", column->name)
                            ));
                        
                        festate->isnull[column->local_index] = true;
                        continue;
                    }
                    else if (src == NULL)
                    {
                        ereport(DEBUG3,
                            (errmsg("tds_fdw: %s: Column value pointer is NULL, but probably shouldn't be", column->name)
                            ));
                        festate->isnull[column->local_index] = true;
                        continue;
                    }
                    else
                    {
                        festate->isnull[column->local_index] = false;
                    }

                    if (column->useraw)
                    {
                        switch (attr_oid)
                        {
                        case INT2OID:
                            festate->datums[column->local_index] = Int16GetDatum(column->value.dbsmallint);
                            break;
                        case INT4OID:
                            festate->datums[column->local_index] = Int32GetDatum(column->value.dbint);
                            break;
                        case INT8OID:
                            festate->datums[column->local_index] = Int64GetDatum(column->value.dbbigint);
                            break;
                        case FLOAT4OID:
                            festate->datums[column->local_index] = Float4GetDatum(column->value.dbreal);
                            break;
                        case FLOAT8OID:
                            festate->datums[column->local_index] = Float8GetDatum(column->value.dbflt8);
                            break;
                        case TEXTOID:
                            festate->datums[column->local_index] = PointerGetDatum(cstring_to_text_with_len((char *)src, srclen));
                            break;
                        case BYTEAOID:
                            bytes = palloc(srclen + VARHDRSZ);
                            SET_VARSIZE(bytes, srclen + VARHDRSZ);
                            memcpy(VARDATA(bytes), src, srclen);
                            festate->datums[column->local_index] = PointerGetDatum(bytes);
                            break;
                        #if (PG_VERSION_NUM >= 90400)
                        case TIMESTAMPOID:
                            erc = tdsDatetimeToDatum(festate->dbproc, (DBDATETIME *)src, &festate->datums[column->local_index]);
                            if (erc != SUCCEED)
                            {
                                ereport(ERROR,
                                    (errcode(ERRCODE_FDW_INVALID_ATTRIBUTE_VALUE),
                                     errmsg("Possibly invalid date value")));
                            }
                            break;
                        #endif
                        default:
                            ereport(ERROR,
                                (errcode(ERRCODE_FDW_ERROR),
                                 errmsg("%s marked useraw but wrong type (internal tds_fdw error)",
                                    dbcolname(festate->dbproc, ncol+1))));
                            break;
                        }
                    }
                    else
                    {
                        cstring = tdsConvertToCString(festate->dbproc, column->srctype, src, srclen);
                        festate->datums[column->local_index] = InputFunctionCall(&festate->attinmeta->attinfuncs[column->local_index],
                                              cstring,
                                              festate->attinmeta->attioparams[column->local_index],
                                              festate->attinmeta->atttypmods[column->local_index]);
                    }
                }
                
                if (show_after_row_memory_stats)
                {
                    fprintf(stderr,"Showing memory statistics after row %d.\n", festate->row);
                        
                    MemoryContextStats(estate->es_query_cxt);
                }

                tuple = heap_form_tuple(node->ss.ss_currentRelation->rd_att, festate->datums, festate->isnull);
#if PG_VERSION_NUM < 120000
                ExecStoreTuple(tuple, slot, InvalidBuffer, false);
#else
                ExecStoreHeapTuple(tuple, slot, false);
#endif              
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
        ereport(DEBUG3,
            (errmsg("tds_fdw: No more rows")
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

void tdsReScanForeignScan(ForeignScanState *node)
{
    TdsFdwExecutionState *festate = (TdsFdwExecutionState *) node->fdw_state;
    int ret_code = NO_MORE_ROWS;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsReScanForeignScan")
            ));
    #endif

    /*
     * Consume any remaining result rows.
     * This might be necessary if the scan stopped before all
     * rows were consumed.
     */
    if (!festate->first)
        while ((ret_code = dbnextrow(festate->dbproc)) == REG_ROW)
            ;

    if (ret_code != NO_MORE_ROWS)
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
            errmsg("Failed to get row during query")
            ));

    /* reset the state for the next scan */
    festate->first = 1;
    
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
    EState *estate = node->ss.ps.state;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsEndForeignScan")
            ));
    #endif
    
    old_cxt = MemoryContextSwitchTo(festate->mem_cxt);
    
    if (show_finished_memory_stats)
    {
        fprintf(stderr,"Showing memory statistics after query finished.\n");
            
        MemoryContextStats(estate->es_query_cxt);
    }
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Closing database connection")
        ));
    
    dbclose(festate->dbproc);
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Freeing login structure")
        ));
    
    dbloginfree(festate->login);
    
    ereport(DEBUG3,
        (errmsg("tds_fdw: Closing DB-Library")
        ));
    
    dbexit();
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsEndForeignScan")
            ));
    #endif

    MemoryContextSwitchTo(old_cxt);
    MemoryContextReset(festate->mem_cxt);
    
    tds_clear_signals();
}

/*
 * estimate_path_cost_size
 *      Get cost and size estimates for a foreign scan
 *
 * We assume that all the baserestrictinfo clauses will be applied, plus
 * any join clauses listed in join_conds.
 */
static void
estimate_path_cost_size(PlannerInfo *root,
                        RelOptInfo *baserel,
                        List *join_conds,
                        List *pathkeys,
                        double *p_rows, int *p_width,
                        Cost *p_startup_cost, Cost *p_total_cost, TdsFdwOptionSet *option_set)
{
    TdsFdwRelationInfo *fpinfo = (TdsFdwRelationInfo *) baserel->fdw_private;
    double      rows = 0.0;
    double      retrieved_rows = 0.0;
    int         width = 0;
    Cost        startup_cost = 0;
    Cost        total_cost = 0;
    Cost        run_cost;
    Cost        cpu_per_tuple;

    /*
     * If the table or the server is configured to use remote estimates,
     * connect to the foreign server and execute EXPLAIN to estimate the
     * number of rows selected by the restriction+join clauses.  Otherwise,
     * estimate rows using whatever statistics we have locally, in a way
     * similar to ordinary tables.
     */
    if (fpinfo->use_remote_estimate)
    {
        LOGINREC *login;
        DBPROCESS *dbproc;
        Selectivity local_sel;
        QualCost    local_cost;
        List       *remote_join_conds;
        List       *local_join_conds;
        List       *usable_pathkeys = NIL;
        ListCell   *lc;
        List *retrieved_attrs;
        
        /*
         * join_conds might contain both clauses that are safe to send across,
         * and clauses that aren't.
         */
        classifyConditions(root, baserel, baserel->baserestrictinfo,
                           &remote_join_conds, &local_join_conds);
                           
        /*
         * Determine whether we can potentially push query pathkeys to the remote
         * side, avoiding a local sort.
         */
        foreach(lc, pathkeys)
        {
            PathKey    *pathkey = (PathKey *) lfirst(lc);
            EquivalenceClass *pathkey_ec = pathkey->pk_eclass;
            Expr       *em_expr;

            /*
             * is_foreign_expr would detect volatile expressions as well, but
             * ec_has_volatile saves some cycles.
             */
            if (!pathkey_ec->ec_has_volatile &&
                (em_expr = find_em_expr_for_rel(pathkey_ec, baserel)) &&
                is_foreign_expr(root, baserel, em_expr))
                usable_pathkeys = lappend(usable_pathkeys, pathkey);
            else
            {
                /*
                 * The planner and executor don't have any clever strategy for
                 * taking data sorted by a prefix of the query's pathkeys and
                 * getting it to be sorted by all of those pathekeys.  We'll just
                 * end up resorting the entire data set.  So, unless we can push
                 * down all of the query pathkeys, forget it.
                 */
                list_free(usable_pathkeys);
                usable_pathkeys = NIL;
                break;
            }
        }
        
        tdsBuildForeignQuery(root, baserel, option_set, 
            fpinfo->attrs_used, &retrieved_attrs,
            fpinfo->remote_conds, remote_join_conds, usable_pathkeys);

        /* Get the remote estimate */

        ereport(DEBUG3,
            (errmsg("tds_fdw: Initiating DB-Library")
            ));
        
        if (dbinit() == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                    errmsg("Failed to initialize DB-Library environment")
                ));
            goto cleanup_before_init;
        }
        
        dberrhandle(tds_err_handler);
        
        if (option_set->msg_handler)
        {
            if (strcmp(option_set->msg_handler, "notice") == 0)
            {
                dbmsghandle(tds_notice_msg_handler);
            }
            
            else if (strcmp(option_set->msg_handler, "blackhole") == 0)
            {
                dbmsghandle(tds_blackhole_msg_handler);
            }
            
            else
            {
                ereport(ERROR,
                    (errcode(ERRCODE_SYNTAX_ERROR),
                        errmsg("Unknown msg handler: %s.", option_set->msg_handler)
                    ));
            }
        }
        
        ereport(DEBUG3,
            (errmsg("tds_fdw: Getting login structure")
            ));
        
        if ((login = dblogin()) == NULL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                    errmsg("Failed to initialize DB-Library login structure")
                ));
            goto cleanup_before_login;
        }
        
        if (tdsSetupConnection(option_set, login, &dbproc) != 0)
        {
            goto cleanup;
        }
            
        rows = tdsGetRowCount(option_set, login, dbproc);
        retrieved_rows = rows;
        
        width = option_set->fdw_tuple_cost;
        startup_cost = option_set->fdw_startup_cost;
        total_cost = 0;

        /* Factor in the selectivity of the locally-checked quals */
        local_sel = clauselist_selectivity(root,
                                           join_conds,
                                           baserel->relid,
                                           JOIN_INNER,
                                           NULL);
        local_sel *= fpinfo->local_conds_sel;

        rows = clamp_row_est(rows * local_sel);

        /* Add in the eval cost of the locally-checked quals */
        startup_cost += fpinfo->local_conds_cost.startup;
        total_cost += fpinfo->local_conds_cost.per_tuple * retrieved_rows;
        cost_qual_eval(&local_cost, join_conds, root);
        startup_cost += local_cost.startup;
        total_cost += local_cost.per_tuple * retrieved_rows;
        
cleanup:
    dbclose(dbproc);
    dbloginfree(login);
        
cleanup_before_login:
    dbexit();
    
cleanup_before_init:
    ;
    }
    else
    {
        /*
         * We don't support join conditions in this mode (hence, no
         * parameterized paths can be made).
         */
        Assert(join_conds == NIL);

        /* Use rows/width estimates made by set_baserel_size_estimates. */
        rows = baserel->rows;
#if (PG_VERSION_NUM < 90600)
        width = baserel->width;
#else
        width = baserel->reltarget->width;
#endif /* PG_VERSION_NUM < 90600 */

        /*
         * Back into an estimate of the number of retrieved rows.  Just in
         * case this is nuts, clamp to at most baserel->tuples.
         */
        retrieved_rows = clamp_row_est(rows / fpinfo->local_conds_sel);
        retrieved_rows = Min(retrieved_rows, baserel->tuples);

        /*
         * Cost as though this were a seqscan, which is pessimistic.  We
         * effectively imagine the local_conds are being evaluated remotely,
         * too.
         */
        startup_cost = 0;
        run_cost = 0;
        run_cost += seq_page_cost * baserel->pages;

        startup_cost += baserel->baserestrictcost.startup;
        cpu_per_tuple = cpu_tuple_cost + baserel->baserestrictcost.per_tuple;
        run_cost += cpu_per_tuple * baserel->tuples;

        /*
         * Without remote estimates, we have no real way to estimate the cost
         * of generating sorted output.  It could be free if the query plan
         * the remote side would have chosen generates properly-sorted output
         * anyway, but in most cases it will cost something.  Estimate a value
         * high enough that we won't pick the sorted path when the ordering
         * isn't locally useful, but low enough that we'll err on the side of
         * pushing down the ORDER BY clause when it's useful to do so.
         */
        if (pathkeys != NIL)
        {
            startup_cost *= DEFAULT_FDW_SORT_MULTIPLIER;
            run_cost *= DEFAULT_FDW_SORT_MULTIPLIER;
        }

        total_cost = startup_cost + run_cost;
    }

    /*
     * Add some additional cost factors to account for connection overhead
     * (fdw_startup_cost), transferring data across the network
     * (fdw_tuple_cost per retrieved row), and local manipulation of the data
     * (cpu_tuple_cost per retrieved row).
     */
    startup_cost += fpinfo->fdw_startup_cost;
    total_cost += fpinfo->fdw_startup_cost;
    total_cost += fpinfo->fdw_tuple_cost * retrieved_rows;
    total_cost += cpu_tuple_cost * retrieved_rows;

    /* Return results. */
    *p_rows = rows;
    *p_width = width;
    *p_startup_cost = startup_cost;
    *p_total_cost = total_cost;
}
    
void tdsGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid)
{
    TdsFdwRelationInfo *fpinfo;
    ListCell   *lc;
    TdsFdwOptionSet option_set;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetForeignRelSize")
            ));
    #endif
    
    tds_clear_signals();
    
    /*
     * We use PgFdwRelationInfo to pass various information to subsequent
     * functions.
     */
    fpinfo = (TdsFdwRelationInfo *) palloc0(sizeof(TdsFdwRelationInfo));
    baserel->fdw_private = (void *) fpinfo;

    /* Look up foreign-table catalog info. */
    fpinfo->table = GetForeignTable(foreigntableid);
    fpinfo->server = GetForeignServer(fpinfo->table->serverid);
    
    tdsGetForeignTableOptionsFromCatalog(foreigntableid, &option_set);
    
    fpinfo->use_remote_estimate = option_set.use_remote_estimate;
    fpinfo->fdw_startup_cost = option_set.fdw_startup_cost;
    fpinfo->fdw_tuple_cost = option_set.fdw_tuple_cost;
        
    /*
     * Identify which baserestrictinfo clauses can be sent to the remote
     * server and which can't.
     */
    classifyConditions(root, baserel, baserel->baserestrictinfo,
                       &fpinfo->remote_conds, &fpinfo->local_conds);

    /*
     * Identify which attributes will need to be retrieved from the remote
     * server.  These include all attrs needed for joins or final output, plus
     * all attrs used in the local_conds.  (Note: if we end up using a
     * parameterized scan, it's possible that some of the join clauses will be
     * sent to the remote and thus we wouldn't really need to retrieve the
     * columns used in them.  Doesn't seem worth detecting that case though.)
     */
    fpinfo->attrs_used = NULL;
#if (PG_VERSION_NUM < 90600)
    pull_varattnos((Node *) baserel->reltargetlist, baserel->relid,
                   &fpinfo->attrs_used);
#else
    pull_varattnos((Node *) baserel->reltarget->exprs, baserel->relid,
                   &fpinfo->attrs_used);
#endif /* PG_VERSION_NUM < 90600 */
    foreach(lc, fpinfo->local_conds)
    {
        RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

        pull_varattnos((Node *) rinfo->clause, baserel->relid,
                       &fpinfo->attrs_used);
    }

    /*
     * Compute the selectivity and cost of the local_conds, so we don't have
     * to do it over again for each path.  The best we can do for these
     * conditions is to estimate selectivity on the basis of local statistics.
     */
    fpinfo->local_conds_sel = clauselist_selectivity(root,
                                                     fpinfo->local_conds,
                                                     baserel->relid,
                                                     JOIN_INNER,
                                                     NULL);

    cost_qual_eval(&fpinfo->local_conds_cost, fpinfo->local_conds, root);

    /*
     * If the table or the server is configured to use remote estimates,
     * connect to the foreign server and execute EXPLAIN to estimate the
     * number of rows selected by the restriction clauses, as well as the
     * average row width.  Otherwise, estimate using whatever statistics we
     * have locally, in a way similar to ordinary tables.
     */
    if (fpinfo->use_remote_estimate)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: Using remote estimate")
        ));
        
        /*
         * Get cost/size estimates with help of remote server.  Save the
         * values in fpinfo so we don't need to do it again to generate the
         * basic foreign path.
         */
        estimate_path_cost_size(root, baserel, NIL, NIL,
                                &fpinfo->rows, &fpinfo->width,
                                &fpinfo->startup_cost, &fpinfo->total_cost, &option_set);

        /* Report estimated baserel size to planner. */
        baserel->rows = fpinfo->rows;
#if (PG_VERSION_NUM < 90600)
        baserel->width = fpinfo->width;
#else
        baserel->reltarget->width = fpinfo->width;
#endif /* PG_VERSION_NUM < 90600 */
    }
    else
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: Using local estimate")
        ));
        
        /*
         * If the foreign table has never been ANALYZEd, it will have relpages
         * and reltuples equal to zero, which most likely has nothing to do
         * with reality.  We can't do a whole lot about that if we're not
         * allowed to consult the remote server, but we can use a hack similar
         * to plancat.c's treatment of empty relations: use a minimum size
         * estimate of 10 pages, and divide by the column-datatype-based width
         * estimate to get the corresponding number of tuples.
         */
        if (baserel->tuples == 0)
        {
            baserel->tuples = option_set.local_tuple_estimate;
        }

        /* Estimate baserel size as best we can with local statistics. */
        set_baserel_size_estimates(root, baserel);

        /* Fill in basically-bogus cost estimates for use later. */
        estimate_path_cost_size(root, baserel, NIL, NIL,
                                &fpinfo->rows, &fpinfo->width,
                                &fpinfo->startup_cost, &fpinfo->total_cost, &option_set);
    }   
    
    
#if (PG_VERSION_NUM < 90600)
    ereport(DEBUG3,
            (errmsg("tds_fdw: Estimated rows = %f, estimated width = %d", baserel->rows, baserel->width)
            ));
#else
    ereport(DEBUG3,
            (errmsg("tds_fdw: Estimated rows = %f, estimated width = %d", baserel->rows,
                    baserel->reltarget->width)
            ));
#endif /* PG_VERSION_NUM < 90600 */

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
    TdsFdwOptionSet option_set;
    TdsFdwRelationInfo *fpinfo = (TdsFdwRelationInfo *) baserel->fdw_private;
    ForeignPath *path;
    #if (PG_VERSION_NUM >= 90500)
    List       *ppi_list;
    #endif
    ListCell   *lc;
    List       *usable_pathkeys = NIL;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetForeignPaths")
            ));
    #endif
    
    tdsGetForeignTableOptionsFromCatalog(foreigntableid, &option_set);
    
    /*
     * Create simplest ForeignScan path node and add it to baserel.  This path
     * corresponds to SeqScan path of regular tables (though depending on what
     * baserestrict conditions we were able to send to remote, there might
     * actually be an indexscan happening there).  We already did all the work
     * to estimate cost and size of this path.
     */
#if PG_VERSION_NUM < 90500
    path = create_foreignscan_path(root, baserel,
                                   fpinfo->rows,
                                   fpinfo->startup_cost,
                                   fpinfo->total_cost,
                                   NIL, /* no pathkeys */
                                   NULL,        /* no outer rel either */
                                   NIL);        /* no fdw_private list */   
#elif PG_VERSION_NUM < 90600
    path = create_foreignscan_path(root, baserel,
                                   fpinfo->rows,
                                   fpinfo->startup_cost,
                                   fpinfo->total_cost,
                                   NIL, /* no pathkeys */
                                   NULL,        /* no outer rel either */
                                   NULL,        /* no extra plan */
                                   NIL);        /* no fdw_private list */
#elif PG_VERSION_NUM < 170000
    path = create_foreignscan_path(root, baserel, NULL,
                                   fpinfo->rows,
                                   fpinfo->startup_cost,
                                   fpinfo->total_cost,
                                   NIL, /* no pathkeys */
                                   NULL,        /* no outer rel either */
                                   NULL,        /* no extra plan */
                                   NIL);        /* no fdw_private list */
#elif PG_VERSION_NUM < 180000
    path = create_foreignscan_path(root, baserel, NULL,
                                   fpinfo->rows,
                                   fpinfo->startup_cost,
                                   fpinfo->total_cost,
                                   NIL, /* no pathkeys */
                                   NULL,                /* no outer rel either */
                                   NULL,                /* no extra plan */
                                   NIL,                 /* no fdw_restrictinfo list */
                                   NIL);                /* no fdw_private list */
#else
    path = create_foreignscan_path(root, baserel, NULL,
                                   fpinfo->rows,
                                   0,                   /* no disabled plan nodes */
                                   fpinfo->startup_cost,
                                   fpinfo->total_cost,
                                   NIL, /* no pathkeys */
                                   NULL,                /* no outer rel either */
                                   NULL,                /* no extra plan */
                                   NIL,                 /* no fdw_restrictinfo list */
                                   NIL);                /* no fdw_private list */
#endif /* PG_VERSION_NUM < 90500 */
    
    add_path(baserel, (Path *) path);

    /*
     * Determine whether we can potentially push query pathkeys to the remote
     * side, avoiding a local sort.
     */
    foreach(lc, root->query_pathkeys)
    {
        PathKey    *pathkey = (PathKey *) lfirst(lc);
        EquivalenceClass *pathkey_ec = pathkey->pk_eclass;
        Expr       *em_expr;

        /*
         * is_foreign_expr would detect volatile expressions as well, but
         * ec_has_volatile saves some cycles.
         */
        if (!pathkey_ec->ec_has_volatile &&
            (em_expr = find_em_expr_for_rel(pathkey_ec, baserel)) &&
            is_foreign_expr(root, baserel, em_expr))
            usable_pathkeys = lappend(usable_pathkeys, pathkey);
        else
        {
            /*
             * The planner and executor don't have any clever strategy for
             * taking data sorted by a prefix of the query's pathkeys and
             * getting it to be sorted by all of those pathekeys.  We'll just
             * end up resorting the entire data set.  So, unless we can push
             * down all of the query pathkeys, forget it.
             */
            list_free(usable_pathkeys);
            usable_pathkeys = NIL;
            break;
        }
    }

    /* Create a path with useful pathkeys, if we found one. */
    if (usable_pathkeys != NULL)
    {
        double      rows;
        int         width;
        Cost        startup_cost;
        Cost        total_cost;

        estimate_path_cost_size(root, baserel, NIL, usable_pathkeys,
                                &rows, &width, &startup_cost, &total_cost, &option_set);

        add_path(baserel, (Path *)
#if PG_VERSION_NUM < 90500
                 create_foreignscan_path(root, baserel,
                                         rows,
                                         startup_cost,
                                         total_cost,
                                         usable_pathkeys,
                                         NULL,
                                         NIL));
#elif PG_VERSION_NUM < 90600
                 create_foreignscan_path(root, baserel,
                                         rows,
                                         startup_cost,
                                         total_cost,
                                         usable_pathkeys,
                                         NULL,
                                         NULL,
                                         NIL));
#elif PG_VERSION_NUM < 170000
                 create_foreignscan_path(root, baserel,
                                         NULL,
                                         rows,
                                         startup_cost,
                                         total_cost,
                                         usable_pathkeys,
                                         NULL,
                                         NULL,
                                         NIL));
#elif PG_VERSION_NUM < 180000
                 create_foreignscan_path(root, baserel,
                                         NULL,
                                         rows,
                                         startup_cost,
                                         total_cost,
                                         usable_pathkeys,
                                         NULL,
                                         NULL,
                                         NIL,
                                         NIL));
#else
                 create_foreignscan_path(root, baserel,
                                         NULL,
                                         rows,
                                         0,
                                         startup_cost,
                                         total_cost,
                                         usable_pathkeys,
                                         NULL,
                                         NULL,
                                         NIL,
                                         NIL));
#endif /* PG_VERSION_NUM < 90500 */
    }
    
    /* Don't worry about join pushdowns unless this is PostgreSQL 9.5+ */
    #if (PG_VERSION_NUM >= 90500)

    /*
     * If we're not using remote estimates, stop here.  We have no way to
     * estimate whether any join clauses would be worth sending across, so
     * don't bother building parameterized paths.
     */
    if (!fpinfo->use_remote_estimate)
        return;

    /*
     * Thumb through all join clauses for the rel to identify which outer
     * relations could supply one or more safe-to-send-to-remote join clauses.
     * We'll build a parameterized path for each such outer relation.
     *
     * It's convenient to manage this by representing each candidate outer
     * relation by the ParamPathInfo node for it.  We can then use the
     * ppi_clauses list in the ParamPathInfo node directly as a list of the
     * interesting join clauses for that rel.  This takes care of the
     * possibility that there are multiple safe join clauses for such a rel,
     * and also ensures that we account for unsafe join clauses that we'll
     * still have to enforce locally (since the parameterized-path machinery
     * insists that we handle all movable clauses).
     */
    ppi_list = NIL;
    foreach(lc, baserel->joininfo)
    {
        RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);
        Relids      required_outer;
        ParamPathInfo *param_info;

        /* Check if clause can be moved to this rel */
        if (!join_clause_is_movable_to(rinfo, baserel))
            continue;

        /* See if it is safe to send to remote */
        if (!is_foreign_expr(root, baserel, rinfo->clause))
            continue;

        /* Calculate required outer rels for the resulting path */
        required_outer = bms_union(rinfo->clause_relids,
                                   baserel->lateral_relids);
        /* We do not want the foreign rel itself listed in required_outer */
        required_outer = bms_del_member(required_outer, baserel->relid);

        /*
         * required_outer probably can't be empty here, but if it were, we
         * couldn't make a parameterized path.
         */
        if (bms_is_empty(required_outer))
            continue;

        /* Get the ParamPathInfo */
        param_info = get_baserel_parampathinfo(root, baserel,
                                               required_outer);
        Assert(param_info != NULL);

        /*
         * Add it to list unless we already have it.  Testing pointer equality
         * is OK since get_baserel_parampathinfo won't make duplicates.
         */
        ppi_list = list_append_unique_ptr(ppi_list, param_info);
    }

    /*
     * The above scan examined only "generic" join clauses, not those that
     * were absorbed into EquivalenceClauses.  See if we can make anything out
     * of EquivalenceClauses.
     */
    if (baserel->has_eclass_joins)
    {
        /*
         * We repeatedly scan the eclass list looking for column references
         * (or expressions) belonging to the foreign rel.  Each time we find
         * one, we generate a list of equivalence joinclauses for it, and then
         * see if any are safe to send to the remote.  Repeat till there are
         * no more candidate EC members.
         */
        ec_member_foreign_arg arg;

        arg.already_used = NIL;
        for (;;)
        {
            List       *clauses;

            /* Make clauses, skipping any that join to lateral_referencers */
            arg.current = NULL;
            clauses = generate_implied_equalities_for_column(root,
                                                             baserel,
                                                   ec_member_matches_foreign,
                                                             (void *) &arg,
                                               baserel->lateral_referencers);

            /* Done if there are no more expressions in the foreign rel */
            if (arg.current == NULL)
            {
                Assert(clauses == NIL);
                break;
            }

            /* Scan the extracted join clauses */
            foreach(lc, clauses)
            {
                RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);
                Relids      required_outer;
                ParamPathInfo *param_info;

                /* Check if clause can be moved to this rel */
                if (!join_clause_is_movable_to(rinfo, baserel))
                    continue;

                /* See if it is safe to send to remote */
                if (!is_foreign_expr(root, baserel, rinfo->clause))
                    continue;

                /* Calculate required outer rels for the resulting path */
                required_outer = bms_union(rinfo->clause_relids,
                                           baserel->lateral_relids);
                required_outer = bms_del_member(required_outer, baserel->relid);
                if (bms_is_empty(required_outer))
                    continue;

                /* Get the ParamPathInfo */
                param_info = get_baserel_parampathinfo(root, baserel,
                                                       required_outer);
                Assert(param_info != NULL);

                /* Add it to list unless we already have it */
                ppi_list = list_append_unique_ptr(ppi_list, param_info);
            }

            /* Try again, now ignoring the expression we found this time */
            arg.already_used = lappend(arg.already_used, arg.current);
        }
    }

    /*
     * Now build a path for each useful outer relation.
     */
    foreach(lc, ppi_list)
    {
        ParamPathInfo *param_info = (ParamPathInfo *) lfirst(lc);
        double      rows;
        int         width;
        Cost        startup_cost;
        Cost        total_cost;

        /* Get a cost estimate from the remote */
        estimate_path_cost_size(root, baserel,
                                param_info->ppi_clauses, NIL,
                                &rows, &width,
                                &startup_cost, &total_cost, &option_set);

        /*
         * ppi_rows currently won't get looked at by anything, but still we
         * may as well ensure that it matches our idea of the rowcount.
         */
        param_info->ppi_rows = rows;

        /* Make the path */
#if PG_VERSION_NUM < 90500
        path = create_foreignscan_path(root, baserel,
                                       rows,
                                       startup_cost,
                                       total_cost,
                                       NIL,     /* no pathkeys */
                                       param_info->ppi_req_outer,
                                       NIL);    /* no fdw_private list */
#elif PG_VERSION_NUM < 90600
        path = create_foreignscan_path(root, baserel,
                                       rows,
                                       startup_cost,
                                       total_cost,
                                       NIL,     /* no pathkeys */
                                       param_info->ppi_req_outer,
                                       NULL,
                                       NIL);    /* no fdw_private list */
#elif PG_VERSION_NUM < 170000
        path = create_foreignscan_path(root, baserel,
                                       NULL,
                                       rows,
                                       startup_cost,
                                       total_cost,
                                       NIL,     /* no pathkeys */
                                       param_info->ppi_req_outer,
                                       NULL,
                                       NIL);    /* no fdw_private list */
#elif PG_VERSION_NUM < 180000
        path = create_foreignscan_path(root, baserel,
                                       NULL,
                                       rows,
                                       startup_cost,
                                       total_cost,
                                       NIL,     /* no pathkeys */
                                       param_info->ppi_req_outer,
                                       NULL,
                                       NIL,     /* no fdw_restrictinfo list */
                                       NIL);    /* no fdw_private list */
#else
        path = create_foreignscan_path(root, baserel,
                                       NULL,
                                       rows,
                                       0,       /* no disabled plan nodes */
                                       startup_cost,
                                       total_cost,
                                       NIL,     /* no pathkeys */
                                       param_info->ppi_req_outer,
                                       NULL,
                                       NIL,     /* no fdw_restrictinfo list */
                                       NIL);    /* no fdw_private list */
#endif /* PG_VERSION_NUM < 90500 */
        add_path(baserel, (Path *) path);
    }
    
    #endif
    
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
#if (PG_VERSION_NUM >= 90500)
ForeignScan* tdsGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, 
    Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses, Plan *outer_plan)
#else
ForeignScan* tdsGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, 
    Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses)
#endif
{
    TdsFdwRelationInfo *fpinfo = (TdsFdwRelationInfo *) baserel->fdw_private;
    TdsFdwOptionSet option_set;
    Index       scan_relid = baserel->relid;
    List       *fdw_private;
    List       *remote_conds = NIL;
    List       *remote_exprs = NIL;
    List       *local_exprs = NIL;
    List       *params_list = NIL;
    List       *retrieved_attrs = NIL;
    ListCell   *lc;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetForeignPlan")
            ));
    #endif
    
    tdsGetForeignTableOptionsFromCatalog(foreigntableid, &option_set);
    
    /*
     * Separate the scan_clauses into those that can be executed remotely and
     * those that can't.  baserestrictinfo clauses that were previously
     * determined to be safe or unsafe by classifyConditions are shown in
     * fpinfo->remote_conds and fpinfo->local_conds.  Anything else in the
     * scan_clauses list will be a join clause, which we have to check for
     * remote-safety.
     *
     * Note: the join clauses we see here should be the exact same ones
     * previously examined by postgresGetForeignPaths.  Possibly it'd be worth
     * passing forward the classification work done then, rather than
     * repeating it here.
     *
     * This code must match "extract_actual_clauses(scan_clauses, false)"
     * except for the additional decision about remote versus local execution.
     * Note however that we don't strip the RestrictInfo nodes from the
     * remote_conds list, since appendWhereClause expects a list of
     * RestrictInfos.
     */
    foreach(lc, scan_clauses)
    {
        RestrictInfo *rinfo = (RestrictInfo *) lfirst(lc);

        Assert(IsA(rinfo, RestrictInfo));

        /* Ignore any pseudoconstants, they're dealt with elsewhere */
        if (rinfo->pseudoconstant)
            continue;

        if (list_member_ptr(fpinfo->remote_conds, rinfo))
        {
            remote_conds = lappend(remote_conds, rinfo);
            remote_exprs = lappend(remote_exprs, rinfo->clause);
        }
        else if (list_member_ptr(fpinfo->local_conds, rinfo))
            local_exprs = lappend(local_exprs, rinfo->clause);
        else if (is_foreign_expr(root, baserel, rinfo->clause))
        {
            remote_conds = lappend(remote_conds, rinfo);
            remote_exprs = lappend(remote_exprs, rinfo->clause);
        }
        else
            local_exprs = lappend(local_exprs, rinfo->clause);
    }

    /*
     * Build the query string to be sent for execution, and identify
     * expressions to be sent as parameters.
     */
     
    tdsBuildForeignQuery(root, baserel, &option_set, 
        fpinfo->attrs_used, &retrieved_attrs, 
        remote_conds, NULL, best_path->path.pathkeys);

    /*
     * Build the fdw_private list that will be available to the executor.
     * Items in the list must match enum FdwScanPrivateIndex, above.
     */
    fdw_private = list_make2(makeString(option_set.query),
                             retrieved_attrs);

    /*
     * Create the ForeignScan node from target list, filtering expressions,
     * remote parameter expressions, and FDW private information.
     *
     * Note that the remote parameter expressions are stored in the fdw_exprs
     * field of the finished plan node; we can't keep them in private state
     * because then they wouldn't be subject to later planner processing.
     */

    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsGetForeignPlan")
            ));
    #endif

    #if (PG_VERSION_NUM >= 90500)
    return make_foreignscan(tlist,
                            local_exprs,
                            scan_relid,
                            params_list,
                            fdw_private,
                            NIL,    /* no custom tlist */
                            remote_exprs,
                            outer_plan);
    #else
        return make_foreignscan(tlist, local_exprs, scan_relid, params_list, fdw_private);
    #endif
}

static bool
tdsExecuteQuery(char *query, DBPROCESS *dbproc)
{
    RETCODE     erc;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsExecuteQuery")
            ));
    #endif

    ereport(DEBUG3,
        (errmsg("tds_fdw: Setting database command to %s", query)
        ));

    if ((erc = dbcmd(dbproc, query)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to set current query to %s", query)
            ));
    }

    ereport(DEBUG3,
        (errmsg("tds_fdw: Executing the query")
        ));

    if ((erc = dbsqlexec(dbproc)) == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to execute query %s", query)
            ));
    }

    ereport(DEBUG3,
        (errmsg("tds_fdw: Query executed correctly")
        ));

    ereport(DEBUG3,
        (errmsg("tds_fdw: Getting results")
        ));

    erc = dbresults(dbproc);

    if (erc == FAIL)
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Failed to get results from query %s", query)
            ));
    }

    else if (erc == NO_MORE_RESULTS)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: There appears to be no results from query %s", query)
            ));

        goto cleanup;
    }

    else if (erc == SUCCEED)
    {
        ereport(DEBUG3,
            (errmsg("tds_fdw: Successfully got results")
            ));

        goto cleanup;
    }

    else
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                errmsg("Unknown return code getting results from query %s", query)
            ));
    }

cleanup:
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsExecuteQuery")
            ));
    #endif

    return (erc == SUCCEED);
}

#ifdef IMPORT_API

/*
 * Translate SQL Server/Sybase default expressions to PostgreSQL equivalents.
 * Returns a newly allocated string with the translated expression, or NULL if
 * the expression should be skipped.
 */
static char *
translateSqlServerDefault(const char *sql_server_default)
{
    char *result;
    const char *trimmed;
    
    if (sql_server_default == NULL || sql_server_default[0] == '\0')
        return NULL;
    
    /* Skip leading/trailing parentheses and whitespace */
    trimmed = sql_server_default;
    while (*trimmed == '(' || *trimmed == ' ' || *trimmed == '\t')
        trimmed++;
    
    /* Case-insensitive comparison for common SQL Server functions */
    if (pg_strncasecmp(trimmed, "getdate()", 9) == 0)
    {
        result = pstrdup("CURRENT_TIMESTAMP");
    }
    else if (pg_strncasecmp(trimmed, "getutcdate()", 12) == 0)
    {
        result = pstrdup("(CURRENT_TIMESTAMP AT TIME ZONE 'UTC')");
    }
    else if (pg_strncasecmp(trimmed, "sysdatetime()", 13) == 0)
    {
        result = pstrdup("CURRENT_TIMESTAMP");
    }
    else if (pg_strncasecmp(trimmed, "sysutcdatetime()", 16) == 0)
    {
        result = pstrdup("(CURRENT_TIMESTAMP AT TIME ZONE 'UTC')");
    }
    else if (pg_strncasecmp(trimmed, "current_timestamp", 17) == 0)
    {
        result = pstrdup("CURRENT_TIMESTAMP");
    }
    else if (pg_strncasecmp(trimmed, "newid()", 7) == 0)
    {
        result = pstrdup("gen_random_uuid()");
    }
    else
    {
        /* For other expressions, try to use them as-is but wrapped in parentheses */
        /* This includes numeric constants, string literals, etc. */
        result = psprintf("%s", sql_server_default);
    }
    
    return result;
}

static List *
tdsImportSqlServerSchema(ImportForeignSchemaStmt *stmt, DBPROCESS  *dbproc,
                         TdsFdwOptionSet option_set,
                         bool import_default, bool import_not_null)
{
    List       *commands = NIL;
    ListCell   *lc;
    StringInfoData buf;

    RETCODE     erc;
    int         ret_code;

    tds_clear_signals();
    
    initStringInfo(&buf);

    /* Check that the schema really exists */
    appendStringInfoString(&buf, "SELECT schema_name FROM INFORMATION_SCHEMA.SCHEMATA WHERE schema_name = ");
    deparseStringLiteral(&buf, stmt->remote_schema);

    if (!tdsExecuteQuery(buf.data, dbproc))
        ereport(ERROR,
                (errcode(ERRCODE_FDW_SCHEMA_NOT_FOUND),
          errmsg("schema \"%s\" is not present on foreign server \"%s\"",
                 stmt->remote_schema, option_set.servername)));
    else
        /* Process results */
        while ((ret_code = dbnextrow(dbproc)) != NO_MORE_ROWS)
        {
            /* Do nothing */
        }
    resetStringInfo(&buf);

    /*
     * Fetch all table data from this schema, possibly restricted by
     * EXCEPT or LIMIT TO.  (We don't actually need to pay any attention
     * to EXCEPT/LIMIT TO here, because the core code will filter the
     * statements we return according to those lists anyway.  But it
     * should save a few cycles to not process excluded tables in the
     * first place.)
     */
    appendStringInfoString(&buf,
                           "SELECT t.table_name,"
                           "  c.column_name, "
                           "  c.data_type, "
                           "  c.column_default, "
                           "  c.is_nullable, "
                           "  c.character_maximum_length, "
                           "  c.numeric_precision, "
                           "  c.numeric_precision_radix, "
                           "  c.numeric_scale, "
                           "  c.datetime_precision "
                           "FROM INFORMATION_SCHEMA.TABLES t "
                           "  LEFT JOIN INFORMATION_SCHEMA.COLUMNS c ON "
                           "    t.table_schema = c.table_schema "
                           "      AND t.table_name = c.table_name "
                           "WHERE t.table_type IN ('BASE TABLE','VIEW') "
                           "  AND t.table_schema = ");
    deparseStringLiteral(&buf, stmt->remote_schema);

    /* Apply restrictions for LIMIT TO and EXCEPT */
    if (stmt->list_type == FDW_IMPORT_SCHEMA_LIMIT_TO ||
        stmt->list_type == FDW_IMPORT_SCHEMA_EXCEPT)
    {
        bool        first_item = true;

        appendStringInfoString(&buf, " AND t.table_name ");
        if (stmt->list_type == FDW_IMPORT_SCHEMA_EXCEPT)
            appendStringInfoString(&buf, "NOT ");
        appendStringInfoString(&buf, "IN (");

        /* Append list of table names within IN clause */
        foreach(lc, stmt->table_list)
        {
            RangeVar   *rv = (RangeVar *) lfirst(lc);

            if (first_item)
                first_item = false;
            else
                appendStringInfoString(&buf, ", ");
            deparseStringLiteral(&buf, rv->relname);
        }
        appendStringInfoChar(&buf, ')');
    }

    /* Append ORDER BY at the end of query to ensure output ordering */
    appendStringInfoString(&buf, " ORDER BY t.table_name, c.ordinal_position");

    if (tdsExecuteQuery(buf.data, dbproc))
    {
        char        table_name[255],
                    prev_table[255],
                    column_name[255],
                    data_type[255],
                    column_default[4000],
                    is_nullable[10];
        int         char_len,
                    numeric_precision,
                    numeric_precision_radix,
                    numeric_scale,
                    datetime_precision;
        bool        first_column = true;
        bool        first_table = true;

        /* Check if there's rows in resultset and if not do not execute the rest */
        erc = dbrows(dbproc);

        if (erc == FAIL)
        {
            ereport(NOTICE,
                    (errmsg("tds_fdw: No table were found in schema %s", stmt->remote_schema))
                );
                return commands;
        }

        prev_table[0] = '\0';

        erc = dbbind(dbproc, 1, NTBSTRINGBIND, sizeof(table_name), (BYTE *) table_name);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"table_name\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 2, NTBSTRINGBIND, sizeof(column_name), (BYTE *) column_name);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"column_name\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 3, NTBSTRINGBIND, sizeof(data_type), (BYTE *) data_type);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"data_type\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 4, NTBSTRINGBIND, sizeof(column_default), (BYTE *) column_default);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"column_default\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 5, NTBSTRINGBIND, sizeof(is_nullable), (BYTE *) is_nullable);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"is_nullable\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 6, INTBIND, sizeof(int), (BYTE *) &char_len);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"character_maximum_length\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 7, INTBIND, sizeof(int), (BYTE *) &numeric_precision);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"numeric_precision\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 8, INTBIND, sizeof(int), (BYTE *) &numeric_precision_radix);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"numeric_precision_radix\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 9, INTBIND, sizeof(int), (BYTE *) &numeric_scale);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"numeric_scale\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 10, INTBIND, sizeof(int), (BYTE *) &datetime_precision);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"datetime_precision\" to a variable.")
                ));
        }

        /* Process results */
        while ((ret_code = dbnextrow(dbproc)) != NO_MORE_ROWS)
        {
            switch (ret_code)
            {
                case REG_ROW:
                    ereport(DEBUG3,
                        (errmsg("tds_fdw: column \"%s.%s\"", table_name, column_name)
                        ));

                    /* Build query for the new table */
                    if (first_table || strcmp(prev_table, table_name) != 0)
                    {
                        if (!first_table)
                        {
                            /*
                             * Add server name and table-level options.  We specify remote
                             * schema and table name as options (the latter to ensure that
                             * renaming the foreign table doesn't break the association).
                             */
                            appendStringInfo(&buf, "\n) SERVER %s\nOPTIONS (",
                                             quote_identifier(stmt->server_name));

                            appendStringInfoString(&buf, "schema_name ");
                            deparseStringLiteral(&buf, stmt->remote_schema);
                            appendStringInfoString(&buf, ", table_name ");
                            deparseStringLiteral(&buf, prev_table);

                            appendStringInfoString(&buf, ");");

                            commands = lappend(commands, pstrdup(buf.data));
                        }

                        resetStringInfo(&buf);
                        appendStringInfo(&buf, "CREATE FOREIGN TABLE %s (\n",
                                         quote_identifier(table_name));
                        first_column = true;
                        first_table = false;
                    }

                    if (first_column)
                        first_column = false;
                    else
                        appendStringInfoString(&buf, ",\n");

                    /* Print column name */
                    appendStringInfo(&buf, "  %s",
                                     quote_identifier(column_name));

                    /* Print column type */

                    /* Numeric types */
                    if (strcmp(data_type, "bit") == 0 ||
                        strcmp(data_type, "smallint") == 0 ||
                        strcmp(data_type, "tinyint") == 0)
                        appendStringInfoString(&buf, " smallint");
                    else if (strcmp(data_type, "int") == 0)
                        appendStringInfoString(&buf, " integer");
                    else if (strcmp(data_type, "bigint") == 0)
                        appendStringInfoString(&buf, " bigint");
                    else if (strcmp(data_type, "decimal") == 0)
                    {
                        if (numeric_scale == 0)
                            appendStringInfo(&buf, " decimal(%d)", numeric_precision);
                        else
                            appendStringInfo(&buf, " decimal(%d, %d)",
                                             numeric_precision, numeric_scale);
                    }
                    else if (strcmp(data_type, "numeric") == 0)
                    {
                        if (numeric_scale == 0)
                            appendStringInfo(&buf, " numeric(%d)", numeric_precision);
                        else
                            appendStringInfo(&buf, " numeric(%d, %d)",
                                             numeric_precision, numeric_scale);
                    }
                     else if (strcmp(data_type, "money") == 0)
                         appendStringInfoString(&buf, " numeric(19,4)");
                      else if (strcmp(data_type, "smallmoney") == 0)
                          appendStringInfoString(&buf, " numeric(10,4)");

                    /* Floating-point types */
                    else if (strcmp(data_type, "float") == 0)
                        if (numeric_precision == 0)
                            appendStringInfoString(&buf, " float");
                        else
                            appendStringInfo(&buf, " float(%d)", numeric_precision);
                    else if (strcmp(data_type, "real") == 0)
                        appendStringInfoString(&buf, " real");

                    /* Date/type types */
                    else if (strcmp(data_type, "date") == 0)
                        appendStringInfoString(&buf, " date");
                    else if (strcmp(data_type, "datetime") == 0 ||
                             strcmp(data_type, "datetime2") == 0 ||
                             strcmp(data_type, "smalldatetime") == 0)
                        appendStringInfo(&buf, " timestamp(%d) without time zone", (datetime_precision > 6) ? 6 : datetime_precision);
                    else if (strcmp(data_type, "datetimeoffset") == 0)
                        appendStringInfo(&buf, " timestamp(%d) with time zone", (datetime_precision > 6) ? 6 : datetime_precision);
                    else if (strcmp(data_type, "time") == 0)
                        appendStringInfoString(&buf, " time");

                    /* Character types */
                    else if (strcmp(data_type, "char") == 0 ||
                             strcmp(data_type, "nchar") == 0)
                        appendStringInfo(&buf, " char(%d)", char_len);
                    else if (strcmp(data_type, "varchar") == 0 ||
                             strcmp(data_type, "nvarchar") == 0)
                    {
                        if (char_len == -1)
                            appendStringInfoString(&buf, " text");
                        else
                            appendStringInfo(&buf, " varchar(%d)", char_len);
                    }
                    else if (strcmp(data_type, "text") == 0 ||
                             strcmp(data_type, "ntext") == 0)
                        appendStringInfoString(&buf, " text");

                    /* Binary types */
                    else if (strcmp(data_type, "binary") == 0 ||
                        strcmp(data_type, "varbinary") == 0 ||
                        strcmp(data_type, "image") == 0 ||
                        strcmp(data_type, "rowversion") == 0 ||
                        strcmp(data_type, "timestamp") == 0 )
                        appendStringInfoString(&buf, " bytea");

                    /* Other types */
                    else if (strcmp(data_type, "xml") == 0)
                        appendStringInfoString(&buf, " xml");
                    else if (strcmp(data_type, "uniqueidentifier") == 0)
                        appendStringInfoString(&buf, " uuid");
                    else
                    {
                        ereport(DEBUG3,
                            (errmsg("tds_fdw: column \"%s\" of table \"%s\" has an untranslatable data type", column_name, table_name)
                            ));
                        appendStringInfoString(&buf, " text");
                    }

                    /*
                     * Add column_name option so that renaming the foreign table's
                     * column doesn't break the association to the underlying
                     * column.
                     */
                    appendStringInfoString(&buf, " OPTIONS (column_name ");
                    deparseStringLiteral(&buf, column_name);
                    appendStringInfoChar(&buf, ')');

                    /* Add DEFAULT if needed */
                    if (import_default && column_default[0] != '\0')
                    {
                        char *translated_default = translateSqlServerDefault(column_default);
                        if (translated_default != NULL)
                        {
                            appendStringInfo(&buf, " DEFAULT %s", translated_default);
                            pfree(translated_default);
                        }
                    }

                    /* Add NOT NULL if needed */
                    if (import_not_null && strcmp(is_nullable, "NO") == 0)
                        appendStringInfoString(&buf, " NOT NULL");

                    strcpy(prev_table, table_name);

                    break;

                case BUF_FULL:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                            errmsg("Buffer filled up while getting plan for query")
                        ));
                    break;

                case FAIL:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to get row while getting plan for query")
                        ));
                    break;

                default:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to get plan for query. Unknown return code.")
                        ));
            }
        }

        /*
         * Add server name and table-level options.  We specify remote
         * schema and table name as options (the latter to ensure that
         * renaming the foreign table doesn't break the association).
         */
        appendStringInfo(&buf, "\n) SERVER %s\nOPTIONS (",
                         quote_identifier(stmt->server_name));

        appendStringInfoString(&buf, "schema_name ");
        deparseStringLiteral(&buf, stmt->remote_schema);
        appendStringInfoString(&buf, ", table_name ");
        deparseStringLiteral(&buf, prev_table);

        appendStringInfoString(&buf, ");");

        commands = lappend(commands, pstrdup(buf.data));
    }

    return commands;
}

static List *
tdsImportSybaseSchema(ImportForeignSchemaStmt *stmt, DBPROCESS  *dbproc,
                      TdsFdwOptionSet option_set,
                      bool import_default, bool import_not_null,
                      bool keep_custom_types)
{
    List       *commands = NIL;
    ListCell   *lc;
    StringInfoData buf;

    RETCODE     erc;
    int         ret_code;

    initStringInfo(&buf);

    /* Check that the schema really exists */
    appendStringInfoString(&buf, "SELECT name FROM sysusers WHERE name = ");
    deparseStringLiteral(&buf, stmt->remote_schema);

    if (!tdsExecuteQuery(buf.data, dbproc))
        ereport(ERROR,
                (errcode(ERRCODE_FDW_SCHEMA_NOT_FOUND),
          errmsg("schema \"%s\" is not present on foreign server \"%s\"",
                 stmt->remote_schema, option_set.servername)));
    else
        /* Process results */
        while ((ret_code = dbnextrow(dbproc)) != NO_MORE_ROWS)
        {
            /* Do nothing */
        }
    resetStringInfo(&buf);

    /*
     * Fetch all table data from this schema, possibly restricted by
     * EXCEPT or LIMIT TO.  (We don't actually need to pay any attention
     * to EXCEPT/LIMIT TO here, because the core code will filter the
     * statements we return according to those lists anyway.  But it
     * should save a few cycles to not process excluded tables in the
     * first place.)
     */
    appendStringInfoString(&buf,
                           "SELECT so.name AS table_name, "
                           "  sc.name AS column_name, ");
    if (keep_custom_types)
        appendStringInfoString(&buf, " st.name AS data_type, ");
    else
        appendStringInfoString(&buf,
                           "  CASE WHEN st.usertype < 100 "
                           "     THEN st.name "
                           "     ELSE (SELECT s1.name FROM dbo.systypes s1 WHERE s1.usertype = "
                           "       (SELECT min(s2.usertype) FROM dbo.systypes s2 WHERE s2.type = st.type)) "
                           "  END AS data_type, ");

    appendStringInfoString(&buf,
                           "  SUBSTRING(sm.text, 10, 255) AS column_default, "
                           "  CASE (sc.status & 0x08) "
                           "    WHEN 8 THEN 'YES' ELSE 'NO' "
                           "  END AS is_nullable, "
                           "  sc.length, "
                           "  sc.prec, "
                           "  sc.scale "
                           "FROM dbo.sysobjects so "
                           "  INNER JOIN dbo.sysusers su ON su.uid = so.uid "
                           "  LEFT JOIN dbo.syscolumns sc ON sc.id = so.id "
                           "  LEFT JOIN dbo.systypes st ON st.usertype = sc.usertype "
                           "  LEFT JOIN dbo.syscomments sm ON sm.id = sc.cdefault "
                           "WHERE so.type IN ('U','V') AND su.name = ");

    deparseStringLiteral(&buf, stmt->remote_schema);

    /* Apply restrictions for LIMIT TO and EXCEPT */
    if (stmt->list_type == FDW_IMPORT_SCHEMA_LIMIT_TO ||
        stmt->list_type == FDW_IMPORT_SCHEMA_EXCEPT)
    {
        bool        first_item = true;

        appendStringInfoString(&buf, " AND so.name ");
        if (stmt->list_type == FDW_IMPORT_SCHEMA_EXCEPT)
            appendStringInfoString(&buf, "NOT ");
        appendStringInfoString(&buf, "IN (");

        /* Append list of table names within IN clause */
        foreach(lc, stmt->table_list)
        {
            RangeVar   *rv = (RangeVar *) lfirst(lc);

            if (first_item)
                first_item = false;
            else
                appendStringInfoString(&buf, ", ");
            deparseStringLiteral(&buf, rv->relname);
        }
        appendStringInfoChar(&buf, ')');
    }

    /* Append ORDER BY at the end of query to ensure output ordering */
    appendStringInfoString(&buf, " ORDER BY so.name, sc.colid");

    if (tdsExecuteQuery(buf.data, dbproc))
    {
        char        table_name[255],
                    prev_table[255],
                    column_name[255],
                    data_type[255],
                    column_default[4000],
                    is_nullable[10];
        int         char_len,
                    numeric_precision,
                    numeric_scale;
        bool        first_column = true;
        bool        first_table = true;
        /* Check if there's rows in resultset and if not do not execute the rest */
        erc = dbrows(dbproc);

        if (erc == FAIL)
        {
            ereport(NOTICE,
                    (errmsg("tds_fdw: No table were found in schema %s", stmt->remote_schema))
                );
                return commands;
        }

        prev_table[0] = '\0';

        erc = dbbind(dbproc, 1, NTBSTRINGBIND, sizeof(table_name), (BYTE *) table_name);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"table_name\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 2, NTBSTRINGBIND, sizeof(column_name), (BYTE *) column_name);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"column_name\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 3, NTBSTRINGBIND, sizeof(data_type), (BYTE *) data_type);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"data_type\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 4, NTBSTRINGBIND, sizeof(column_default), (BYTE *) column_default);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"column_default\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 5, NTBSTRINGBIND, sizeof(is_nullable), (BYTE *) is_nullable);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"is_nullable\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 6, INTBIND, sizeof(int), (BYTE *) &char_len);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"length\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 7, INTBIND, sizeof(int), (BYTE *) &numeric_precision);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"prec\" to a variable.")
                ));
        }

        erc = dbbind(dbproc, 8, INTBIND, sizeof(int), (BYTE *) &numeric_scale);
        if (erc == FAIL)
        {
            ereport(ERROR,
                (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                    errmsg("Failed to bind results for column \"scale\" to a variable.")
                ));
        }

        /* Process results */
        while ((ret_code = dbnextrow(dbproc)) != NO_MORE_ROWS)
        {
            switch (ret_code)
            {
                case REG_ROW:
                    ereport(DEBUG3,
                        (errmsg("tds_fdw: column \"%s.%s\"", table_name, column_name)
                        ));

                    /* Build query for the new table */
                    if (first_table || strcmp(prev_table, table_name) != 0)
                    {
                        if (!first_table)
                        {
                            /*
                             * Add server name and table-level options.  We specify remote
                             * schema and table name as options (the latter to ensure that
                             * renaming the foreign table doesn't break the association).
                             */
                            appendStringInfo(&buf, "\n) SERVER %s\nOPTIONS (",
                                             quote_identifier(stmt->server_name));

                            appendStringInfoString(&buf, "schema_name ");
                            deparseStringLiteral(&buf, stmt->remote_schema);
                            appendStringInfoString(&buf, ", table_name ");
                            deparseStringLiteral(&buf, prev_table);

                            appendStringInfoString(&buf, ");");

                            commands = lappend(commands, pstrdup(buf.data));
                        }

                        resetStringInfo(&buf);
                        appendStringInfo(&buf, "CREATE FOREIGN TABLE %s (\n",
                                         quote_identifier(table_name));
                        first_column = true;
                        first_table = false;
                    }

                    if (first_column)
                        first_column = false;
                    else
                        appendStringInfoString(&buf, ",\n");

                    /* Print column name */
                    appendStringInfo(&buf, "  %s",
                                     quote_identifier(column_name));

                    /* Print column type */

                    /* Numeric types */
                    if (strcmp(data_type, "bit") == 0 ||
                        strcmp(data_type, "smallint") == 0 ||
                        strcmp(data_type, "tinyint") == 0)
                        appendStringInfoString(&buf, " smallint");
                    else if (strcmp(data_type, "int") == 0)
                        appendStringInfoString(&buf, " integer");
                    else if (strcmp(data_type, "bigint") == 0)
                        appendStringInfoString(&buf, " bigint");
                    else if (strcmp(data_type, "decimal") == 0)
                    {
                        if (numeric_scale == 0)
                            appendStringInfo(&buf, " decimal(%d)", numeric_precision);
                        else
                            appendStringInfo(&buf, " decimal(%d, %d)",
                                             numeric_precision, numeric_scale);
                    }
                    else if (strcmp(data_type, "numeric") == 0)
                    {
                        if (numeric_scale == 0)
                            appendStringInfo(&buf, " numeric(%d)", numeric_precision);
                        else
                            appendStringInfo(&buf, " numeric(%d, %d)",
                                             numeric_precision, numeric_scale);
                    }
                    else if (strcmp(data_type, "money") == 0 ||
                             strcmp(data_type, "smallmoney") == 0)
                        appendStringInfoString(&buf, " money");

                    /* Floating-point types */
                    else if (strcmp(data_type, "float") == 0)
                        if (numeric_precision == 0)
                            appendStringInfoString(&buf, " float");
                        else
                            appendStringInfo(&buf, " float(%d)", numeric_precision);
                    else if (strcmp(data_type, "real") == 0)
                        appendStringInfoString(&buf, " real");

                    /* Date/type types */
                    else if (strcmp(data_type, "date") == 0)
                        appendStringInfoString(&buf, " date");
                    else if (strcmp(data_type, "datetime") == 0 ||
                             strcmp(data_type, "smalldatetime") == 0 ||
                             strcmp(data_type, "bigdatetime") == 0)
                        appendStringInfoString(&buf, " timestamp without time zone");
                    else if (strcmp(data_type, "time") == 0 ||
                             strcmp(data_type, "bigtime") == 0)
                        appendStringInfoString(&buf, " time");

                    /* Character types */
                    else if (strcmp(data_type, "char") == 0 ||
                             strcmp(data_type, "nchar") == 0 ||
                             strcmp(data_type, "unichar") == 0)
                        appendStringInfo(&buf, " char(%d)", char_len);
                    else if (strcmp(data_type, "varchar") == 0 ||
                             strcmp(data_type, "nvarchar") == 0 ||
                             strcmp(data_type, "univarchar") == 0)
                    {
                        if (char_len == -1)
                            appendStringInfoString(&buf, " text");
                        else
                            appendStringInfo(&buf, " varchar(%d)", char_len);
                    }
                    else if (strcmp(data_type, "text") == 0 ||
                             strcmp(data_type, "unitext") == 0)
                        appendStringInfoString(&buf, " text");

                    /* Binary types */
                    else if (strcmp(data_type, "binary") == 0 ||
                        strcmp(data_type, "varbinary") == 0 ||
                        strcmp(data_type, "image") == 0 ||
                        strcmp(data_type, "timestamp") == 0 )
                        appendStringInfoString(&buf, " bytea");

                    /* Other types */
                    else if (strcmp(data_type, "xml") == 0)
                        appendStringInfoString(&buf, " xml");
                    else if (keep_custom_types)
                    {
                        appendStringInfoString(&buf, " ");
                        appendStringInfoString(&buf, data_type);
                    }
                    else
                    {
                        ereport(DEBUG3,
                            (errmsg("tds_fdw: column \"%s\" of table \"%s\" has an untranslatable data type", column_name, table_name)
                            ));
                        appendStringInfoString(&buf, " text");
                    }

                    /*
                     * Add column_name option so that renaming the foreign table's
                     * column doesn't break the association to the underlying
                     * column.
                     */
                    appendStringInfoString(&buf, " OPTIONS (column_name ");
                    deparseStringLiteral(&buf, column_name);
                    appendStringInfoChar(&buf, ')');

                    /* Add DEFAULT if needed */
                    if (import_default && column_default[0] != '\0')
                    {
                        char *translated_default = translateSqlServerDefault(column_default);
                        if (translated_default != NULL)
                        {
                            appendStringInfo(&buf, " DEFAULT %s", translated_default);
                            pfree(translated_default);
                        }
                    }

                    /* Add NOT NULL if needed */
                    if (import_not_null && strcmp(is_nullable, "NO") == 0)
                        appendStringInfoString(&buf, " NOT NULL");

                    strcpy(prev_table, table_name);

                    break;

                case BUF_FULL:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                            errmsg("Buffer filled up while getting plan for query")
                        ));
                    break;

                case FAIL:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to get row while getting plan for query")
                        ));
                    break;

                default:
                    ereport(ERROR,
                        (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
                            errmsg("Failed to get plan for query. Unknown return code.")
                        ));
            }
        }

        /*
         * Add server name and table-level options.  We specify remote
         * schema and table name as options (the latter to ensure that
         * renaming the foreign table doesn't break the association).
         */
        appendStringInfo(&buf, "\n) SERVER %s\nOPTIONS (",
                         quote_identifier(stmt->server_name));

        appendStringInfoString(&buf, "schema_name ");
        deparseStringLiteral(&buf, stmt->remote_schema);
        appendStringInfoString(&buf, ", table_name ");
        deparseStringLiteral(&buf, prev_table);

        appendStringInfoString(&buf, ");");

        commands = lappend(commands, pstrdup(buf.data));
    }

    return commands;
}

List *tdsImportForeignSchema(ImportForeignSchemaStmt *stmt, Oid serverOid)
{
    TdsFdwOptionSet option_set;
    List       *commands = NIL;
    bool        import_default = false;
    bool        import_not_null = true;
    bool        keep_custom_types = false;
    ListCell   *lc;

    LOGINREC   *login;
    DBPROCESS  *dbproc;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsImportForeignSchema")
            ));
    #endif

    /* Parse statement options */
    foreach(lc, stmt->options)
    {
        DefElem    *def = (DefElem *) lfirst(lc);

        if (strcmp(def->defname, "import_default") == 0)
            import_default = defGetBoolean(def);
        else if (strcmp(def->defname, "import_not_null") == 0)
            import_not_null = defGetBoolean(def);
        else if (strcmp(def->defname, "keep_custom_types") == 0)
            keep_custom_types = defGetBoolean(def);
        else
            ereport(ERROR,
                    (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                     errmsg("invalid option \"%s\"", def->defname)));
    }

    tdsGetForeignServerOptionsFromCatalog(serverOid, &option_set);

    ereport(DEBUG3,
        (errmsg("tds_fdw: Initiating DB-Library")
        ));

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

    ereport(DEBUG3,
        (errmsg("tds_fdw: Getting login structure")
        ));

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

    if (tdsIsSqlServer(dbproc))
        commands = tdsImportSqlServerSchema(stmt, dbproc, option_set,
                                            import_default, import_not_null);
    else
        commands = tdsImportSybaseSchema(stmt, dbproc, option_set,
                                         import_default, import_not_null,
                                         keep_custom_types);

cleanup:
    dbclose(dbproc);
    dbloginfree(login);

cleanup_before_login:
    dbexit();

cleanup_before_init:
    ;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsImportForeignSchema")
            ));
    #endif

    return commands;
}
#endif  /* IMPORT_API */

char *tds_err_msg(int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
    StringInfoData buf;

    initStringInfo(&buf);
    appendStringInfo(
            &buf,
            "DB-Library error: DB #: %i, DB Msg: %s, OS #: %i, OS Msg: %s, Level: %i",
            dberr,
            dberrstr ? dberrstr : "",
            oserr,
            oserrstr ? oserrstr : "",
            severity
    );

    return buf.data;
}

int tds_err_capture(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
    last_error_message = tds_err_msg(severity, dberr, oserr, dberrstr, oserrstr);

    return INT_CANCEL;
}

int tds_err_handler(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tds_err_handler")
            ));
    #endif

    /* Character set conversions should be non-fatal */
    if (dberr == 2403)
    {
        ereport(NOTICE,
            (errmsg("%s", tds_err_msg(severity, dberr, oserr, dberrstr, oserrstr))
            )); 

        return INT_CONTINUE;
    }
    else
    {
        ereport(ERROR,
            (errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
            errmsg("%s", tds_err_msg(severity, dberr, oserr, dberrstr, oserrstr))
            )); 
    
        return INT_CANCEL;
    }
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

void tds_signal_handler(int signum)
{
    interrupt_flag = true;
}

void tds_clear_signals()
{
    interrupt_flag = false;
}

int tds_chkintr_func(void *vdbproc)
{
    int status = FALSE;
    if(interrupt_flag)
    {
        status = TRUE;
    }
    return status;
}

int tds_hndlintr_func(void *vdbproc)
{
    tds_clear_signals();
    return INT_CANCEL;
}
