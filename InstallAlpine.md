# TDS Foreign data wrapper

* **Author:** TheRevenantStar
* **EditedBy:** Guriy Samarin
* **Name:** tds_fdw
* **File:** tds_fdw/InstallAlpine.md

## Installing on Alpine Linux

This document will show how to install tds_fdw on Alpine Linux 3.10.3. Other Alpine Linux distributions should be similar.

### Install FreeTDS and build dependencies

The TDS foreign data wrapper requires a library that implements the DB-Library interface,
such as [FreeTDS](http://www.freetds.org).

```bash
apk add --update freetds-dev
```

Some other dependencies are also needed to install PostgreSQL and then compile tds_fdw:

```bash
apk add gcc libc-dev make
```

In case you will get `fatal error: stdio.h: No such file or directory` later on (on `make USE_PGXS=1`) - installing `musl-dev` migth help (https://stackoverflow.com/questions/42366739/gcc-cant-find-stdio-h-in-alpine-linux):

```bash
apk add musl-dev
```

### Install PostgreSQL

If you need to install PostgreSQL, do so by installing from APK. For example, to install PostgreSQL 11.6 on Alpine Linux:

```bash
apk add postgresql=11.6-r0 postgresql-client=11.6-r0 postgresql-dev=11.6-r0
```

In postgres-alpine docker image you will need only 

```bash
apk add postgresql-dev
```

### Install tds_fdw

#### Build from release package

If you'd like to use one of the release packages, you can download and install them via something like the following:

```bash
export TDS_FDW_VERSION="2.0.3"
apk add wget
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
apk add git
git clone https://github.com/tds-fdw/tds_fdw.git
cd tds_fdw
make USE_PGXS=1
make USE_PGXS=1 install
```

**NOTE:** If you have several PostgreSQL versions and you do not want to build for the default one, first locate where the binary for `pg_config` is, take note of the full path, and then append `PG_CONFIG=<PATH>` after `USE_PGXS=1` at the `make` commands.

#### Start server

If this is a fresh installation, then create the initial cluster and start the server:

```bash
mkdir /var/lib/postgresql/data
chmod 0700 /var/lib/postgresql/data
chown postgres. /var/lib/postgresql/data
su postgres -c 'initdb /var/lib/postgresql/data'
mkdir /run/postgresql/
chown postgres. /run/postgresql/
su postgres -c 'pg_ctl start -D /var/lib/postgresql/data "-o -c listen_addresses=\"\""'
```

#### Install extension

```bash
psql -U postgres
postgres=# CREATE EXTENSION tds_fdw;
```

#### Dockerfile Example

This Dockerfile will build PostgreSQL 11 in Alpine Linux with tds_fdw from master branch

```
FROM library/postgres:11-alpine
RUN apk add --update freetds-dev && \
    apk add git gcc libc-dev make && \
    apk add postgresql-dev postgresql-contrib && \
    git clone https://github.com/tds-fdw/tds_fdw.git && \
    cd tds_fdw && \
    make USE_PGXS=1 && \
    make USE_PGXS=1 install && \
    apk del git gcc libc-dev make && \
    cd ..  && \
    rm -rf tds_fdw
```

You can easily adapt the Dockerfile if you want to use a release package.

This Dockerfile works just like to official PostgreSQL image, just with tds_fdw added. See [Docker Hub library/postgres](https://hub.docker.com/_/postgres/) for details.
