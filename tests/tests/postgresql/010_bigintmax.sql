DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.bigint_max;

CREATE FOREIGN TABLE @PSCHEMANAME.bigint_max (
        id int,
        value bigint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.bigint_max', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.bigint_max;
SELECT (value = 9223372036854775807) AS pass FROM @PSCHEMANAME.bigint_max;
