# TDS Foreign data wrapper

* **Author:** TheRevenantStar
* **Name:** tds_fdw
* **File:** tds_fdw/InstallUbuntu.md

## Installing on Alpine Linux

This document will show how to install tds_fdw on Alpine Linux

### Install FreeTDS

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

**Alpine Linux requires the 3.7 repo branch be added to install the proper freetds versions**

```bash
echo 'http://dl-cdn.alpinelinux.org/alpine/v3.7/main' >> /etc/apk/repositories
apk add --update freetds-dev=1.00.44-r0 make g++
```

### Install PostgreSQL

If you need to install PostgreSQL, do so by installing from APK.

```bash
sudo apk add --update postgresql=10.5-r0 postgresql-client=10.5-r0 postgresql-dev=10.5-r0
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
sudo pg_ctl start
```

#### Install extension

```bash
psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```

#### Dockerfile Example
This Dockerfile will build PostgreSQL 10.5 in Alpine Linux with TDS FDW

```
FROM library/postgres:10.5-alpine
COPY install/postgresql/tds_fdw /tds_fdw
COPY install/postgresql/init/* /docker-entrypoint-initdb.d/
RUN \
     echo 'http://dl-cdn.alpinelinux.org/alpine/v3.7/main' >> /etc/apk/repositories \
  && apk update \
  && apk add postgresql-dev postgresql-contrib freetds-dev=1.00.44-r0 make g++ \
  && cd /tds_fdw \
  && make USE_PGXS=1 \
  && make USE_PGXS=1 install
```

This Dockerfile works just like to official PostgreSQL image, just with TDS FDW added.
See [Docker Hub library/postgres](https://hub.docker.com/_/postgres/) for details.
