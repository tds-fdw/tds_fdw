DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.binary4;

CREATE FOREIGN TABLE @PSCHEMANAME.binary4 (
        id int,
        value bytea
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.binary4', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.binary4;
