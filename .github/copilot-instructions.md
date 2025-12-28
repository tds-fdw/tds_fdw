# tds_fdw AI Coding Instructions

## Project Overview

tds_fdw is a PostgreSQL Foreign Data Wrapper (FDW) for databases using the Tabular Data Stream (TDS) protocol (Microsoft SQL Server and Sybase). It bridges PostgreSQL's FDW API with FreeTDS's DB-Library interface.

**Current Limitations**: No JOIN push-down support. Write operations (INSERT/UPDATE/DELETE) are **experimental**.

**Note**: The README still states "does not yet support...write operations" but this branch has experimental implementation.

## Architecture

### Core Components

- **[src/tds_fdw.c](../src/tds_fdw.c)** (5027 lines): Main FDW handler implementing PostgreSQL's `FdwRoutine` callbacks (read: `GetForeignRelSize`, `BeginForeignScan`, `IterateForeignScan`; write: `IsForeignRelUpdatable`, `PlanForeignModify`, `BeginForeignModify`, `ExecForeignInsert`, `ExecForeignUpdate`, `ExecForeignDelete`, `EndForeignModify`) and DB-Library connection management
- **[src/deparse.c](../src/deparse.c)** (2529 lines): Query deparsing logic adapted from postgres_fdw - converts PostgreSQL query plans to TDS-compatible SQL with WHERE/column pushdown support, plus direct SQL generation for INSERT/UPDATE/DELETE via `deparseDirectInsertSql`, `deparseDirectUpdateSql`, `deparseDirectDeleteSql`, and `datumToTdsString` for value conversion
- **[src/options.c](../src/options.c)** (1072 lines): FDW option validation and extraction for servers, tables, and user mappings
- **[include/tds_fdw.h](../include/tds_fdw.h)** (273 lines): Key data structures: `TdsFdwExecutionState` (scan state with DBPROCESS handle), `TdsFdwModifyState` (INSERT/UPDATE/DELETE state with target_attrs, key_attrs, temp_cxt), `TdsFdwRelationInfo` (query planning metadata for optimizer)

### Data Flow

**Read Operations:**
1. PostgreSQL planner calls `tdsGetForeignRelSize` → `tdsGetForeignPaths` → `tdsGetForeignPlan`
2. Deparse module converts WHERE clauses to TDS SQL (pushdown when `match_column_names` enabled)
3. Executor calls `tdsBeginForeignScan` → establishes DB-Library connection via `dblogin()`/`dbopen()`
4. `tdsIterateForeignScan` fetches rows using `dbnextrow()`, converts DB-Library types to PostgreSQL datums with direct binding for numeric types
5. `tdsEndForeignScan` cleans up connection with `dbclose()`/`dbloginfree()`/`dbexit()`

**Write Operations (Experimental):**
1. PostgreSQL planner calls `IsForeignRelUpdatable` (returns bitmask: CMD_INSERT | CMD_UPDATE | CMD_DELETE)
2. `tdsPlanForeignModify` identifies target columns and key columns (via `tdsGetKeyAttrs` - uses primary key or all columns as fallback)
3. `tdsBeginForeignModify` establishes DB-Library connection, allocates `TdsFdwModifyState` with temp context
4. For each row:
   - `tdsExecForeignInsert`: calls `deparseDirectInsertSql` → builds INSERT with inline values via `datumToTdsString`
   - `tdsExecForeignUpdate`: calls `deparseDirectUpdateSql` → builds UPDATE with SET clause and WHERE using key columns
   - `tdsExecForeignDelete`: calls `deparseDirectDeleteSql` → builds DELETE with WHERE using key columns
   - All execute via `dbcmd()`/`dbsqlexec()`/`dbresults()` cycle with proper error handling
5. `tdsEndForeignModify` cleans up connection and temp context

## PostgreSQL Version Compatibility

Code uses version-conditional compilation extensively:
```c
#if PG_VERSION_NUM >= 120000
    #include "access/table.h"
#else
    #include "access/heapam.h"
#endif
```
Supports PostgreSQL 9.2 through 18. Check `#if PG_VERSION_NUM` guards when modifying core functions.

## Build System (PGXS)

Uses PostgreSQL Extension Building Infrastructure (PGXS):
- **[Makefile](../Makefile)**: Standard PGXS pattern with `PG_CONFIG = pg_config` and `include $(PGXS)`
- Build: `make` (requires `pg_config` in PATH and FreeTDS installed)
- Install: `make install` (copies .so and .sql files to PostgreSQL directories)
- Extension version: Extracted from `tds_fdw.control` (`default_version`)
- Migration scripts: `tds_fdw--X.Y.Z--X.Y.Z+1.sql` for version upgrades

