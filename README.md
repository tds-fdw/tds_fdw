
# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/README.md

## About

This is a [PostgreSQL foreign data wrapper](http://wiki.postgresql.org/wiki/Foreign_data_wrappers) that can connect to databases that use the [Tabular Data Stream (TDS) protocol](http://en.wikipedia.org/wiki/Tabular_Data_Stream),
such as Sybase databases and Microsoft SQL server.

This foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org). This has been tested with FreeTDS, but not
the proprietary implementations of DB-Library.

This should support PostgreSQL 9.2+.

The current version does not yet support JOIN push-down, or write operations.

It does support WHERE and column pushdowns when *match_column_names* is enabled.

## Build Status

[![Build Status](https://jenkins.juliogonzalez.es/job/tds_fdw-build/badge/icon)](https://jenkins.juliogonzalez.es/job/tds_fdw-build/)

## Installing on CentOS

See [installing tds_fdw on CentOS](InstallCentOS.md).

## Installing on Ubuntu

See [installing tds_fdw on Ubuntu](InstallUbuntu.md).

## Installing on OSX

See [installing tds_fdw on OSX](InstallOSX.md).

## Usage

### Foreign server

See [creating a foreign server](ForeignServerCreation.md).
	
### Foreign table
	
See [creating a foreign table](ForeignTableCreation.md).
	
### User mapping
	
See [creating a user mapping](UserMappingCreation.md).

### Foreign schema

See [importing a foreign schema](ForeignSchemaImporting.md).

### Variables

See [variables](Variables.md).
	
## Notes about character sets/encoding

1. If you get an error like this with MS SQL Server when working with Unicode data:
   
   > NOTICE:  DB-Library notice: Msg #: 4004, Msg state: 1, Msg: Unicode data in a Unicode-only 
   > collation or ntext data cannot be sent to clients using DB-Library (such as ISQL) or ODBC 
   > version 3.7 or earlier., Server: PILLIUM\SQLEXPRESS, Process: , Line: 1, Level: 16  
   > ERROR:  DB-Library error: DB #: 4004, DB Msg: General SQL Server error: Check messages from 
   > the SQL Server, OS #: -1, OS Msg: (null), Level: 16
   
   You may have to manually set *tds version* in *freetds.conf* to 7.0 or higher. See [The *freetds.conf* File](http://www.freetds.org/userguide/freetdsconf.htm).
   and [Choosing a TDS protocol version](http://www.freetds.org/userguide/choosingtdsprotocol.htm).

2. Although many newer versions of the TDS protocol will only use USC-2 to communicate
with the server, FreeTDS converts the UCS-2 to the client character set of your choice. 
To set the client character set, you can set *client charset* in *freetds.conf*. See 
[The *freetds.conf* File](http://www.freetds.org/userguide/freetdsconf.htm) and [Localization and TDS 7.0](http://www.freetds.org/userguide/localization.htm).

## Support

If you find any bugs, or you would like to request enhancements, please submit your comments on the [project's GitHub Issues page](https://github.com/GeoffMontee/tds_fdw/issues).

Additionally, I do subscribe to several [PostgreSQL mailing lists](http://www.postgresql.org/list/) including *pgsql-general* and *pgsql-hackers*. If tds_fdw is mentioned in an email sent to one of those lists, I typically see it.

