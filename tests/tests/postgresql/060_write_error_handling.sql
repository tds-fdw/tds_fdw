-- Test error handling scenarios in write operations
-- Note: These tests verify that appropriate errors are raised

-- Test 1: Duplicate primary key violation
-- First insert should succeed
INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (400, 'error_test_unique', 4000, CURRENT_TIMESTAMP);

-- Verify first insert succeeded
SELECT (COUNT(*) = 1) AS first_insert_success 
FROM @PSCHEMANAME.write_test WHERE id = 400;

-- Second insert with same primary key should fail
-- This is wrapped in a way that catches the error for test validation
DO $$
BEGIN
    INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
    VALUES (400, 'duplicate_key', 4001, CURRENT_TIMESTAMP);
    RAISE EXCEPTION 'Expected duplicate key error did not occur';
EXCEPTION
    WHEN unique_violation THEN
        -- Expected error occurred
        RAISE NOTICE 'Duplicate key error correctly raised';
    WHEN OTHERS THEN
        -- Unexpected error
        RAISE EXCEPTION 'Unexpected error: %', SQLERRM;
END;
$$;

-- Verify only one row exists (second insert failed)
SELECT (COUNT(*) = 1) AS duplicate_key_prevented 
FROM @PSCHEMANAME.write_test WHERE id = 400;

-- Test 2: UPDATE non-existent row (should succeed but affect 0 rows)
UPDATE @PSCHEMANAME.write_test 
SET name = 'should_not_exist'
WHERE id = 99999;

-- Verify row doesn't exist
SELECT (COUNT(*) = 0) AS update_nonexistent_handled 
FROM @PSCHEMANAME.write_test WHERE id = 99999;

-- Test 3: DELETE non-existent row (should succeed but affect 0 rows)
DELETE FROM @PSCHEMANAME.write_test WHERE id = 99999;

-- Verify row doesn't exist
SELECT (COUNT(*) = 0) AS delete_nonexistent_handled 
FROM @PSCHEMANAME.write_test WHERE id = 99999;

-- Cleanup
DELETE FROM @PSCHEMANAME.write_test WHERE id = 400;

-- Verify cleanup
SELECT (COUNT(*) = 0) AS error_test_cleanup_success 
FROM @PSCHEMANAME.write_test WHERE id = 400;
