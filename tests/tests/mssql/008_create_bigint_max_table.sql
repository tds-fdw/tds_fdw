IF OBJECT_ID('@SCHEMANAME.bigint_max', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.bigint_max;

CREATE TABLE @SCHEMANAME.bigint_max (
        id int primary key,
        value bigint
);

INSERT INTO @SCHEMANAME.bigint_max (id, value) VALUES (1, 9223372036854775807);
