# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallUbuntu.md

## Installing on Ubuntu

This document will show how to install tds_fdw on Ubuntu 16.04. Other Ubuntu distributions should be similar.

### Install FreeTDS

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

```bash
sudo apt-get install libsybdb5 freetds-dev freetds-common
```

### Install PostgreSQL

If you need to install PostgreSQL, do so by following the [apt installation directions](https://wiki.postgresql.org/wiki/Apt). For example, to install PostgreSQL 9.5 on Ubuntu:

```bash
sudo bash -c "echo \"deb http://apt.postgresql.org/pub/repos/apt/ $(lsb_release -c -s)-pgdg main\" > /etc/apt/sources.list.d/pgdg.list"
wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install postgresql-9.5 postgresql-client-9.5 postgresql-server-dev-9.5
```

### Install tds_fdw

#### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
wget https://github.com/tds-fdw/tds_fdw/archive/v1.0.7.tar.gz
tar -xvzf v1.0.7.tar.gz
cd tds_fdw-1.0.7
make USE_PGXS=1
sudo make USE_PGXS=1 install
```

#### Build from repository

If you would rather use the current development version, you can clone and build the git repository via something like the following:

```bash
git clone https://github.com/tds-fdw/tds_fdw.git
cd tds_fdw
make USE_PGXS=1
sudo make USE_PGXS=1 install
```

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
