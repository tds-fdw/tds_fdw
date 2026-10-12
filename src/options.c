
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

#include "tds_fdw.h"
#include "options.h"

void tdsGetForeignServerOptions(List *options_list, TdsFdwOptionSet *option_set);
void tdsGetForeignServerTableOptions(List *options_list, TdsFdwOptionSet *option_set);
void tdsGetForeignTableOptions(List *options_list, TdsFdwOptionSet *option_set);
void tdsGetUserMappingOptions(List *options_list, TdsFdwOptionSet *option_set);

void tdsValidateForeignTableOptionSet(TdsFdwOptionSet *option_set);

void tdsSetDefaultOptions(TdsFdwOptionSet *option_set);

bool tdsIsValidOption(const char *option, Oid context);
void tdsOptionSetInit(TdsFdwOptionSet* option_set);

/* these are valid options */

static struct TdsFdwOption valid_options[] =
{
    { "servername",             ForeignServerRelationId },
    { "language",               ForeignServerRelationId },
    { "character_set",          ForeignServerRelationId },
    { "port",                   ForeignServerRelationId },
    { "database",               ForeignServerRelationId },
    { "dbuse",                  ForeignServerRelationId },
    { "sqlserver_ansi_mode",    ForeignServerRelationId },
    { "tds_version",            ForeignServerRelationId },
    { "msg_handler",            ForeignServerRelationId },
    { "row_estimate_method",    ForeignServerRelationId },
    { "use_remote_estimate",    ForeignServerRelationId },
    { "fdw_startup_cost",       ForeignServerRelationId },
    { "fdw_tuple_cost",         ForeignServerRelationId },
    { "username",               UserMappingRelationId },
    { "password",               UserMappingRelationId },
    { "query",                  ForeignTableRelationId },
    { "table",                  ForeignTableRelationId },
    { "schema_name",            ForeignTableRelationId },
    { "table_name",             ForeignTableRelationId },
    { "row_estimate_method",    ForeignTableRelationId },
    { "match_column_names",     ForeignTableRelationId },
    { "use_remote_estimate",    ForeignTableRelationId },
    { "local_tuple_estimate",   ForeignTableRelationId },
    { "column_name",            AttributeRelationId },
    { NULL,                     InvalidOid }
};


/* default IP address */

static const char *DEFAULT_SERVERNAME = "127.0.0.1";

/* default method to use to estimate rows in results */

static const char *DEFAULT_ROW_ESTIMATE_METHOD = "execute";

/* default function used to handle TDS messages */

static const char *DEFAULT_MSG_HANDLER = "blackhole";

/* whether to match on column names by default. if not, we use column order. */

static const int DEFAULT_MATCH_COLUMN_NAMES = 1;

/* by default we use remote estimates */

static const int DEFAULT_USE_REMOTE_ESTIMATE = 1;

/* by default we use remote estimates */

static const int DEFAULT_FDW_STARTUP_COST = 100;

/* by default we use remote estimates */

static const int DEFAULT_FDW_TUPLE_COST = 100;

/* conservative default tuple count */

static const int DEFAULT_LOCAL_TUPLE_ESTIMATE = 1000;

void tdsValidateOptions(List *options_list, Oid context, TdsFdwOptionSet* option_set)
{
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsValidateOptions")
            ));
    #endif
    
    tdsOptionSetInit(option_set);
    
    if (context == ForeignServerRelationId)
    {
        tdsGetForeignServerOptions(options_list, option_set);
        tdsGetForeignServerTableOptions(options_list, option_set);
    }
    
    else if (context == ForeignTableRelationId)
    {
        tdsGetForeignTableOptions(options_list, option_set);
        tdsValidateForeignTableOptionSet(option_set);
    }
    
    else if (context == UserMappingRelationId)
    {
        tdsGetUserMappingOptions(options_list, option_set);
    }
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsValidateOptions")
            ));
    #endif
}

/* get options for FOREIGN SERVER objects using this module */

