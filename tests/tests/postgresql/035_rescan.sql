/* function is expensive so that the optimizer puts the foreign table on the inner side of the netsed loop */
CREATE FUNCTION @PSCHEMANAME.expensive() RETURNS TABLE (id integer)
   LANGUAGE sql IMMUTABLE COST 1000000000 ROWS 1 AS
'SELECT * FROM generate_series(1, 3)';

/* force a nested loop */
SET enable_hashjoin = off;
SET enable_mergejoin = off;
SET enable_material = off;

CREATE TEMP TABLE results (id integer, data text);

INSERT INTO results
SELECT id, v.data FROM @PSCHEMANAME.expensive() AS f
   LEFT JOIN @PSCHEMANAME.view_simple AS v
      USING (id);

/* results.data should not be NULL */
DO $$BEGIN
   IF EXISTS (SELECT 1 FROM results WHERE data IS NULL)
   THEN
      RAISE EXCEPTION 'bad results from query';
   END IF;
END;$$;

RESET enable_hashjoin;
RESET enable_mergejoin;
RESET enable_material;
