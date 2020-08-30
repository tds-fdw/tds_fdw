IF OBJECT_ID('@SCHEMANAME.table_view_simple', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.table_view_simple;

CREATE TABLE @SCHEMANAME.table_view_simple (
        id int primary key,
        data varchar(50),
        owner varchar(50)
);

INSERT INTO @SCHEMANAME.table_view_simple (id, data, owner) VALUES
        (1, 'geoff''s data', 'geoff'),
        (2, 'alice''s data', 'alice'),
        (3, 'bob''s data', 'bob');

IF OBJECT_ID('@SCHEMANAME.view_simple', 'V') IS NOT NULL
        DROP VIEW @SCHEMANAME.view_simple;
