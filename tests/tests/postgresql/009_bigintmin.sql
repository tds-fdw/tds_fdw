DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.bigint_min;

CREATE FOREIGN TABLE @PSCHEMANAME.bigint_min (
        id int,
        value bigint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.bigint_min', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.bigint_min;
SELECT (value = -9223372036854775808) AS pass FROM @PSCHEMANAME.bigint_min;
