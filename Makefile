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

MODULE_big = tds_fdw
OBJS = tds_fdw.o

EXTENSION = tds_fdw
DATA = tds_fdw--1.0.sql

REGRESS = tds_fdw

# modify these variables to point to FreeTDS, if needed
SHLIB_LINK := -lsybdb
# PG_CPPFLAGS :=

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/tds_fdw
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif