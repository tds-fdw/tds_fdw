IF OBJECT_ID('@SCHEMANAME.tinyint_max', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.tinyint_max;

CREATE TABLE @SCHEMANAME.tinyint_max (
        id int primary key,
        value tinyint
);

INSERT INTO @SCHEMANAME.tinyint_max (id, value) VALUES (1, 255);
