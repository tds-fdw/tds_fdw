DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.datetimeoffset;

CREATE FOREIGN TABLE @PSCHEMANAME.datetimeoffset (
        id int,
        value timestamp with time zone
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.datetimeoffset', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.datetimeoffset;
