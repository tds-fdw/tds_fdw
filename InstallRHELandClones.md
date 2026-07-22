# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallRHELandClones.md


## Installing on RHEL and clones Rocky Linux, AlmaLinux or Oracle Linux

This document will show how to install tds_fdw on RHEL / Rocky Linux / AlmaLinux 8, 9 and 10. 

### Option A: dnf (released versions)

#### PostgreSQL

If you need to install PostgreSQL, first install PostgreSQL RPM repository by following the [RHEL installation instructions](https://www.postgresql.org/download/linux/redhat/).

#### tds_fdw

The PostgreSQL RPM repository maintainers packages `tds_fdw`, but they do not provide FreeTDS.

First, install the EPEL repository:

```bash
sudo dnf install epel-release
```

And then install `tds_fdw`:

```bash
sudo dnf install tds_fdw_18
```

Replace 18 with the other supported PostgreSQL versions.

### Option B: Compile tds_fdw

#### PostgreSQL

If you need to install PostgreSQL, do so by following the [RHEL installation instructions](https://www.postgresql.org/download/linux/redhat/).

Make sure that, besides `postgresqlXX-server`, `postgresqlXX-devel` is installed as well

You need to enable the PowerTools repository for RHEL8 and clones, or the CBR repository for RHEL9, RHEL 10 and clones.

#### Install FreeTDS devel and build dependencies

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

**NOTE:** You need the [EPEL repository installed](https://fedoraproject.org/wiki/EPEL) to install FreeTDS

```bash
RHEL/Rocky/AlmaLinux 10: sudo dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-10.noarch.rpm
RHEL/Rocky/AlmaLinux 9: sudo dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-9.noarch.rpm
RHEL/Rocky/AlmaLinux 8: sudo dnf -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm
```

Some other dependencies are also needed to install PostgreSQL and then compile tds_fdw:

```bash
sudo dnf install clang llvm make redhat-rpm-config wget
```

##### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
export TDS_FDW_VERSION="2.0.5"
wget https://github.com/tds-fdw/tds_fdw/archive/v${TDS_FDW_VERSION}.tar.gz
tar -xvzf v${TDS_FDW_VERSION}.tar.gz
cd tds_fdw-${TDS_FDW_VERSION}
make USE_PGXS=1 PG_CONFIG=/usr/pgsql-18/bin/pg_config
sudo make USE_PGXS=1 PG_CONFIG=/usr/pgsql-18/bin/pg_config install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, then adjust `PG_CONFIG` accordingly.

##### Build from repository

If you would rather use the current development version, you can clone and build the git repository via something like the following:

```bash
dnf install git
git clone https://github.com/tds-fdw/tds_fdw.git
cd tds_fdw
make USE_PGXS=1 PG_CONFIG=/usr/pgsql-18/bin/pg_config
sudo make USE_PGXS=1 PG_CONFIG=/usr/pgsql-18/bin/pg_config install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, then adjust `PG_CONFIG` accordingly.

### Final steps

#### Start server 

If this is a fresh installation, then initialize the data directory and start the server:

```bash
sudo /usr/pgsql-18/bin/postgresql18-setup initdb
sudo systemctl enable postgresql-18.service
sudo systemctl start postgresql-18.service
```

#### Install extension

```bash
/usr/pgsql-18/bin/psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```
