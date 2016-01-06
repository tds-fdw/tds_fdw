IF OBJECT_ID('@SCHEMANAME.smallint_min', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.smallint_min;

CREATE TABLE @SCHEMANAME.smallint_min (
        id int primary key,
        value smallint
);

INSERT INTO @SCHEMANAME.smallint_min (id, value) VALUES (1, -32768);
