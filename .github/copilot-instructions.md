# tds_fdw AI Coding Instructions

## Project Overview

tds_fdw is a PostgreSQL Foreign Data Wrapper (FDW) for **read-only** access to databases using the Tabular Data Stream (TDS) protocol (Microsoft SQL Server and Sybase). It bridges PostgreSQL's FDW API with FreeTDS's DB-Library interface.

**Current Limitations**: No JOIN push-down support, no write operations (INSERT/UPDATE/DELETE).

## Architecture

### Core Components

- **[src/tds_fdw.c](../src/tds_fdw.c)** (4267 lines): Main FDW handler implementing PostgreSQL's `FdwRoutine` callbacks (`GetForeignRelSize`, `BeginForeignScan`, `IterateForeignScan`, etc.) and DB-Library connection management
- **[src/deparse.c](../src/deparse.c)** (2529 lines): Query deparsing logic adapted from postgres_fdw - converts PostgreSQL query plans to TDS-compatible SQL with WHERE/column pushdown support
- **[src/options.c](../src/options.c)** (1072 lines): FDW option validation and extraction for servers, tables, and user mappings
- **[include/ (Read Operations Only)

1. PostgreSQL planner calls `tdsGetForeignRelSize` → `tdsGetForeignPaths` → `tdsGetForeignPlan`
2. Deparse module converts WHERE clauses to TDS SQL (pushdown when `match_column_names` enabled)
3. Executor calls `tdsBeginForeignScan` → establishes DB-Library connection via `dblogin()`/`dbopen()`
4. `tdsIterateForeignScan` fetches rows using `dbnextrow()`, converts DB-Library types to PostgreSQL datums with direct binding for numeric types
5. `tdsEndForeignScan` cleans up connection with `dbclose()`/`dbloginfree()`/`dbexitLibrary connection via `dblogin()`/`dbopen()`
4. `tdsIterateForeignScan` fetches rows using `dbnextrow()`, converts DB-Library types to PostgreSQL datums
5. `tdsEndForeignScan` cleans up connection with `dbclose()`

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
- **No write operations** - INSERT/UPDATE/DELETE not supported in this branch
- Column/WHERE pushdown requires `match_column_names` option enabled
- Character encoding depends on FreeTDS `freetds.conf` settings (`client charset`, `tds version`)
- ANSI mode support via `sqlserver_ansi_mode` option (SQL Server only)
