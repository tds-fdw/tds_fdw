IF OBJECT_ID('@SCHEMANAME.column_name', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.column_name;

CREATE TABLE @SCHEMANAME.column_name (
        id int primary key,
        value bigint
);

INSERT INTO @SCHEMANAME.column_name (id, value) VALUES (1, 9223372036854775807);

