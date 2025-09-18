# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallRHELandClones.md


## Installing on RHEL and Clones such as CentOS, Rocky Linux, AlmaLinux or Oracle

This document will show how to install tds_fdw on Rocky Linux 8.9. RHEL distributions should be similar.

NOTE: For the sake of simplicity, we will use `yum` as it works as an alias for `dnf` on newer distributions.

### Option A: yum/dnf (released versions)

#### PostgreSQL

If you need to install PostgreSQL, do so by following the [RHEL installation instructions](https://www.postgresql.org/download/linux/redhat/).

#### tds_fdw

The PostgreSQL development team packages `tds_fdw`, but they do not provide FreeTDS.

First, install the EPEL repository:

```bash
sudo yum install epel-release
```

And then install `tds_fdw`:

```bash
sudo yum install tds_fdw11.x86_64
```

### Option B: Compile tds_fdw

#### PostgreSQL

If you need to install PostgreSQL, do so by following the [RHEL installation instructions](https://www.postgresql.org/download/linux/redhat/).

Make sure that, besides `postgresqlXX-server`, `postgresqlXX-devel` is installed as well

You need to enable the PowerTools repository for RHEL8 and clones, or the CBR repository for RHEL9 and clones.

#### Install FreeTDS devel and build dependencies

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

**NOTE:** You need the [EPEL repository installed](https://fedoraproject.org/wiki/EPEL) to install FreeTDS

```bash
sudo yum install epel-release
sudo yum install freetds-devel
```

Some other dependencies are also needed to install PostgreSQL and then compile tds_fdw:

```bash
sudo yum install clang llvm make redhat-rpm-config wget
```

#### IMPORTANT: CentOS7/Oracle7 and PostgreSQL >= 11

When using the official PostgreSQL packages from postgresql.org, JIT with bitcode is enabled by default and will require llvm5 and `clang` from LLVM5 installed at `/opt/rh/llvm-toolset-7/root/usr/bin/clang` to be able to compile.

You have LLVM5 at the EPEL CentOS7 repository, but not LLVM7, so you will need install the CentOS Software collections.

You can easily do it with the following commands:

```bash
sudo yum install centos-release-scl
```

##### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
export TDS_FDW_VERSION="2.0.5"
wget https://github.com/tds-fdw/tds_fdw/archive/v${TDS_FDW_VERSION}.tar.gz
tar -xvzf v${TDS_FDW_VERSION}.tar.gz
cd tds_fdw-${TDS_FDW_VERSION}
make USE_PGXS=1 PG_CONFIG=/usr/pgsql-11/bin/pg_config
sudo make USE_PGXS=1 PG_CONFIG=/usr/pgsql-11/bin/pg_config install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, then adjust `PG_CONFIG` accordingly.

##### Build from repository

If you would rather use the current development version, you can clone and build the git repository via something like the following:

```bash
yum install git
git clone https://github.com/tds-fdw/tds_fdw.git
cd tds_fdw
make USE_PGXS=1 PG_CONFIG=/usr/pgsql-11/bin/pg_config
sudo make USE_PGXS=1 PG_CONFIG=/usr/pgsql-11/bin/pg_config install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, then adjust `PG_CONFIG` accordingly.

### Final steps

#### Start server 

If this is a fresh installation, then initialize the data directory and start the server:

```bash
sudo /usr/pgsql-11/bin/postgresql11-setup initdb
sudo systemctl enable postgresql-11.service
sudo systemctl start postgresql-11.service
```

#### Install extension

```bash
/usr/pgsql-11/bin/psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```
