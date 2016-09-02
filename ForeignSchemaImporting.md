# TDS Foreign data wrapper

* **Name:** tds_fdw
* **File:** tds_fdw/ForeignSchemaImporting.md

## Importing a Foreign Schema

### Options

#### Foreign schema parameters accepted:

* *import_default*

Required: No

Default: false

Controls whether column DEFAULT expressions are included in the definitions of foreign tables.

* *import_not_null*

Required: No

Default: true

Controls whether column NOT NULL constraints are included in the definitions of foreign tables.

### Example

```SQL
IMPORT FOREIGN SCHEMA dbo
	EXCEPT (mssql_table)
	FROM SERVER mssql_svr
	INTO public
	OPTIONS (import_default 'true');
```
