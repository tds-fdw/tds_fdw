-- Test UPDATE with WHERE clause affecting multiple rows
-- Update all rows with id between 101 and 103
UPDATE @PSCHEMANAME.write_test 
SET value = value + 100,
    name = name || '_updated',
    modified_at = CURRENT_TIMESTAMP
WHERE id BETWEEN 101 AND 103;

-- Verify updates succeeded
SELECT id, name, value 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 101 AND 103
ORDER BY id;

-- Validate all three rows were updated
SELECT (COUNT(*) = 3) AS multi_update_success 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 101 AND 103 
  AND name LIKE '%_updated' 
  AND value BETWEEN 1101 AND 1103;
