
# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/README.md

## About

This is a [PostgreSQL foreign data wrapper](http://wiki.postgresql.org/wiki/Foreign_data_wrappers) that can connect to databases that use the [Tabular Data Stream (TDS) protocol](http://en.wikipedia.org/wiki/Tabular_Data_Stream),
such as Sybase databases and Microsoft SQL server.

This foreign data wrapper requires requires a library that uses the DB-Library interface,
such as [FreeTDS](http://www.freetds.org). This has been tested with FreeTDS, but not
the proprietary implementations of DB-Library.

This was written to support PostgreSQL 9.1 and 9.2. It does not yet support write operations, 
as added in PostgreSQL 9.3. However, it should still support read operations in PostgreSQL 9.3.

## Building on CentOS 6

Building was accomplished by doing the following under CentOS 6. Other Linux platforms should be similar.

### Install EPEL

In CentOS, you need the [EPEL repository installed](https://fedoraproject.org/wiki/EPEL) to install FreeTDS.

```bash
wget http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm
sudo rpm -ivh epel-release-6-8.noarch.rpm
```

### Install FreeTDS

```bash
sudo yum install freetds freetds-devel
```

### Build for PostgreSQL 9.1

#### Install PostgreSQL 9.1

Install PostgreSQL 9.1 via [yum](https://wiki.postgresql.org/wiki/YUM_Installation).

```bash
wget http://yum.postgresql.org/9.1/redhat/rhel-6-x86_64/pgdg-centos91-9.1-4.noarch.rpm
sudo rpm -ivh pgdg-centos91-9.1-4.noarch.rpm
sudo yum install postgresql91 postgresql91-server postgresql91-libs postgresql91-devel
```

#### Clone and build

```bash
git clone https://github.com/GeoffMontee/tds_fdw.git
cd tds_fdw
PATH=/usr/pgsql-9.1/bin:$PATH make USE_PGXS=1
sudo PATH=/usr/pgsql-9.1/bin:$PATH make USE_PGXS=1 install
```

#### Start server and install extension

```bash
sudo /etc/init.d/postgresql-9.1 initdb
sudo /etc/init.d/postgresql-9.1 start
/usr/pgsql-9.1/bin/psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```

### Build for PostgreSQL 9.2

#### Install PostgreSQL 9.2

Install PostgreSQL 9.2 via [yum](https://wiki.postgresql.org/wiki/YUM_Installation).

```bash
wget http://yum.postgresql.org/9.2/redhat/rhel-6-x86_64/pgdg-centos92-9.2-6.noarch.rpm
sudo rpm -ivh pgdg-centos92-9.2-6.noarch.rpm
sudo yum install postgresql92 postgresql92-server postgresql92-libs postgresql92-devel
```

#### Clone and build

```bash
git clone https://github.com/GeoffMontee/tds_fdw.git
cd tds_fdw
PATH=/usr/pgsql-9.2/bin:$PATH make USE_PGXS=1
sudo PATH=/usr/pgsql-9.2/bin:$PATH make USE_PGXS=1 install
```

#### Start server and install extension

```bash
sudo /etc/init.d/postgresql-9.2 initdb
sudo /etc/init.d/postgresql-9.2 start
/usr/pgsql-9.2/bin/psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```

## Building on Ubuntu

Building was accomplished by doing the following under Ubuntu 12.04. Other Ubuntu distributions should work too.

### Install FreeTDS

```bash
sudo apt-get install libsybdb5 freetds-dev freetds-common
```

### Build for PostgreSQL 9.1

#### Install PostgreSQL 9.1

Install PostgreSQL 9.1 via [apt](https://wiki.postgresql.org/wiki/Apt).

```bash
sudo bash -c "echo \"deb http://apt.postgresql.org/pub/repos/apt/ $(lsb_release -c -s)-pgdg main\" > /etc/apt/sources.list.d/pgdg.list"
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install postgresql-9.1 postgresql-client-9.1 postgresql-server-dev-9.1
```

#### Clone and build

```bash
git clone https://github.com/GeoffMontee/tds_fdw.git
cd tds_fdw
make USE_PGXS=1
make USE_PGXS=1 install
```

#### Start server and install extension

```bash
sudo /etc/init.d/postgresql start
psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```

## Usage

The usage of tds_fdw is similar to [mysql_fdw](https://github.com/dpage/mysql_fdw).

### Foreign server

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

#### Foreign server example
			
```SQL			
CREATE SERVER mssql_svr
	FOREIGN DATA WRAPPER tds_fdw
	OPTIONS (servername '127.0.0.1', port '1433');
```
	
### Foreign table
	
Foreign table parameters accepted:

* *database*  
  
Required: No  
  
The database name that the foreign table is a part of. Since you can set your default login
database on the server-side, this is optional.
				
* *query*  
  
Required: Yes (mutually exclusive with *table*)  
  
The query string to use to query the foreign table.
				
* *table*  
  
Required: Yes (mutually exclusive with *query*)  
  
The table on the foreign server to query.

#### Foreign table example

Using a *table* definition:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (database 'mydb', table 'dbo.mytable');
```
	
Or using a *query* definition:

```SQL
CREATE FOREIGN TABLE mssql_table (
	id integer,
	data varchar)
	SERVER mssql_svr
	OPTIONS (database 'mydb', query 'SELECT * FROM dbo.mytable');
```
	
### User mapping
	
User mapping parameters accepted:

* *username*  
  
Required: Yes  
  
The username of the account on the foreign server.
				
* *password*  
  
Required: Yes  
  
The password of the account on the foreign server.

#### User mapping example

```SQL				
CREATE USER MAPPING FOR postgres
	SERVER mssql_svr 
	OPTIONS (username 'sa', password '');
```
	
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