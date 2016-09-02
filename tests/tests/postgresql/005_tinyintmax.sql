DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.tinyint_max;

CREATE FOREIGN TABLE @PSCHEMANAME.tinyint_max (
        id int,
        value smallint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.tinyint_max', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.tinyint_max;
SELECT (value = 255) AS pass FROM @PSCHEMANAME.tinyint_max;
