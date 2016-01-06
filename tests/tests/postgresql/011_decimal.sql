DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.decimal;

CREATE FOREIGN TABLE @PSCHEMANAME.decimal (
        id int,
        value numeric(9, 2)
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.decimal', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.decimal;