void tdsGetForeignServerOptionsFromCatalog(Oid foreignserverid, TdsFdwOptionSet* option_set)
{
    ForeignServer *f_server;
    UserMapping *f_mapping;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetForeignServerOptionsFromCatalog")
            ));
    #endif

    tdsOptionSetInit(option_set);

    f_server = GetForeignServer(foreignserverid);
    f_mapping = GetUserMapping(GetUserId(), foreignserverid);

    tdsGetForeignServerOptions(f_server->options, option_set);
    tdsGetForeignServerTableOptions(f_server->options, option_set);

    tdsGetUserMappingOptions(f_mapping->options, option_set);

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsGetForeignTableOptionsFromCatalog")
            ));
    #endif
}

/* get options for FOREIGN TABLE and FOREIGN SERVER objects using this module */

void tdsGetForeignTableOptionsFromCatalog(Oid foreigntableid, TdsFdwOptionSet* option_set)
{
    ForeignTable *f_table;
    ForeignServer *f_server;
    UserMapping *f_mapping;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetForeignTableOptionsFromCatalog")
            ));
    #endif
    
    tdsOptionSetInit(option_set);
    
    f_table = GetForeignTable(foreigntableid);
    f_server = GetForeignServer(f_table->serverid);
    f_mapping = GetUserMapping(GetUserId(), f_table->serverid);
    
    tdsGetForeignServerOptions(f_server->options, option_set);
    tdsGetForeignServerTableOptions(f_server->options, option_set);
    
    tdsGetForeignTableOptions(f_table->options, option_set);
    
    tdsGetUserMappingOptions(f_mapping->options, option_set);

    tdsValidateOptionSet(option_set);
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsGetForeignTableOptionsFromCatalog")
            ));
    #endif
}

void tdsGetForeignServerOptions(List *options_list, TdsFdwOptionSet *option_set)
{
    ListCell *cell;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetForeignServerOptions")
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
        
        if (!tdsIsValidOption(def->defname, ForeignServerRelationId))
        {
            TdsFdwOption *opt;
            StringInfoData buf;
            
            initStringInfo(&buf);
            for (opt = valid_options; opt->optname; opt++)
            {
                if (ForeignServerRelationId == opt->optcontext)
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
            option_set->servername = defGetString(def); 
        }
        
        else if (strcmp(def->defname, "language") == 0)
        {
            option_set->language = defGetString(def);
        }
        
        else if (strcmp(def->defname, "character_set") == 0)
        {
            option_set->character_set = defGetString(def);
        }
        
        else if (strcmp(def->defname, "port") == 0)
        {
            if (IsA(def->arg, Integer)) 
                option_set->port = defGetInt32(def); 
            else
                option_set->port = atoi(defGetString(def));
        }
        
        else if (strcmp(def->defname, "database") == 0)
        {
            option_set->database = defGetString(def);
        }   

        else if (strcmp(def->defname, "dbuse") == 0)
        {
            if (IsA(def->arg, Integer))
                option_set->dbuse = defGetBoolean(def);
            else
                parse_bool(defGetString(def), &option_set->dbuse);
        }   

        else if(strcmp(def->defname, "sqlserver_ansi_mode") == 0)
        {
            if (IsA(def->arg, Integer))
                option_set->sqlserver_ansi_mode = defGetBoolean(def);
            else
                parse_bool(defGetString(def), &option_set->sqlserver_ansi_mode);
        }

        else if (strcmp(def->defname, "tds_version") == 0)
        {
            int tds_version_test = 0;
            
            option_set->tds_version = defGetString(def);
            
            if (strcmp(option_set->tds_version, "4.2") == 0)
            {
                tds_version_test = 1;
            }
            
            else if (strcmp(option_set->tds_version, "5.0") == 0)
            {
                tds_version_test = 1;
            }
            
            else if (strcmp(option_set->tds_version, "7.0") == 0)
            {
                tds_version_test = 1;
            }
            
            #ifdef DBVERSION_71
            else if (strcmp(option_set->tds_version, "7.1") == 0)
            {
                tds_version_test = 1;
            }
            #endif
            
            #ifdef DBVERSION_72
            else if (strcmp(option_set->tds_version, "7.2") == 0)
            {
                tds_version_test = 1;
            }
            #endif

            #ifdef DBVERSION_73
            else if (strcmp(option_set->tds_version, "7.3") == 0)
            {
                tds_version_test = 1;
            }
            #endif

            #ifdef DBVERSION_74
            else if (strcmp(option_set->tds_version, "7.4") == 0)
            {
                    tds_version_test = 1;
            }
            #endif
            
            if (!tds_version_test)
            {
                ereport(ERROR,
                    (errcode(ERRCODE_SYNTAX_ERROR),
                        errmsg("Unknown tds version: %s.", option_set->tds_version)
                    ));
            }
        }

        else if (strcmp(def->defname, "msg_handler") == 0)
        {
            int msg_handler_test = 0;
            
            option_set->msg_handler = defGetString(def);

            if (strcmp(option_set->msg_handler, "notice") == 0)
            {
                msg_handler_test = 1;
            }
            
            else if (strcmp(option_set->msg_handler, "blackhole") == 0)
            {
                msg_handler_test = 1;
            }   

            if (!msg_handler_test)
            {
                ereport(ERROR,
                    (errcode(ERRCODE_SYNTAX_ERROR),
                        errmsg("Unknown msg handler: %s.", option_set->msg_handler)
                    ));
            }           
        }
        
        else if (strcmp(def->defname, "fdw_startup_cost") == 0)
        {
            if (IsA(def->arg, Integer))
                option_set->fdw_startup_cost = defGetInt64(def); 
            else
                option_set->fdw_startup_cost = atoi(defGetString(def));
        }
        
        else if (strcmp(def->defname, "fdw_tuple_cost") == 0)
        {
            if (IsA(def->arg, Integer))
                option_set->fdw_tuple_cost = defGetInt64(def);
            else
                option_set->fdw_tuple_cost = atoi(defGetString(def));
        }
    }
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsGetForeignServerOptions")
            ));
    #endif
}

