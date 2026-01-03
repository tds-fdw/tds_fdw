-- Test INSERT with NULL values
INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (200, NULL, NULL, CURRENT_TIMESTAMP);

-- Verify NULL insert
SELECT id, name, value FROM @PSCHEMANAME.write_test WHERE id = 200;
SELECT (name IS NULL AND value IS NULL) AS null_insert_success 
FROM @PSCHEMANAME.write_test WHERE id = 200;

-- Test UPDATE to NULL
UPDATE @PSCHEMANAME.write_test 
SET value = NULL, modified_at = CURRENT_TIMESTAMP
WHERE id = 2;

-- Verify NULL update
SELECT (value IS NULL) AS null_update_success 
FROM @PSCHEMANAME.write_test WHERE id = 2;

-- Test UPDATE from NULL to value
UPDATE @PSCHEMANAME.write_test 
SET name = 'no_longer_null', modified_at = CURRENT_TIMESTAMP
WHERE id = 200;

-- Verify update from NULL
SELECT (name = 'no_longer_null') AS update_from_null_success 
FROM @PSCHEMANAME.write_test WHERE id = 200;

-- Cleanup
DELETE FROM @PSCHEMANAME.write_test WHERE id = 200;
