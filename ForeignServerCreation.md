# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/ForeignServerCreation.md

## Creating a Foreign Server

### Options

#### Foreign server parameters accepted:

* *servername*  
  
Required: Yes  
  
Default: 127.0.0.1  
  
The servername, address or hostname of the foreign server server.  
  
This can be a DSN, as specified in *freetds.conf*. See [FreeTDS name lookup](https://www.freetds.org/userguide/name.lookup.html).

You can set this option to a comma separated list of server names, then each
server is tried until the first connection succeeds.  
This is useful for automatic fail-over to a secondary server.
				
* *port*  
  
Required: No  
  
The port of the foreign server. This is optional. Instead of providing a port
here, it can be specified in *freetds.conf* (if *servername* is a DSN).

* *database*  
  
Required: No  
  
The database to connect to for this server.

* *dbuse*

Required: No

Default: 0

This option tells tds_fdw to connect directly to *database* if *dbuse* is 0.
If *dbuse* is not 0, tds_fdw will connect to the server's default database, and
then select *database* by calling DB-Library's dbuse() function.

For Azure, *dbuse* currently needs to be set to 0.
				
* *language*  
  
Required: No  
  
The language to use for messages and the locale to use for date formats.
FreeTDS may default to *us_english* on most systems. You can probably also change
this in *freetds.conf*.  
  
For information related to this for MS SQL Server, see [SET LANGUAGE in MS SQL Server](https://technet.microsoft.com/en-us/library/ms174398.aspx).
  
For information related to Sybase ASE, see [Sybase ASE login options](http://infocenter.sybase.com/help/topic/com.sybase.infocenter.dc32300.1570/html/sqlug/X68290.htm)
and [SET LANGUAGE in Sybase ASE](http://infocenter.sybase.com/help/topic/com.sybase.infocenter.dc36272.1572/html/commands/X64136.htm).
				
* *character_set*  
  
Required: No  
  
The client character set to use for the connection, if you need to set this
for some reason.  
  
For TDS protocol versions 7.0+, the connection always uses UCS-2, so
this parameter does nothing in those cases. See [Localization and TDS 7.0](https://www.freetds.org/userguide/Localization.html).		

* *tds_version*  
  
Required: No  
  
The version of the TDS protocol to use for this server. See [Choosing a TDS protocol version](https://www.freetds.org/userguide/ChoosingTdsProtocol.html) and [History of TDS Versions](https://www.freetds.org/userguide/tdshistory.html).

* *msg_handler*  
  
Required: No  

Default: blackhole
  
The function used for the TDS message handler. Options are "notice" and "blackhole." With the "notice" option, TDS messages are turned into PostgreSQL notices. With the "blackhole" option, TDS messages are ignored.

* *fdw_startup_cost*

Required: No

A cost that is used to represent the overhead of using this FDW used in query planning.

* *fdw_tuple_cost*

Required: No

A cost that is used to represent the overhead of fetching rows from this server used in query planning.

* *sqlserver_ansi_mode*

Required: No

This option is supported for SQL Server only. The default is "false". Setting this to "true" will enable the following server-side settings after a successful connection to the foreign server:

	* CONCAT_NULLS_YIELDS_NULL ON
	* ANSI_NULLS ON
	* ANSI_WARNINGS ON
	* QUOTED_IDENTIFIER ON
	* ANSI_PADDING ON
	* ANSI_NULL_DFLT_ON ON

Those parameters in summary are comparable to the SQL Server option *ANSI_DEFAULTS*. In contrast, *sqlserver_ansi_mode* currently does not activate the following options:

	* CURSOR_CLOSE_ON_COMMIT
	* IMPLICIT_TRANSACTIONS

This follows the behavior of the native ODBC and OLEDB driver for SQL Servers, which explicitly turn them `OFF` if not configured otherwise.

#### Foreign table parameters accepted in server definition:

Some foreign table options can also be set at the server level. Those include:

* *use_remote_estimate*
* *row_estimate_method*

### Example
			
```SQL			
CREATE SERVER mssql_svr
	FOREIGN DATA WRAPPER tds_fdw
	OPTIONS (servername '127.0.0.1', port '1433', database 'tds_fdw_test', tds_version '7.1');
```
