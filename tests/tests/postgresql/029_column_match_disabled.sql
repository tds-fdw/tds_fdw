DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.column_match_disabled;

CREATE FOREIGN TABLE @PSCHEMANAME.column_match_disabled (
        id int,
        value bigint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.column_match', row_estimate_method 'showplan_all', match_column_names '0');

SELECT * FROM @PSCHEMANAME.column_match_disabled;
SELECT (value = 9223372036854775807) AS pass FROM @PSCHEMANAME.column_match_disabled;