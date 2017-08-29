# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallopenSUSE.md

## Installing on OpenSUSE

This document will show how to install tds_fdw on openSUSE Leap 42.3. Other OpenSUSE and SUSE distributions should be similar. 

### Install FreeTDS

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

```bash
sudo zypper install freetds freetds-dev
```

### Install PostgreSQL

If you need to install PostgreSQL, for example, 9.5:

```bash
sudo zypper install postgresql95 postgresql95-server postgresql95-devel
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
sudo service postgresql start
```

#### Install extension

```bash
psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```
