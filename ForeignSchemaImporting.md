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

* *keep_custom_types*

Required: No

Default: false

Controls how user-defined datatypes from a remote schema are handled. This option only applies to Sybase.

By default, all Sybase user-defined types are resolved to their underlying system types. When this option is enabled, the import preserves the original user-defined types instead of translating them.

If you turn this on, make sure the equivalent domain types already exist in Postgres, e.g.:
```SQL
CREATE DOMAIN public.country_code AS char(2)
    NOT NULL CHECK (VALUE ~ '^[A-Z]{2}$');
```

### Example

```SQL
IMPORT FOREIGN SCHEMA dbo
	EXCEPT (mssql_table)
	FROM SERVER mssql_svr
	INTO public
	OPTIONS (import_default 'true');
```
