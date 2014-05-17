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
# This foreign data wrapper requires requires a library that uses the DB-Library interface,
# such as FreeTDS (http://www.freetds.org/). This has been tested with FreeTDS, but not
# the proprietary implementations of DB-Library.
#----------------------------------------------------------------------------

EXTENSION = tds_fdw

EXTVERSION   = $(shell grep default_version $(EXTENSION).control | sed -e "s/default_version[[:space:]]*=[[:space:]]*'\\([^']*\\)'/\\1/")

DATA         = $(filter-out $(wildcard sql/*--*.sql),$(wildcard sql/*.sql))

# no tests yet
# TESTS        = $(wildcard test/sql/*.sql)
# REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
# REGRESS_OPTS = --inputdir=test

DOCS         = README.${EXTENSION}.md
MODULES      = $(patsubst %.c,%,$(wildcard src/*.c))
PG_CONFIG    = pg_config

# modify these variables to point to FreeTDS, if needed
SHLIB_LINK := -lsybdb
# PG_CPPFLAGS :=
# PG_LIBS :=

all: sql/$(EXTENSION)--$(EXTVERSION).sql README.${EXTENSION}.md

sql/$(EXTENSION)--$(EXTVERSION).sql: sql/$(EXTENSION).sql
	cp $< $@
	
README.${EXTENSION}.md: README.md
	cp $< $@

DATA = $(wildcard sql/*--*.sql) sql/$(EXTENSION)--$(EXTVERSION).sql
EXTRA_CLEAN = sql/$(EXTENSION)--$(EXTVERSION).sql README.${EXTENSION}.md

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)