void tdsGetForeignServerTableOptions(List *options_list, TdsFdwOptionSet *option_set)
{
    ListCell *cell;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetForeignServerTableOptions")
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
        
        if (!tdsIsValidOption(def->defname, ForeignServerRelationId))
        {
            TdsFdwOption *opt;
            StringInfoData buf;
            
            initStringInfo(&buf);
            for (opt = valid_options; opt->optname; opt++)
            {
                if (ForeignServerRelationId == opt->optcontext)
                    appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "", opt->optname);
            }
            
            ereport(ERROR,
                (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                    errmsg("Invalid option \"%s\"", def->defname),
                    errhint("Valid options in this context are: %s", buf.len ? buf.data : "<none>")
                ));
        }
    
        if (strcmp(def->defname, "row_estimate_method") == 0)
        {   
            option_set->row_estimate_method = defGetString(def);
            
            if ((strcmp(option_set->row_estimate_method, "execute") != 0)
                && (strcmp(option_set->row_estimate_method, "showplan_all") != 0))
            {
                ereport(ERROR,
                    (errcode(ERRCODE_SYNTAX_ERROR),
                        errmsg("row_estimate_method should be set to \"execute\" or \"showplan_all\". Currently set to %s", option_set->row_estimate_method)
                    ));
            }
        }
        
        else if (strcmp(def->defname, "use_remote_estimate") == 0)
        {
            if (IsA(def->arg, Integer))
                option_set->use_remote_estimate = defGetBoolean(def);
            else
                parse_bool(defGetString(def), &option_set->use_remote_estimate);
        }
    }
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsGetForeignServerTableOptions")
            ));
    #endif
}

