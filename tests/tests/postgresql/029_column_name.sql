DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.column_name;

CREATE FOREIGN TABLE @PSCHEMANAME.column_name (
        id int,
        local_name bigint OPTIONS (column_name 'value')
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.column_name', row_estimate_method 'showplan_all', match_column_names '1');

SELECT * FROM @PSCHEMANAME.column_name;
SELECT (local_name = 9223372036854775807) AS pass FROM @PSCHEMANAME.column_name;

