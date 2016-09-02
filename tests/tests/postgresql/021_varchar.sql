DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.varchar;

CREATE FOREIGN TABLE @PSCHEMANAME.varchar (
        id int,
        value varchar(8000)
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.varchar', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.varchar;
