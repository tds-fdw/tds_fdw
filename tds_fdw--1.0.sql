/*------------------------------------------------------------------
#
#				Foreign data wrapper for TDS (Sybase and Microsoft SQL Server)
#
# Author: Geoff Montee
# Name: tds_fdw
# File: tds_fdw/tds_fdw--1.0.sql
#
# Description:
# This is a PostgreSQL foreign data wrapper for use to connect to databases that use TDS,
# such as Sybase databases and Microsoft SQL server.
#
# This foreign data wrapper requires requires a library that uses the DB-Library interface,
# such as FreeTDS (http://www.freetds.org/). This has been tested with FreeTDS, but not
# the proprietary implementations of DB-Library.
#----------------------------------------------------------------------------*/

CREATE FUNCTION tds_fdw_handler()
RETURNS fdw_handler
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION tds_fdw_validator(text[], oid)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER tds_fdw
  HANDLER tds_fdw_handler
  VALIDATOR tds_fdw_validator;
  