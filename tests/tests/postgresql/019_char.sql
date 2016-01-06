DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.char;

CREATE FOREIGN TABLE @PSCHEMANAME.char (
        id int,
        value char(8000)
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.char', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.char;
