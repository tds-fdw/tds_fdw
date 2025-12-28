-- Drop table if it exists
IF OBJECT_ID('@SCHEMANAME.write_test', 'U') IS NOT NULL
    DROP TABLE @SCHEMANAME.write_test;

-- Create table for write operations testing
-- Using timestamp-based approach to minimize collisions
CREATE TABLE @SCHEMANAME.write_test (
    id INT PRIMARY KEY NOT NULL,
    name VARCHAR(100),
    value INT,
    created_at DATETIME2 DEFAULT GETDATE(),
    modified_at DATETIME2
);

-- Insert initial test data
INSERT INTO @SCHEMANAME.write_test (id, name, value, created_at)
VALUES 
    (1, 'initial_row_1', 100, GETDATE()),
    (2, 'initial_row_2', 200, GETDATE()),
    (3, 'initial_row_3', 300, GETDATE());
