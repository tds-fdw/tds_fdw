DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.varbinarymax;

CREATE FOREIGN TABLE @PSCHEMANAME.varbinarymax (
        id int,
        value bytea
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.varbinarymax', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.varbinarymax;
