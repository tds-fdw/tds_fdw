DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.date;

CREATE FOREIGN TABLE @PSCHEMANAME.date (
        id int,
        value date
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.date', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.date;
