IF OBJECT_ID('@SCHEMANAME.float8', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.float8;

CREATE TABLE @SCHEMANAME.float8 (
        id int primary key,
        value float(53)
);

INSERT INTO @SCHEMANAME.float8 (id, value) VALUES (1, 1000.01);
