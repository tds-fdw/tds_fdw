# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/ForeignTableCreation.md

## Creating a Foreign Table

### Options

#### Foreign table parameters accepted:
				
* *query*  
  
Required: Yes (mutually exclusive with *table*)  
  
The query string to use to query the foreign table.

* *schema_name*

Required: No

The schema that the table is in. The schema name can also be included in *table_name*, so this is not required.

* *table_name*

Aliases: *table*  
  
Required: Yes (mutually exclusive with *query*)  
  
The table on the foreign server to query.

* *match_column_names*

Required: No

Whether to match local columns with remote columns by comparing their table names or whether to use the order that they appear in the result set.

* *use_remote_estimate*

Required: No

Whether we estimate the size of the table by performing some operation on the remote server (as defined by *row_estimate_method*), or whether we just use a local estimate, as defined by *local_tuple_estimate*.

* *local_tuple_estimate*

Required: No

A locally set estimate of the number of tuples that is used when *use_remote_estimate* is disabled.

* *row_estimate_method*

Required: No

Default: `execute`

This can be one of the following values:

* `execute`: Execute the query on the remote server, and get the actual number of rows in the query.
* `showplan_all`: This gets the estimated number of rows using [MS SQL Server's SET SHOWPLAN_ALL](https://msdn.microsoft.com/en-us/library/ms187735.aspx).

#### Foreign table column parameters accepted:

* *column_name*

Required: No

The name of the column on the remote server. If this is not set, the column's remote name is assumed to be the same as the column's local name. If match_column_names is set to 0 for the table, then column names are not used at all, so this is ignored.

### Example

Using a *table_name* definition:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (table_name 'dbo.mytable', row_estimate_method 'showplan_all');
```

Or using a *schema_name* and *table_name* definition:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (schema_name 'dbo', table_name 'mytable', row_estimate_method 'showplan_all');
```
	
Or using a *query* definition:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (query 'SELECT * FROM dbo.mytable', row_estimate_method 'showplan_all');
```

Or setting a remote column name:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	col2 varchar OPTIONS (column_name 'data'))
	SERVER mssql_svr
	OPTIONS (schema_name 'dbo', table_name 'mytable', row_estimate_method 'showplan_all');
```
