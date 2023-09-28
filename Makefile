#*------------------------------------------------------------------
#
#				Foreign data wrapper for TDS (Sybase and Microsoft SQL Server)
#
# Author: Geoff Montee
# Name: tds_fdw
# File: tds_fdw/Makefile
#
# Description:
# This is a PostgreSQL foreign data wrapper for use to connect to databases that use TDS,
# such as Sybase databases and Microsoft SQL server.
#
# This foreign data wrapper requires a library that uses the DB-Library interface,
# such as FreeTDS (http://www.freetds.org/). This has been tested with FreeTDS, but not
# the proprietary implementations of DB-Library.
#----------------------------------------------------------------------------

EXTENSION = tds_fdw

MODULE_big = $(EXTENSION)

OBJS = src/tds_fdw.o src/options.o src/deparse.o

EXTVERSION = $(shell grep default_version $(EXTENSION).control | sed -e "s/default_version[[:space:]]*=[[:space:]]*'\\([^']*\\)'/\\1/")

# no tests yet
# TESTS        = $(wildcard test/sql/*.sql)
# REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
# REGRESS_OPTS = --inputdir=test

DOCS         = README.${EXTENSION}.md

DATA = tds_fdw--2.0.1--2.0.2.sql  tds_fdw--2.0.2--2.0.3.sql sql/$(EXTENSION)--$(EXTVERSION).sql

PG_CONFIG    = pg_config

# modify these variables to point to FreeTDS, if needed
SHLIB_LINK := -lsybdb
TDS_INCLUDE :=
PG_CPPFLAGS := -I./include/ -fvisibility=hidden ${TDS_INCLUDE}
# PG_LIBS :=

all: sql/$(EXTENSION)--$(EXTVERSION).sql README.${EXTENSION}.md

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
	cp $< $@

README.${EXTENSION}.md: README.md
	cp $< $@

EXTRA_CLEAN = sql/$(EXTENSION)--$(EXTVERSION).sql README.${EXTENSION}.md

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
