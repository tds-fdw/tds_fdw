# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallopenSUSE.md

## Installing on openSUSE

This document will show how to install tds_fdw on SLES 15, SLES 16 and openSUSE Leap 16.0.

### Option A: zypper (released versions)

#### PostgreSQL

If you need to install PostgreSQL, do so by following the [SLES/OpenSuSE PostgreSQL repo installation instructions](https://zypp.postgresql.org/howtozypp/).

#### tds_fdw

freetds is a part of SLES 16.0 and OpenSuSE 16.0. Please follow the instructions above to install freetds-config package on SLES 15.7.

Install `tds_fdw`:

```bash
sudo zypper install tds_fdw_18
```

Replace 18 with the other supported PostgreSQL versions.

### Option B: Compile tds_fdw

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

```bash
sudo zypper install freetds-devel
```

Some other dependencies are also needed to install PostgreSQL and then compile tds_fdw:

```bash
sudo zypper install gcc make
```

### Install PostgreSQL

If you need to install PostgreSQL, for example, 18.X:

```bash
sudo zypper install postgresql18 postgresql18-server postgresql18-devel
```

**NOTE**: If you already have PostgreSQL installed on your system be sure that the package postgresqlXX-devel is installed too (where XX stands for your PostgreSQL version). 

### Install tds_fdw

#### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
export TDS_FDW_VERSION="2.0.5"
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
zypper in git
git clone https://github.com/tds-fdw/tds_fdw.git
cd tds_fdw
make USE_PGXS=1
sudo make USE_PGXS=1 install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, and then append `PG_CONFIG=<PATH>` after `USE_PGXS=1` at the `make` commands.

#### Start server 

If this is a fresh installation, then start the server:

```bash
sudo /usr/pgsql-18/bin/postgresql18-setup initdb
sudo systemctl enable postgresql-18.service
sudo systemctl start postgresql-18.service
```

#### Install extension

```bash
psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```
