/* function is expensive so that the optimizer puts the foreign table on the inner side of the netsed loop */
CREATE FUNCTION @PSCHEMANAME.expensive() RETURNS TABLE (id integer)
   LANGUAGE sql IMMUTABLE COST 1000000000 ROWS 1 AS
'SELECT * FROM generate_series(1, 3)';

/* force a nested loop */
SET enable_hashjoin = off;
SET enable_mergejoin = off;
SET enable_material = off;

SELECT * FROM @PSCHEMANAME.expensive()
   LEFT JOIN @PSCHEMANAME.view_simple
      USING (id);

RESET enable_hashjoin;
RESET enable_mergejoin;
RESET enable_material;
