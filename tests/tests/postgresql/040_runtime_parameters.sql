-- Reuse view_simple's three synthetic rows (id = 1, 2, 3).
-- Clean up prepared statements if a previous attempt failed in this session.
DO $$
DECLARE
    statement_name text;
BEGIN
    FOREACH statement_name IN ARRAY ARRAY[
        'tds_runtime_range', 'tds_runtime_and', 'tds_runtime_or'
    ] LOOP
        IF EXISTS (SELECT 1 FROM pg_prepared_statements WHERE name = statement_name) THEN
            EXECUTE format('DEALLOCATE %I', statement_name);
        END IF;
    END LOOP;
END;
$$;

SET plan_cache_mode = force_generic_plan;

-- id is used only by the filter, so it must also be fetched for local quals.
PREPARE tds_runtime_range(integer) AS
    SELECT array_agg(data ORDER BY data)
    FROM @PSCHEMANAME.view_simple WHERE id > $1;

-- The independent constant condition can still be pushed down.
PREPARE tds_runtime_and(integer) AS
    SELECT count(*) FROM @PSCHEMANAME.view_simple WHERE id > $1 AND id < 3;

-- A parameter inside OR must keep the whole expression local.
PREPARE tds_runtime_or(integer) AS
    SELECT count(*) FROM @PSCHEMANAME.view_simple WHERE id > $1 OR id = 1;

DO $$
DECLARE
    actual text[];
    actual_count bigint;
BEGIN
    EXECUTE 'EXECUTE tds_runtime_range(1)' INTO actual;
    IF actual IS DISTINCT FROM ARRAY['alice''s data', 'bob''s data'] THEN
        RAISE EXCEPTION 'generic range(1): unexpected rows %', actual;
    END IF;
    EXECUTE 'EXECUTE tds_runtime_range(2)' INTO actual;
    IF actual IS DISTINCT FROM ARRAY['bob''s data'] THEN
        RAISE EXCEPTION 'generic range(2): unexpected rows %', actual;
    END IF;
    EXECUTE 'EXECUTE tds_runtime_range(3)' INTO actual;
    IF actual IS NOT NULL THEN
        RAISE EXCEPTION 'generic range(3): expected no rows, got %', actual;
    END IF;
    EXECUTE 'EXECUTE tds_runtime_range(NULL)' INTO actual;
    IF actual IS NOT NULL THEN
        RAISE EXCEPTION 'generic range(NULL): expected no rows, got %', actual;
    END IF;
    EXECUTE 'EXECUTE tds_runtime_and(1)' INTO actual_count;
    IF actual_count <> 1 THEN
        RAISE EXCEPTION 'generic AND: expected 1 row, got %', actual_count;
    END IF;
    EXECUTE 'EXECUTE tds_runtime_or(2)' INTO actual_count;
    IF actual_count <> 2 THEN
        RAISE EXCEPTION 'generic OR: expected 2 rows, got %', actual_count;
    END IF;
    EXECUTE 'EXECUTE tds_runtime_or(NULL)' INTO actual_count;
    IF actual_count <> 1 THEN
        RAISE EXCEPTION 'generic OR(NULL): expected 1 row, got %', actual_count;
    END IF;
END;
$$;

-- InitPlan values are PARAM_EXEC, rather than prepared-statement PARAM_EXTERN.
DROP TABLE IF EXISTS pg_temp.tds_runtime_cutoff;
CREATE TEMP TABLE tds_runtime_cutoff (cutoff integer);
INSERT INTO tds_runtime_cutoff VALUES (1);
DO $$
DECLARE
    actual_count bigint;
BEGIN
    SELECT count(*) INTO actual_count FROM @PSCHEMANAME.view_simple
        WHERE id > (SELECT cutoff FROM tds_runtime_cutoff);
    IF actual_count <> 2 THEN
        RAISE EXCEPTION 'InitPlan parameter: expected 2 rows, got %', actual_count;
    END IF;
    UPDATE tds_runtime_cutoff SET cutoff = 2;
    SELECT count(*) INTO actual_count FROM @PSCHEMANAME.view_simple
        WHERE id > (SELECT cutoff FROM tds_runtime_cutoff);
    IF actual_count <> 1 THEN
        RAISE EXCEPTION 'changed InitPlan parameter: expected 1 row, got %', actual_count;
    END IF;
END;
$$;
DROP TABLE tds_runtime_cutoff;

-- Exercise changing PARAM_EXEC values and rescans after a partial read.
DO $$
DECLARE
    actual text[];
BEGIN
    SELECT array_agg(next_row.data ORDER BY bounds.cutoff) INTO actual
    FROM (VALUES (0), (1), (2), (3)) AS bounds(cutoff)
    LEFT JOIN LATERAL (
        SELECT data FROM @PSCHEMANAME.view_simple
        WHERE id > bounds.cutoff ORDER BY id LIMIT 1
    ) AS next_row ON true;
    IF actual IS DISTINCT FROM ARRAY['geoff''s data', 'alice''s data', 'bob''s data', NULL] THEN
        RAISE EXCEPTION 'correlated rescan: unexpected rows %', actual;
    END IF;
END;
$$;

-- A custom plan can fold the parameter to a constant and still push it down.
SET plan_cache_mode = force_custom_plan;
DO $$
DECLARE
    actual text[];
BEGIN
    EXECUTE 'EXECUTE tds_runtime_range(1)' INTO actual;
    IF actual IS DISTINCT FROM ARRAY['alice''s data', 'bob''s data'] THEN
        RAISE EXCEPTION 'custom range(1): unexpected rows %', actual;
    END IF;
END;
$$;

DEALLOCATE tds_runtime_range;
DEALLOCATE tds_runtime_and;
DEALLOCATE tds_runtime_or;
RESET plan_cache_mode;
