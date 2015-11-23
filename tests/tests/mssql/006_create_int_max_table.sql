IF OBJECT_ID('@SCHEMANAME.int_max', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.int_max;

CREATE TABLE @SCHEMANAME.int_max (
        id int primary key,
        value int
);

INSERT INTO @SCHEMANAME.int_max (id, value) VALUES (1, 2147483647);
