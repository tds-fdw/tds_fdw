# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallDebian.md

## Installing on Debian

This document will show how to install tds_fdw on Debian 10. Other Debian distributions should be similar.

### Install FreeTDS

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

```bash
sudo apt-get install libsybdb5 freetds-dev freetds-common
```

### Install PostgreSQL

If you need to install PostgreSQL, do so by following the [apt installation directions](https://wiki.postgresql.org/wiki/Apt). For example, to install PostgreSQL 11 on Debian:

```bash
sudo bash -c "echo \"deb http://apt.postgresql.org/pub/repos/apt/ $(lsb_release -c -s)-pgdg main\" > /etc/apt/sources.list.d/pgdg.list"
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install postgresql-11 postgresql-client-11 postgresql-server-dev-11
```

#### Important

If you already have PostgreSQL installed on your system be sure that the package postgresql-server-dev-XX is installed too (where XX stands for your PostgreSQL version). 


### Install tds_fdw

#### Build from release package

With Debian 10 it's recommended to use one of the release packages.
You can download and install them via something like the following:

```bash
wget https://github.com/tds-fdw/tds_fdw/archive/v2.0.0-alpha.3.tar.gz
tar -xvzf v2.0.0-alpha.3.tar.gz
cd tds_fdw-2.0.0-alpha.3/
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

Please note that at the moment of writing this guide is reported a problem creating the extension in Postgres using the current evelopment version.
Everything should be fine using the latest Alpha release (previous versions not yet tested in Debian 10).

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, and then append `PG_CONFIG=<PATH>` after `USE_PGXS=1` at the `make` commands.

#### Start server 

If this is a fresh installation, then start the server:

```bash
sudo /etc/init.d/postgresql start
```

#### Install extension

```bash
psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```
