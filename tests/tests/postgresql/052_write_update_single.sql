-- Verify initial state before update
SELECT id, name, value FROM @PSCHEMANAME.write_test WHERE id = 1;

-- Test single UPDATE - modify both name and value
UPDATE @PSCHEMANAME.write_test 
SET name = 'updated_row_1', 
    value = 999, 
    modified_at = CURRENT_TIMESTAMP
WHERE id = 1;

-- Verify UPDATE succeeded
SELECT id, name, value FROM @PSCHEMANAME.write_test WHERE id = 1;

-- Validate update was successful
SELECT (COUNT(*) = 1) AS update_success 
FROM @PSCHEMANAME.write_test 
WHERE id = 1 AND name = 'updated_row_1' AND value = 999;
