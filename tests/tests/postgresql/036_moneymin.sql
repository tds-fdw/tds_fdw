DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.money_min;

CREATE FOREIGN TABLE @PSCHEMANAME.money_min (
        id int,
        value money
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.money_min', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.money_min;