### FreeTDS Integration

Links against FreeTDS's libsybdb (`SHLIB_LINK := -lsybdb`). Key dependencies:
- Headers: `<sybfront.h>`, `<sybdb.h>`
- Connection: `LOGINREC`, `DBPROCESS` structs
- Data types: `DBINT`, `DBFLT8`, `DBBIGINT`, etc.
- See README.md for TDS version and encoding configuration via `freetds.conf`

## Testing Framework

Python-based test suite with JSON-driven tests:

### Test Structure
- **[tests/mssql-tests.py](../tests/mssql-tests.py)**: Runs tests against MSSQL/Azure using `pymssql` library
- **[tests/postgresql-tests.py](../tests/postgresql-tests.py)**: Runs tests against PostgreSQL using `psycopg2` library  
- **Test format**: Each test = `.json` (metadata) + `.sql` (queries) pair in `tests/tests/{mssql,postgresql}/`
- **JSON schema**: `{"test_desc": "...", "server": {"version": {"min": "X.Y.Z", "max": "X.Y.Z"}}}`

### Running Tests
```bash
# Setup MSSQL test data
./tests/mssql-tests.py --server HOST --port 1433 --database DB --schema tds_fdw_tests --username USER --password PASS

# Run PostgreSQL tests
./tests/postgresql-tests.py --postgres_server HOST --postgres_port 5432 --postgres_database DB \
    --postgres_schema pg_tests --postgres_username USER --postgres_password PASS \
    --mssql_server HOST --mssql_port 1433 --mssql_database DB --mssql_schema tds_fdw_tests \
    --mssql_username USER --mssql_password PASS

# Debug mode (attach gdb)
./tests/postgresql-tests.py --debugging [options]  # Prints backend PID, pauses for gdb attach
```

### Variable Substitution in SQL
Tests use placeholders replaced at runtime: `@PSCHEMANAME`, `@MSERVER`, `@MPORT`, `@MUSER`, `@MPASSWORD`, `@MDATABASE`, `@MSCHEMANAME`, `@TDSVERSION` (see [tests/README.md](../tests/README.md))

## Debugging Practices

### Enable Verbose Logging
```sql
SET client_min_messages TO DEBUG3;
ALTER SERVER myserver OPTIONS (SET msg_handler 'notice');
```

### Memory Tracking Variables
Project has custom GUCs for debugging memory usage:
- `tds_fdw.show_before_row_memory_stats`
- `tds_fdw.show_after_row_memory_stats`
- `tds_fdw.show_finished_memory_stats`

### Symbol Visibility
Uses `-fvisibility=hidden` and `visibility.h` macros to control exported symbols. Check `PGDLLEXPORT` usage when adding new functions.

## Code Conventions

- **Error handling**: Use PostgreSQL's `ereport(ERROR, ...)` for errors, never direct exits
- **Memory management**: Use PostgreSQL memory contexts (`MemoryContext`), not malloc/free
- **Naming**: Prefix functions with `tds` (e.g., `tdsGetForeignRelSize`, `tdsOptionSetInit`)
- **DB-Library cleanup**: Always pair `dbopen()` with `dbclose()`, check return codes with `dbresults()` loop
- **Type conversions**: See `COL` struct and type-specific logic in `tdsIterateForeignScan()` for DB-Library → PostgreSQL datum conversions

## Key Limitations (Document Changes)

- **No JOIN push-down support** - queries joining foreign tables execute locally in PostgreSQL
- **Write operations are experimental** - INSERT/UPDATE/DELETE implemented but not thoroughly tested:
  - UPDATE/DELETE require primary key or use all columns in WHERE clause (via `tdsGetKeyAttrs`)
  - Values are inline in SQL (no prepared statement parameters) - potential SQL injection risk with user-provided data
  - `datumToTdsString` converts PostgreSQL Datums to TDS-compatible SQL literals
  - No RETURNING clause support despite structure field
  - Transaction handling relies on PostgreSQL's transaction management
- Column/WHERE pushdown requires `match_column_names` option enabled
- Character encoding depends on FreeTDS `freetds.conf` settings (`client charset`, `tds version`)
- ANSI mode support via `sqlserver_ansi_mode` option (SQL Server only)
