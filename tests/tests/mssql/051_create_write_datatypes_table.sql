-- Drop table if it exists
IF OBJECT_ID('@SCHEMANAME.write_datatypes_test', 'U') IS NOT NULL
    DROP TABLE @SCHEMANAME.write_datatypes_test;

-- Create table with various data types for write testing
CREATE TABLE @SCHEMANAME.write_datatypes_test (
    id INT PRIMARY KEY NOT NULL,
    int_val INT,
    bigint_val BIGINT,
    float_val FLOAT,
    varchar_val VARCHAR(100),
    datetime_val DATETIME2
);

-- Insert initial test data with various data types
INSERT INTO @SCHEMANAME.write_datatypes_test 
(id, int_val, bigint_val, float_val, varchar_val, datetime_val)
VALUES 
    (1, 2147483647, 9223372036854775807, 3.14159, 'test_string_1', '2024-01-15 10:30:00'),
    (2, -2147483648, -9223372036854775808, -2.71828, 'test_string_2', '2024-02-20 15:45:30'),
    (3, 0, 0, 0.0, '', '1970-01-01 00:00:00');
