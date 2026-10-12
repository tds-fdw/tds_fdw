DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.unconvertible_chars;
DROP SERVER IF EXISTS mssql_svr_latin1 CASCADE;

CREATE SERVER mssql_svr_latin1
       FOREIGN DATA WRAPPER tds_fdw
       OPTIONS (servername '@MSERVER', port '@MPORT', database '@MDATABASE', tds_version '@TDSVERSION', character_set 'ISO-8859-1', msg_handler 'blackhole');

CREATE USER MAPPING FOR @PUSER
       SERVER mssql_svr_latin1
       OPTIONS (username '@MUSER', password '@MPASSWORD');

CREATE FOREIGN TABLE @PSCHEMANAME.unconvertible_chars (
        id int,
        value text
)
        SERVER mssql_svr_latin1
        OPTIONS (table '@MSCHEMANAME.unconvertible_chars', row_estimate_method 'showplan_all');

DO $$
DECLARE
        v text;
BEGIN
        SELECT value INTO v FROM @PSCHEMANAME.unconvertible_chars;

        IF v <> '??' THEN
                RAISE EXCEPTION 'expected unconvertible characters to become ??, got %', v;
        END IF;
END
$$;

DROP SERVER mssql_svr_latin1 CASCADE;
