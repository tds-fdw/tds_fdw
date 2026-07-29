-- Setup: Create foreign table reference (should already exist from 057)
-- Verify initial data
SELECT COUNT(*) AS initial_count FROM @PSCHEMANAME.write_datatypes_test;

-- Test INSERT with various data types
INSERT INTO @PSCHEMANAME.write_datatypes_test 
(id, int_val, bigint_val, float_val, varchar_val, datetime_val)
VALUES 
    (100, 42, 123456789012345, 1.618033, 'golden_ratio', '2024-03-01 12:00:00');

-- Verify INSERT with different data types
SELECT * FROM @PSCHEMANAME.write_datatypes_test WHERE id = 100;

SELECT 
    (int_val = 42) AND 
    (bigint_val = 123456789012345) AND 
    (ABS(float_val - 1.618033) < 0.0001) AND 
    (varchar_val = 'golden_ratio') AS insert_datatypes_success
FROM @PSCHEMANAME.write_datatypes_test 
WHERE id = 100;

-- Test UPDATE with various data types
UPDATE @PSCHEMANAME.write_datatypes_test 
SET 
    int_val = -999,
    bigint_val = -987654321098765,
    float_val = 2.23606797749979,  -- sqrt(5)
    varchar_val = 'updated_string'
WHERE id = 100;

-- Verify UPDATE with different data types
SELECT * FROM @PSCHEMANAME.write_datatypes_test WHERE id = 100;

SELECT 
    (int_val = -999) AND 
    (bigint_val = -987654321098765) AND 
    (ABS(float_val - 2.236067) < 0.0001) AND 
    (varchar_val = 'updated_string') AS update_datatypes_success
FROM @PSCHEMANAME.write_datatypes_test 
WHERE id = 100;

-- Test edge cases: max/min values
INSERT INTO @PSCHEMANAME.write_datatypes_test 
(id, int_val, bigint_val, float_val, varchar_val, datetime_val)
VALUES 
    (101, 2147483647, 9223372036854775807, 1.7976931348623157E+308, 
     'max_values', '9999-12-31 23:59:59');

-- Verify edge case insert
SELECT (int_val = 2147483647) AND (bigint_val = 9223372036854775807) AS edge_case_success
FROM @PSCHEMANAME.write_datatypes_test WHERE id = 101;

-- Cleanup test data
DELETE FROM @PSCHEMANAME.write_datatypes_test WHERE id >= 100;

-- Verify cleanup
SELECT (COUNT(*) = 0) AS cleanup_success 
FROM @PSCHEMANAME.write_datatypes_test WHERE id >= 100;
