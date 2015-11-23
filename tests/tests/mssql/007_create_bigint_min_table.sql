IF OBJECT_ID('@SCHEMANAME.bigint_min', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.bigint_min;

CREATE TABLE @SCHEMANAME.bigint_min (
        id int primary key,
        value bigint
);

INSERT INTO @SCHEMANAME.bigint_min (id, value) VALUES (1, -9223372036854775808);
