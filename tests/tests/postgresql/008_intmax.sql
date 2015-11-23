DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.int_max;

CREATE FOREIGN TABLE @PSCHEMANAME.int_max (
        id int,
        value int
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.int_max', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.int_max;
SELECT (value = 2147483647) AS pass FROM @PSCHEMANAME.int_max;
