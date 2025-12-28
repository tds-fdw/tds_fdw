-- Create table with timestamp-based naming for concurrent execution safety
-- This demonstrates a pattern for avoiding collisions when multiple test runs execute simultaneously

DECLARE @TableSuffix VARCHAR(20);
DECLARE @TableName NVARCHAR(256);
DECLARE @SQL NVARCHAR(MAX);

-- Generate timestamp-based suffix (format: YYYYMMDD_HHMMSS)
SET @TableSuffix = CONVERT(VARCHAR(8), GETDATE(), 112) + '_' + 
                   REPLACE(CONVERT(VARCHAR(8), GETDATE(), 108), ':', '');

SET @TableName = '@SCHEMANAME.write_concurrent_' + @TableSuffix;

-- Drop if exists (for cleanup from previous failed runs)
SET @SQL = N'
IF OBJECT_ID(''' + @TableName + ''', ''U'') IS NOT NULL
    DROP TABLE ' + @TableName + ';
';
EXEC sp_executesql @SQL;

-- Create table with timestamp in name
SET @SQL = N'
CREATE TABLE ' + @TableName + ' (
    id INT PRIMARY KEY NOT NULL,
    test_run_id VARCHAR(20) NOT NULL,
    operation_type VARCHAR(20),
    data_value INT,
    created_at DATETIME2 DEFAULT GETDATE()
);
';
EXEC sp_executesql @SQL;

-- Insert initial marker row
SET @SQL = N'
INSERT INTO ' + @TableName + ' (id, test_run_id, operation_type, data_value)
VALUES (1, ''' + @TableSuffix + ''', ''SETUP'', 0);
';
EXEC sp_executesql @SQL;

-- Store table name for PostgreSQL tests to use
-- Note: In practice, you'd pass this via test framework or environment variable
PRINT 'Created table: ' + @TableName;