void tdsGetForeignTableOptions(List *options_list, TdsFdwOptionSet *option_set)
{
    ListCell *cell;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetForeignTableOptions")
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
        
        if (!tdsIsValidOption(def->defname, ForeignTableRelationId))
        {
            TdsFdwOption *opt;
            StringInfoData buf;
            
            initStringInfo(&buf);
            for (opt = valid_options; opt->optname; opt++)
            {
                if (ForeignTableRelationId == opt->optcontext)
                    appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "", opt->optname);
            }
            
            ereport(ERROR,
                (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                    errmsg("Invalid option \"%s\"", def->defname),
                    errhint("Valid options in this context are: %s", buf.len ? buf.data : "<none>")
                ));
        }
        
        if (strcmp(def->defname, "query") == 0)
        {           
            option_set->query = defGetString(def);
        }
        
        else if (strcmp(def->defname, "schema_name") == 0)
        {           
            option_set->schema_name = defGetString(def);
        }
        
        else if (strcmp(def->defname, "table") == 0
            || strcmp(def->defname, "table_name") == 0)
        {           
            option_set->table_name = defGetString(def);
        }
    
        else if (strcmp(def->defname, "row_estimate_method") == 0)
        {   
            option_set->row_estimate_method = defGetString(def);
            
            if ((strcmp(option_set->row_estimate_method, "execute") != 0)
                && (strcmp(option_set->row_estimate_method, "showplan_all") != 0))
            {
                ereport(ERROR,
                    (errcode(ERRCODE_SYNTAX_ERROR),
                        errmsg("row_estimate_method should be set to \"execute\" or \"showplan_all\". Currently set to %s", option_set->row_estimate_method)
                    ));
            }
        }

        else if (strcmp(def->defname, "match_column_names") == 0)
        {      
            if (IsA(def->arg, Integer))
                option_set->match_column_names = defGetBoolean(def);
            else
                parse_bool(defGetString(def), &option_set->match_column_names);
        }
        
        else if (strcmp(def->defname, "use_remote_estimate") == 0)
        {
            if (IsA(def->arg, Integer))
                option_set->use_remote_estimate = defGetBoolean(def);
            else
                parse_bool(defGetString(def), &option_set->use_remote_estimate);
        }
        
        else if (strcmp(def->defname, "local_tuple_estimate") == 0)
        {
            if (IsA(def->arg, Integer)) 
                option_set->local_tuple_estimate = defGetInt64(def);
            else
                option_set->local_tuple_estimate = atoi(defGetString(def));
        }
    }
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsGetForeignTableOptions")
            ));
    #endif
}

void tdsGetUserMappingOptions(List *options_list, TdsFdwOptionSet *option_set)
{
    ListCell *cell;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsGetUserMappingOptions")
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
        
        if (!tdsIsValidOption(def->defname, UserMappingRelationId))
        {
            TdsFdwOption *opt;
            StringInfoData buf;
            
            initStringInfo(&buf);
            for (opt = valid_options; opt->optname; opt++)
            {
                if (UserMappingRelationId == opt->optcontext)
                    appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "", opt->optname);
            }
            
            ereport(ERROR,
                (errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
                    errmsg("Invalid option \"%s\"", def->defname),
                    errhint("Valid options in this context are: %s", buf.len ? buf.data : "<none>")
                ));
        }

        if (strcmp(def->defname, "username") == 0)
        {
            option_set->username = defGetString(def);
        }
        
        else if (strcmp(def->defname, "password") == 0)
        {
            option_set->password = defGetString(def);
        }
    }
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsGetUserMappingOptions")
            ));
    #endif
}

