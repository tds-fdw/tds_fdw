-- Count rows before delete
SELECT COUNT(*) AS before_multi_delete FROM @PSCHEMANAME.write_test WHERE id BETWEEN 101 AND 103;

-- Test DELETE with WHERE clause affecting multiple rows
DELETE FROM @PSCHEMANAME.write_test WHERE id BETWEEN 101 AND 103;

-- Count rows after delete
SELECT COUNT(*) AS after_multi_delete FROM @PSCHEMANAME.write_test WHERE id BETWEEN 101 AND 103;

-- Validate deletion was successful
SELECT (COUNT(*) = 0) AS multi_delete_success 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 101 AND 103;

-- Verify initial data rows still exist (weren't accidentally deleted)
SELECT (COUNT(*) = 2) AS initial_rows_preserved 
FROM @PSCHEMANAME.write_test 
WHERE id IN (2, 3);
