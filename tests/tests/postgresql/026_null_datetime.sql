DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.null_datetime;

CREATE FOREIGN TABLE @PSCHEMANAME.null_datetime (
        id int,
        value timestamp without time zone
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.null_datetime', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.null_datetime;
