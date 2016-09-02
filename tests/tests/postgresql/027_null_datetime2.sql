DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.null_datetime2;

CREATE FOREIGN TABLE @PSCHEMANAME.null_datetime2 (
        id int,
        value timestamp without time zone
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.null_datetime2', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.null_datetime2;
