-- Test multiple INSERT operations
-- Note: Each INSERT is a separate transaction in the current implementation

INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (101, 'multi_insert_1', 1001, CURRENT_TIMESTAMP);

INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (102, 'multi_insert_2', 1002, CURRENT_TIMESTAMP);

INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (103, 'multi_insert_3', 1003, CURRENT_TIMESTAMP);

-- Verify all inserts succeeded
SELECT COUNT(*) AS multi_insert_count 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 101 AND 103;

-- Validate all three rows exist with correct values
SELECT (COUNT(*) = 3) AS multi_insert_success 
FROM @PSCHEMANAME.write_test 
WHERE id IN (101, 102, 103) 
  AND name LIKE 'multi_insert_%' 
  AND value BETWEEN 1001 AND 1003;
