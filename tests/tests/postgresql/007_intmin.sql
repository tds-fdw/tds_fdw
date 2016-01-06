DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.int_min;

CREATE FOREIGN TABLE @PSCHEMANAME.int_min (
        id int,
        value int
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.int_min', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.int_min;
SELECT (value = -2147483648) AS pass FROM @PSCHEMANAME.int_min;
