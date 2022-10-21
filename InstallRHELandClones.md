# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallRHELandClones.md


## Installing on RHEL and Clones such as CentOS, Rocky Linux, AlmaLinux or Oracle

This document will show how to install tds_fdw on Rocky Linux 8.5. RHEL distributions should be similar.

NOTE: For the sake of simplicity, we will use `yum` as it works as an alias for `dnf` on newer distributions.

### Option A: yum/dnf (released versions)

#### PostgreSQL

If you need to install PostgreSQL, do so by following the [RHEL installation instructions](https://www.postgresql.org/download/linux/redhat/).

Here is an extract of the instructions:

Only for RHEL 8 and clones such as Rocky Linux 8:
```bash
sudo sudo dnf -qy module disable postgresql # Not required for RHEL8 and clones
```

Install the PostgreSQL repository and packages:

```bash
sudo rpm -i https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-x86_64/pgdg-redhat-repo-latest.noarch.rpm
sudo yum install postgresql11 postgresql11-server postgresql11-libs postgresql11-devel
```

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

Here is an extract of the instructions:

Only for RHEL 8 and clones such as Rocky Linux 8:
```bash
sudo sudo dnf -qy module disable postgresql # Not required for RHEL8 and clones
```

Install the PostgreSQL repository and packages:

```bash
sudo rpm -i https://download.postgresql.org/pub/repos/yum/reporpms/EL-8-x86_64/pgdg-redhat-repo-latest.noarch.rpm
sudo yum install postgresql11 postgresql11-server postgresql11-libs postgresql11-devel
```

#### Install FreeTDS devel and build dependencies

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

**NOTE:** In CentOS, you need the [EPEL repository installed](https://fedoraproject.org/wiki/EPEL) to install FreeTDS

```bash
sudo yum install epel-release
sudo yum install freetds-devel
```

## IMPORTANT: CentOS7/Oracle7 and PostgreSQL >= 11

When using the official PostgreSQL packages from postgresql.org, JIT with bitcode is enabled by default and will require llvm5 and `clang` from LLVM5 installed at `/opt/rh/llvm-toolset-7/root/usr/bin/clang` to be able to compile.

You have LLVM5 at the EPEL CentOS7 repository, but not LLVM7, so you will need install the CentOS Software collections.

You can easily do it with the following commands:

```bash
sudo yum install centos-release-scl
```

Some other dependencies are also needed to install PostgreSQL and then compile tds_fdw:

```bash
sudo yum install gcc make wget
```

##### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
export TDS_FDW_VERSION="2.0.3"
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
