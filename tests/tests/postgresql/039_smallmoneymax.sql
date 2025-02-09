DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.smallmoney_max;

-- There is no smallmoney in PostgreSQL so we map it as the 8bit money type
CREATE FOREIGN TABLE @PSCHEMANAME.smallmoney_max (
        id int,
        value money
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.smallmoney_max', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.smallmoney_max;
