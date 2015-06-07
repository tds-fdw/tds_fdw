# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/ForeignTableCreation.md

## Creating a Foreign Table

### Options

Foreign table parameters accepted:

* *database*  
  
Required: No  
  
The database name that the foreign table is a part of.

If specified, the foreign tables database is always selected with *dbuse()* after connecting. Do not use this with Azure.

This option might eventually be phased out and replaced with the *database* option supplied to the foreign server.
				
* *query*  
  
Required: Yes (mutually exclusive with *table*)  
  
The query string to use to query the foreign table.
				
* *table*  
  
Required: Yes (mutually exclusive with *query*)  
  
The table on the foreign server to query.

* *row_estimate_method*

Required: No

Default: `execute`

This can be one of the following values:

* `execute`: Execute the query on the remote server, and get the actual number of rows in the query.
* `showplan_all`: This gets the estimated number of rows using [MS SQL Server's SET SHOWPLAN_ALL](https://msdn.microsoft.com/en-us/library/ms187735.aspx).

### Example

Using a *table* definition:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (table 'dbo.mytable', row_estimate_method 'showplan_all');
```
	
Or using a *query* definition:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (query 'SELECT * FROM dbo.mytable', row_estimate_method 'showplan_all');
```