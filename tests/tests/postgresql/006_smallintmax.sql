DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.smallint_max;

CREATE FOREIGN TABLE @PSCHEMANAME.smallint_max (
        id int,
        value smallint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.smallint_max', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.smallint_max;
SELECT (value = 32767) AS pass FROM @PSCHEMANAME.smallint_max;
