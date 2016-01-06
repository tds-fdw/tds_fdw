DROP SERVER IF EXISTS mssql_svr CASCADE;

CREATE SERVER mssql_svr
       FOREIGN DATA WRAPPER tds_fdw
       OPTIONS (servername '@MSERVER', port '@MPORT', database '@MDATABASE', tds_version '7.1', msg_handler 'blackhole');
