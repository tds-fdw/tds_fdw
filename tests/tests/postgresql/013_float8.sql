DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.float8;

CREATE FOREIGN TABLE @PSCHEMANAME.float8 (
        id int,
        value double precision
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.float8', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.float8;
