IF OBJECT_ID('@SCHEMANAME.int_min', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.int_min;

CREATE TABLE @SCHEMANAME.int_min (
        id int primary key,
        value int
);

INSERT INTO @SCHEMANAME.int_min (id, value) VALUES (1, -2147483648);
