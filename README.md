
# Foreign data wrapper for TDS (Sybase and Microsoft SQL Server)

Author: Geoff Montee
Name: tds_fdw
File: tds_fdw/README

# About

This is a PostgreSQL foreign data wrapper for use to connect to databases that use TDS,
such as Sybase databases and Microsoft SQL server.

This foreign data wrapper requires requires a library that uses the DB-Library interface,
such as FreeTDS (http://www.freetds.org/). This has been tested with FreeTDS, but not
the proprietary implementations of DB-Library.

This was written to support PostgreSQL 9.1 and 9.2. It does not support write operations, 
as added in PostgreSQL 9.3. However, it should still work in PostgreSQL 9.3.

# Building

Building was accomplished by doing the following under CentOS 6:

## Install EPEL

In CentOS, you need the EPEL to install FreeTDS.

```bash
wget http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm
sudo rpm -ivh epel-release-6-8.noarch.rpm
```

## Install FreeTDS

```bash
sudo yum install freetds freetds-devel
```

## Build for PostgreSQL 9.1

### Install PostgreSQL 9.1

```bash
wget http://yum.postgresql.org/9.1/redhat/rhel-6-x86_64/pgdg-centos91-9.1-4.noarch.rpm
sudo rpm -ivh pgdg-centos91-9.1-4.noarch.rpm
sudo yum install postgresql91 postgresql91-server postgresql91-libs postgresql91-devel
```

### Clone and build

```bash
git clone https://github.com/GeoffMontee/tds_fdw.git
cd tds_fdw
PATH=/usr/pgsql-9.1/bin:$PATH make USE_PGXS=1
sudo PATH=/usr/pgsql-9.1/bin:$PATH make USE_PGXS=1 install
```

### Start server and install extension

```bash
sudo /etc/init.d/postgresql-9.1 initdb
sudo /etc/init.d/postgresql-9.1 start
/usr/pgsql-9.1/bin/psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```

## Build for PostgreSQL 9.2

### Install PostgreSQL 9.2

```bash
wget http://yum.postgresql.org/9.2/redhat/rhel-6-x86_64/pgdg-centos92-9.2-6.noarch.rpm
sudo rpm -ivh pgdg-centos92-9.2-6.noarch.rpm
sudo yum install postgresql92 postgresql92-server postgresql92-libs postgresql92-devel
```

### Clone and build

```bash
git clone https://github.com/GeoffMontee/tds_fdw.git
cd tds_fdw
PATH=/usr/pgsql-9.2/bin:$PATH make USE_PGXS=1
sudo PATH=/usr/pgsql-9.2/bin:$PATH make USE_PGXS=1 install
```

### Start server and install extension

```bash
sudo /etc/init.d/postgresql-9.2 initdb
sudo /etc/init.d/postgresql-9.2 start
/usr/pgsql-9.2/bin/psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```

# Usage

The usage of tds_fdw is similar to mysql_fdw created by Dave Page.

## Foreign server

Foreign server parameters accepted:

* *servername*		
		The servername, address or hostname of the foreign server server.
		Default: 127.0.0.1
		
		This can be a DSN, as specified in freetds.conf. See:
		http://www.freetds.org/userguide/name.lookup.htm
				
* *port*			
		The port of the foreign server.
		No Default (Optional. Instead, port can be in freetds.conf.)
				
* *language*		
		The language to use for messages and the locale to use for date formats.
		No Default (Optional. FreeTDS may default to 'us_english' on most systems.)
		
		For information related to this for MS SQL Server, see:
		http://technet.microsoft.com/en-us/library/ms174398.aspx
		
		For information related to Sybase ASE, see:
		http://infocenter.sybase.com/help/topic/com.sybase.infocenter.dc32300.1570/html/sqlug/X68290.htm
		and
		http://infocenter.sybase.com/help/topic/com.sybase.infocenter.dc36272.1572/html/commands/X64136.htm
				
* *character_set*	
		The client character set to use for the connection, if you need to set this 
		for some reason.
		No Default
		
		For TDS protocol versions 7.0+, the connection always uses UCS-2, so
		this parameter does nothing in those cases. See:
		http://www.freetds.org/userguide/localization.htm
				

### Foreign server example
			
```SQL			
CREATE SERVER mssql_svr
	FOREIGN DATA WRAPPER tds_fdw
	OPTIONS (servername '127.0.0.1', port '1433');
```
	
## Foreign table
	
Foreign table parameters accepted:

* *database*		
	The database name that the foreign table is a part of.
	Default: NULL
				
* *query*			
	The query string to use to query the foreign table.
	Default: NULL
				
* *table*			
	The table on the foreign server to query.
	Default: NULL
				
The query and table paramters are mutually exclusive.

### Foreign table example

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (database 'mydb', table 'dbo.mytable');
```
	
Or:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (database 'mydb', query 'SELECT * FROM dbo.mytable');
```
	
## User mapping
	
User mapping parameters accepted:

* *username*		
	The username of the account on the foreign server.
	Default: NULL
				
* *password*		
	The password of the account on the foreign server.
	Default: NULL

### User mapping example example

```SQL				
CREATE USER MAPPING FOR postgres
	SERVER mssql_svr 
	OPTIONS (username 'sa', password '');
```
	
# Notes about character sets/encoding

1.) If you get an error like this with MS SQL Server when working with Unicode data:

> NOTICE:  DB-Library notice: Msg #: 4004, Msg state: 1, Msg: Unicode data in a Unicode-only 
> collation or ntext data cannot be sent to clients using DB-Library (such as ISQL) or ODBC 
> version 3.7 or earlier., Server: PILLIUM\SQLEXPRESS, Process: , Line: 1, Level: 16
> ERROR:  DB-Library error: DB #: 4004, DB Msg: General SQL Server error: Check messages from 
> the SQL Server, OS #: -1, OS Msg: (null), Level: 16

You may have to manually set "tds version" in freetds.conf to 7.0 or higher. See:

http://www.freetds.org/userguide/freetdsconf.htm
and
http://www.freetds.org/userguide/choosingtdsprotocol.htm

2.) Although many newer versions of the TDS protocol will only use USC-2 to communicate
with the server, FreeTDS converts the UCS-2 to the client character set of your choice.
To set the client character set, you can set "client charset" in freetds.conf. See:

http://www.freetds.org/userguide/freetdsconf.htm
and
http://www.freetds.org/userguide/localization.htm