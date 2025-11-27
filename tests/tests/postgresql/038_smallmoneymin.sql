DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.smallmoney_min;

-- There is no smallmoney in PostgreSQL so we map it as the 8bit money type
CREATE FOREIGN TABLE @PSCHEMANAME.smallmoney_min (
        id int,
        value money
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.smallmoney_min', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.smallmoney_min;
