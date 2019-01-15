# TDS Foreign data wrapper

* **Author:** Geoff Montee
* **Name:** tds_fdw
* **File:** tds_fdw/InstallCentOS.md

## Installing on CentOS/RHEL

This document will show how to install tds_fdw on CentOS 6. Other CentOS and RHEL distributions should be similar. 

### Install EPEL

In CentOS, you need the [EPEL repository installed](https://fedoraproject.org/wiki/EPEL) to install FreeTDS.

```bash
sudo yum install epel-release
```

### Install FreeTDS

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

```bash
sudo yum install freetds freetds-devel
```

### Install PostgreSQL

If you need to install PostgreSQL, do so by following the [yum installation directions](https://wiki.postgresql.org/wiki/YUM_Installation). For example, to install PostgreSQL 9.5 on CentOS 7:

```bash
wget https://download.postgresql.org/pub/repos/yum/9.5/redhat/rhel-7-x86_64/pgdg-centos95-9.5-2.noarch.rpm
sudo rpm -ivh pgdg-centos95-9.5-2.noarch.rpm
sudo yum install postgresql95 postgresql95-server postgresql95-libs postgresql95-devel
```

### CentOS7 and PostgreSQL >= 11

When using the official PostgreSQL packages from postgresql.org, JIT with bitcode is enabled by default and will require llvm5 and `clang` from LLVM7 installed at `/opt/rh/llvm-toolset-7/root/usr/bin/clang`

You have LLVM5 at the standard CentOS7 repositories, but not LLVM7, so you will need install the CentOS Software collections.

You can easily do it with the following commands:

```bash
sudo yum install centos-release-scl
sudo yum install llvm-toolset-7-clang llvm5.0
```

### Install tds_fdw

#### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
export TDS_FDW_VERSION="1.0.7"
wget https://github.com/tds-fdw/tds_fdw/archive/v${TDS_FDW_VERSION}.tar.gz -O tds_fdw-${TDS_FDW_VERSION}.tar.gz
tar -xvzf tds_fdw-${TDS_FDW_VERSION}.tar.gz
cd tds_fdw-${TDS_FDW_VERSION}
PATH=/usr/pgsql-9.5/bin:$PATH make USE_PGXS=1
sudo PATH=/usr/pgsql-9.5/bin:$PATH make USE_PGXS=1 install
```

#### Build from repository

If you would rather use the current development version, you can clone and build the git repository via something like the following:

```bash
git clone https://github.com/tds-fdw/tds_fdw.git
cd tds_fdw
PATH=/usr/pgsql-9.5/bin:$PATH make USE_PGXS=1
sudo PATH=/usr/pgsql-9.5/bin:$PATH make USE_PGXS=1 install
```

#### Start server 

If this is a fresh installation, then initialize the data directory and start the server:

```bash
sudo /etc/init.d/postgresql-9.5 initdb
sudo /etc/init.d/postgresql-9.5 start
```

#### Install extension

```bash
/usr/pgsql-9.5/bin/psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```
