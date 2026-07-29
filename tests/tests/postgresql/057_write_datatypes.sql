-- Drop and recreate foreign table for datatype testing
DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.write_datatypes_test;

-- First, ensure MSSQL table exists
-- Note: This would normally be in an MSSQL test file, but included here for completeness

-- Create foreign table with various data types
CREATE FOREIGN TABLE @PSCHEMANAME.write_datatypes_test (
    id INT,
    int_val INT,
    bigint_val BIGINT,
    float_val FLOAT,
    varchar_val VARCHAR(100),
    datetime_val TIMESTAMP
)
SERVER mssql_svr
OPTIONS (schema_name '@MSCHEMANAME', table_name 'write_datatypes_test');

-- Note: The MSSQL side table should be created separately
-- This test assumes the table exists on MSSQL side
