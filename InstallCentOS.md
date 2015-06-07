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

If you need to install PostgreSQL, do so by following the [yum installation directions](https://wiki.postgresql.org/wiki/YUM_Installation). For example, to install PostgreSQL 9.4 on CentOS 6:

```bash
wget http://yum.postgresql.org/9.4/redhat/rhel-6-x86_64/pgdg-centos94-9.4-1.noarch.rpm
sudo rpm -ivh pgdg-centos94-9.4-1.noarch.rpm
sudo yum install postgresql94 postgresql94-server postgresql94-libs postgresql94-devel
```

### Install tds_fdw

#### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
wget https://github.com/GeoffMontee/tds_fdw/archive/v1.0.2.tar.gz
tar -xvzf tds_fdw-1.0.2.tar.gz
cd tds_fdw-1.0.2
PATH=/usr/pgsql-9.4/bin:$PATH make USE_PGXS=1
sudo PATH=/usr/pgsql-9.4/bin:$PATH make USE_PGXS=1 install
```

#### Build from repository

If you would rather use the current development version, you can clone and build the git repository via something like the following:

```bash
git clone https://github.com/GeoffMontee/tds_fdw.git
cd tds_fdw
PATH=/usr/pgsql-9.4/bin:$PATH make USE_PGXS=1
sudo PATH=/usr/pgsql-9.4/bin:$PATH make USE_PGXS=1 install
```

#### Start server 

If this is a fresh installation, then initialize the data directory and start the server:

```bash
sudo /etc/init.d/postgresql-9.4 initdb
sudo /etc/init.d/postgresql-9.4 start
```

#### Install extension

```bash
/usr/pgsql-9.4/bin/psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```