void tdsSetDefaultOptions(TdsFdwOptionSet *option_set)
{
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsSetDefaultOptions")
            ));
    #endif
    
    if ((option_set->servername = palloc((strlen(DEFAULT_SERVERNAME) + 1) * sizeof(char))) == NULL)
        {
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                                errmsg("Failed to allocate memory for server name")
                        ));
        }

    sprintf(option_set->servername, "%s", DEFAULT_SERVERNAME);
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("Set servername to default: %s", option_set->servername)
            ));
    #endif

    if ((option_set->row_estimate_method = palloc((strlen(DEFAULT_ROW_ESTIMATE_METHOD) + 1) * sizeof(char))) == NULL)
        {
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                                errmsg("Failed to allocate memory for row estimate method")
                        ));
        }

    sprintf(option_set->row_estimate_method, "%s", DEFAULT_ROW_ESTIMATE_METHOD);

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("Set row_estimate_method to default: %s", option_set->row_estimate_method)
            ));
    #endif

    if ((option_set->msg_handler= palloc((strlen(DEFAULT_MSG_HANDLER) + 1) * sizeof(char))) == NULL)
        {
                ereport(ERROR,
                        (errcode(ERRCODE_FDW_OUT_OF_MEMORY),
                                errmsg("Failed to allocate memory for msg handler")
                        ));
        }

    sprintf(option_set->msg_handler, "%s", DEFAULT_MSG_HANDLER);

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("Set msg_handler to default: %s", option_set->msg_handler)
            ));
    #endif

    option_set->use_remote_estimate = DEFAULT_USE_REMOTE_ESTIMATE;
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("Set use_remote_estimate to default: %d", option_set->use_remote_estimate)
            ));
    #endif  

    option_set->local_tuple_estimate = DEFAULT_LOCAL_TUPLE_ESTIMATE;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("Set local_tuple_estimate to default: %d", option_set->local_tuple_estimate)
            ));
    #endif  

    option_set->fdw_startup_cost = DEFAULT_FDW_STARTUP_COST;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("Set fdw_startup_cost to default: %d", option_set->fdw_startup_cost)
            ));
    #endif  

    option_set->fdw_tuple_cost = DEFAULT_FDW_TUPLE_COST;
   
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("Set fdw_tuple_cost to default: %d", option_set->fdw_tuple_cost)
            ));
    #endif  

    option_set->match_column_names = DEFAULT_MATCH_COLUMN_NAMES;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsSetDefaultOptions")
            ));
    #endif  
}

void tdsValidateOptionSet(TdsFdwOptionSet *option_set)
{
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsValidateOptionSet")
            ));
    #endif
    
    tdsValidateForeignTableOptionSet(option_set);
    
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsValidateOptionSet")
            ));
    #endif
}

void tdsValidateForeignTableOptionSet(TdsFdwOptionSet *option_set)
{
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsValidateForeignTableOptionSet")
            ));
    #endif
    
    /* Check conflicting options */
    
    if (option_set->table_name && option_set->query)
    {
        ereport(ERROR,
            (errcode(ERRCODE_SYNTAX_ERROR),
                errmsg("Conflicting options: table and query options can't be used together.")
            ));
    }
    
    /* Check required options */
    
    if (!option_set->table_name && !option_set->query)
    {
        ereport(ERROR,
            (errcode(ERRCODE_SYNTAX_ERROR),
                errmsg("Required options: either a table or a query must be specified")
            ));
    }

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsValidateForeignTableOptionSet")
            ));
    #endif
}

/* validate options for FOREIGN TABLE and FOREIGN SERVER objects using this module */

bool tdsIsValidOption(const char *option, Oid context)
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

/* initialize the option set */

void tdsOptionSetInit(TdsFdwOptionSet* option_set)
{
    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> starting tdsOptionSetInit")
            ));
    #endif

    tdsSetDefaultOptions(option_set);

    option_set->language = NULL;
    option_set->character_set = NULL;
    option_set->port = 0;
    option_set->database = NULL;
    option_set->dbuse = 0;
    option_set->sqlserver_ansi_mode = false;
    option_set->tds_version = NULL;
    option_set->username = NULL;
    option_set->password = NULL;
    option_set->query = NULL;
    option_set->schema_name = NULL;
    option_set->table_name = NULL;

    #ifdef DEBUG
        ereport(NOTICE,
            (errmsg("----> finishing tdsOptionSetInit")
            ));
    #endif  
}

