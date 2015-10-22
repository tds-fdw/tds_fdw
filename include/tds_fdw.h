
#ifndef TDS_FDW_H
#define TDS_FDW_H

/* postgres headers */

#include "postgres.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"

#if (PG_VERSION_NUM >= 90200)
#include "optimizer/pathnode.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/planmain.h"
#endif

/* DB-Library headers (e.g. FreeTDS) */
#include <sybfront.h>
#include <sybdb.h>

#include "options.h"

/* a column */

typedef union COL_VALUE
{
	DBSMALLINT dbsmallint;
	DBINT dbint;
	DBBIGINT dbbigint;
	DBREAL dbreal;
	DBFLT8 dbflt8;
} COL_VALUE;

typedef struct COL
{
	char *name;
	int srctype;
	bool useraw;
	COL_VALUE value;
} COL;

/* this maintains state */

typedef struct TdsFdwExecutionState
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	AttInMetadata *attinmeta;
	char *query;
	int first;
	COL *columns;
	Datum *datums;
	bool *isnull;
	int ncols;
	int row;
	MemoryContext mem_cxt;
} TdsFdwExecutionState;

/* functions called via SQL */

extern Datum tds_fdw_handler(PG_FUNCTION_ARGS);
extern Datum tds_fdw_validator(PG_FUNCTION_ARGS);

/* FDW callback routines */

void tdsExplainForeignScan(ForeignScanState *node, ExplainState *es);
void tdsBeginForeignScan(ForeignScanState *node, int eflags);
TupleTableSlot* tdsIterateForeignScan(ForeignScanState *node);
void tdsReScanForeignScan(ForeignScanState *node);
void tdsEndForeignScan(ForeignScanState *node);

/* routines for 9.2.0+ */
#if (PG_VERSION_NUM >= 90200)
void tdsGetForeignRelSize(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
void tdsEstimateCosts(PlannerInfo *root, RelOptInfo *baserel, Cost *startup_cost, Cost *total_cost, Oid foreigntableid);
void tdsGetForeignPaths(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid);
bool tdsAnalyzeForeignTable(Relation relation, AcquireSampleRowsFunc *func, BlockNumber *totalpages);
ForeignScan* tdsGetForeignPlan(PlannerInfo *root, RelOptInfo *baserel, Oid foreigntableid, ForeignPath *best_path, List *tlist, List *scan_clauses);
/* routines for versions older than 9.2.0 */
#else
FdwPlan* tdsPlanForeignScan(Oid foreigntableid, PlannerInfo *root, RelOptInfo *baserel);
#endif

/* Helper functions */

int tdsSetupConnection(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS **dbproc);
double tdsGetRowCount(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc);
double tdsGetRowCountShowPlanAll(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc);
double tdsGetRowCountExecute(TdsFdwOptionSet* option_set, LOGINREC *login, DBPROCESS *dbproc);
double tdsGetStartupCost(TdsFdwOptionSet* option_set);
void tdsGetColumnMetadata(ForeignScanState *node);
char* tdsConvertToCString(DBPROCESS* dbproc, int srctype, const BYTE* src, DBINT srclen);
#if (PG_VERSION_NUM >= 90400)
int tdsDatetimeToDatum(DBPROCESS *dbproc, DBDATETIME *src, Datum *datetime_out);
#endif

/* Helper functions for DB-Library API */

int tds_err_handler(DBPROCESS *dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
int tds_notice_msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *svr_name, char *proc_name, int line);
int tds_blackhole_msg_handler(DBPROCESS *dbproc, DBINT msgno, int msgstate, int severity, char *msgtext, char *svr_name, char *proc_name, int line);

#endif
