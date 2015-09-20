# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/Variables.md

## Variables

### Available Variables

* *tds_fdw.show_before_row_memory_stats* - print memory context stats to the PostgreSQL log before each row is fetched.

* *tds_fdw.show_after_row_memory_stats* - print memory context stats to the PostgreSQL log after each row is fetched.

* *tds_fdw.show_finished_memory_stats* - print memory context stats to the PostgreSQL log when a query is finished.

### Setting Variables

To set a variable, use the [SET command](http://www.postgresql.org/docs/9.4/static/sql-set.html). i.e.:

```
postgres=# SET tds_fdw.show_finished_memory_stats=1;
SET
```