-- Create foreign table for write operations
DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.write_test;

CREATE FOREIGN TABLE @PSCHEMANAME.write_test (
    id INT,
    name VARCHAR(100),
    value INT,
    created_at TIMESTAMP,
    modified_at TIMESTAMP
)
SERVER mssql_svr
OPTIONS (schema_name '@MSCHEMANAME', table_name 'write_test');

-- Verify initial data
SELECT COUNT(*) AS initial_count FROM @PSCHEMANAME.write_test;

-- Test single INSERT
INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (100, 'fdw_insert_test', 1000, CURRENT_TIMESTAMP);

-- Verify INSERT succeeded
SELECT COUNT(*) AS after_insert_count FROM @PSCHEMANAME.write_test;
SELECT id, name, value FROM @PSCHEMANAME.write_test WHERE id = 100;

-- Validate results
SELECT (COUNT(*) = 1) AS insert_success 
FROM @PSCHEMANAME.write_test 
WHERE id = 100 AND name = 'fdw_insert_test' AND value = 1000;
