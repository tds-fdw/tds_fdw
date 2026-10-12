DROP SERVER IF EXISTS mssql_svr_repeated_options CASCADE;

CREATE SERVER mssql_svr_repeated_options
       FOREIGN DATA WRAPPER tds_fdw
       OPTIONS (servername '@MSERVER', port '@MPORT', database '@MDATABASE', tds_version '@TDSVERSION', msg_handler 'blackhole', dbuse '0', sqlserver_ansi_mode 'false');

CREATE USER MAPPING FOR @PUSER
       SERVER mssql_svr_repeated_options
       OPTIONS (username '@MUSER', password '@MPASSWORD');

CREATE FOREIGN TABLE @PSCHEMANAME.repeated_server_options (
        id int,
        value bigint
)
        SERVER mssql_svr_repeated_options
        OPTIONS (table '@MSCHEMANAME.column_match', row_estimate_method 'showplan_all');

SELECT * FROM @PSCHEMANAME.repeated_server_options;
SELECT * FROM @PSCHEMANAME.repeated_server_options;
