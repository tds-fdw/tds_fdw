DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.repeated_table_options_1;
DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.repeated_table_options_2;

CREATE FOREIGN TABLE @PSCHEMANAME.repeated_table_options_1 (
        id int,
        value bigint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.column_match', row_estimate_method 'showplan_all', use_remote_estimate '1');

CREATE FOREIGN TABLE @PSCHEMANAME.repeated_table_options_2 (
        id int,
        value bigint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.column_match', row_estimate_method 'showplan_all', use_remote_estimate '1');

SELECT * FROM @PSCHEMANAME.repeated_table_options_1;
SELECT * FROM @PSCHEMANAME.repeated_table_options_1;
SELECT * FROM @PSCHEMANAME.repeated_table_options_2;
