# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallDebian.md

## Installing on Debian

This document will show how to install tds_fdw on Debian 10. Other Debian distributions should be similar.

### Install FreeTDS and build dependencies

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

```bash
sudo apt-get update
sudo apt-get install libsybdb5 freetds-dev freetds-common
```

Some other dependencies are also needed to install PostgreSQL and then compile tds_fdw:

```bash
sudo apt-get install gnupg gcc make
```

### Install PostgreSQL

If you need to install PostgreSQL, do so by following the [apt installation directions](https://wiki.postgresql.org/wiki/Apt). For example, to install PostgreSQL 11 on Debian:

```bash
sudo bash -c 'source /etc/os-release; echo "deb http://apt.postgresql.org/pub/repos/apt/ ${VERSION_CODENAME}-pgdg main" > /etc/apt/sources.list.d/pgdg.list'
sudo apt-key adv --keyserver hkp://pool.sks-keyservers.net --recv-keys 0xACCC4CF8
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install postgresql-11 postgresql-client-11 postgresql-server-dev-11
```

**NOTE**: If you already have PostgreSQL installed on your system be sure that the package postgresql-server-dev-XX is installed too (where XX stands for your PostgreSQL version). 

### Install tds_fdw

#### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
export TDS_FDW_VERSION="2.0.4"
sudo apt-get install wget
wget https://github.com/tds-fdw/tds_fdw/archive/v${TDS_FDW_VERSION}.tar.gz
tar -xvzf v${TDS_FDW_VERSION}.tar.gz
cd tds_fdw-${TDS_FDW_VERSION}/
make USE_PGXS=1
sudo make USE_PGXS=1 install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, and then append `PG_CONFIG=<PATH>` after `USE_PGXS=1` at the `make` commands.

#### Build from repository

If you would rather use the current development version, you can clone and build the git repository via something like the following:

```bash
sudo apt-get install git
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
