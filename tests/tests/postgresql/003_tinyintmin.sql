DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.tinyint_min;

CREATE FOREIGN TABLE @PSCHEMANAME.tinyint_min (
        id int,
        value smallint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.tinyint_min', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.tinyint_min;
SELECT (value = 0) AS pass FROM @PSCHEMANAME.tinyint_min;
