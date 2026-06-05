-- Count rows before delete
SELECT COUNT(*) AS before_delete_count FROM @PSCHEMANAME.write_test;

-- Test single DELETE operation
DELETE FROM @PSCHEMANAME.write_test WHERE id = 100;

-- Count rows after delete
SELECT COUNT(*) AS after_delete_count FROM @PSCHEMANAME.write_test;

-- Verify the row is gone
SELECT COUNT(*) AS deleted_row_count 
FROM @PSCHEMANAME.write_test 
WHERE id = 100;

-- Validate deletion was successful
SELECT (COUNT(*) = 0) AS delete_success 
FROM @PSCHEMANAME.write_test 
WHERE id = 100;
