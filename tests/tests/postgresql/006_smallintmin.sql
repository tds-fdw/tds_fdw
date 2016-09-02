DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.smallint_min;

CREATE FOREIGN TABLE @PSCHEMANAME.smallint_min (
        id int,
        value smallint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.smallint_min', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.smallint_min;
SELECT (value = -32768) AS pass FROM @PSCHEMANAME.smallint_min;
