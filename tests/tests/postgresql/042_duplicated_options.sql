DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.duplicated_options;
DROP SERVER IF EXISTS mssql_svr_duplicated_options CASCADE;

-- tds_fdw has no redundancy checks of its own; it relies on PostgreSQL
-- rejecting an option given twice before the validator runs.

DO $$
BEGIN
    CREATE SERVER mssql_svr_duplicated_options
           FOREIGN DATA WRAPPER tds_fdw
           OPTIONS (servername '@MSERVER', dbuse '0', dbuse '1');
    RAISE EXCEPTION 'duplicated server option was accepted';
EXCEPTION WHEN duplicate_object THEN
    IF SQLERRM NOT LIKE '%provided more than once' THEN
        RAISE;
    END IF;
END
$$;

DO $$
BEGIN
    CREATE FOREIGN TABLE @PSCHEMANAME.duplicated_options (
            id int,
            value bigint
    )
            SERVER mssql_svr
            OPTIONS (table '@MSCHEMANAME.column_match', use_remote_estimate '1', use_remote_estimate '0');
    RAISE EXCEPTION 'duplicated table option was accepted';
EXCEPTION WHEN duplicate_object THEN
    IF SQLERRM NOT LIKE '%provided more than once' THEN
        RAISE;
    END IF;
END
$$;

CREATE FOREIGN TABLE @PSCHEMANAME.duplicated_options (
        id int,
        value bigint
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.column_match', use_remote_estimate '1');

DO $$
BEGIN
    ALTER FOREIGN TABLE @PSCHEMANAME.duplicated_options OPTIONS (ADD use_remote_estimate '0');
    RAISE EXCEPTION 'option added twice was accepted';
EXCEPTION WHEN duplicate_object THEN
    IF SQLERRM NOT LIKE '%provided more than once' THEN
        RAISE;
    END IF;
END
$$;
