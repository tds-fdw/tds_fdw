# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallopenSUSE.md

## Installing on openSUSE

This document will show how to install tds_fdw on openSUSE Leap 15.1. Other openSUSE and SUSE distributions should be similar. 

### Install FreeTDS

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

```bash
sudo zypper install freetds freetds-dev
```

### Install PostgreSQL

If you need to install PostgreSQL, for example, 10.X:

```bash
sudo zypper install postgresql10 postgresql10-server postgresql10-devel
```

### Install tds_fdw

#### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
wget https://github.com/tds-fdw/tds_fdw/archive/v1.0.7.tar.gz
tar -xvzf tds_fdw-1.0.7.tar.gz
cd tds_fdw-1.0.7
make USE_PGXS=1
sudo make USE_PGXS=1 install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, and then append `PG_CONFIG=<PATH>` after `USE_PGXS=1` at the `make` commands.

#### Build from repository

If you would rather use the current development version, you can clone and build the git repository via something like the following:

```bash
git clone https://github.com/tds-fdw/tds_fdw.git
cd tds_fdw
make USE_PGXS=1
sudo make USE_PGXS=1 install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, and then append `PG_CONFIG=<PATH>` after `USE_PGXS=1` at the `make` commands.

#### Start server 

If this is a fresh installation, then start the server:

```bash
sudo service postgresql start
```

#### Install extension

```bash
psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```
