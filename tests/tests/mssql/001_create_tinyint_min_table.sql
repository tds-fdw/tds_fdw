IF OBJECT_ID('@SCHEMANAME.tinyint_min', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.tinyint_min;

CREATE TABLE @SCHEMANAME.tinyint_min (
        id int primary key,
        value tinyint
);

INSERT INTO @SCHEMANAME.tinyint_min (id, value) VALUES (1, 0);
