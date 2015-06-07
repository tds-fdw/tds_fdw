# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/ForeignServerCreation.md

## Creating a Foreign Server

### Options

Foreign server parameters accepted:

* *servername*  
  
Required: Yes  
  
Default: 127.0.0.1  
  
The servername, address or hostname of the foreign server server.  
  
This can be a DSN, as specified in *freetds.conf*. See [FreeTDS name lookup](http://www.freetds.org/userguide/name.lookup.htm).
				
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
  
For information related to this for MS SQL Server, see [SET LANGUAGE in MS SQL Server](http://technet.microsoft.com/en-us/library/ms174398.aspx).  
  
For information related to Sybase ASE, see [Sybase ASE login options](http://infocenter.sybase.com/help/topic/com.sybase.infocenter.dc32300.1570/html/sqlug/X68290.htm)
and [SET LANGUAGE in Sybase ASE](http://infocenter.sybase.com/help/topic/com.sybase.infocenter.dc36272.1572/html/commands/X64136.htm).
				
* *character_set*  
  
Required: No  
  
The client character set to use for the connection, if you need to set this
for some reason.  
  
For TDS protocol versions 7.0+, the connection always uses UCS-2, so
this parameter does nothing in those cases. See [Localization and TDS 7.0](http://www.freetds.org/userguide/localization.htm).		

* *tds_version*  
  
Required: No  
  
The version of the TDS protocol to use for this server. See [Choosing a TDS protocol version](http://www.freetds.org/userguide/choosingtdsprotocol.htm) and [History of TDS Versions](http://www.freetds.org/userguide/tdshistory.htm).

### Example
			
```SQL			
CREATE SERVER mssql_svr
	FOREIGN DATA WRAPPER tds_fdw
	OPTIONS (servername '127.0.0.1', port '1433', database 'tds_fdw_test', tds_version '7.1');
```