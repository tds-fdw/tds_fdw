IF OBJECT_ID('@SCHEMANAME.column_match', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.column_match;

CREATE TABLE @SCHEMANAME.column_match (
        id int primary key,
        value bigint
);

INSERT INTO @SCHEMANAME.column_match (id, value) VALUES (1, 9223372036854775807);
