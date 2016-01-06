IF OBJECT_ID('@SCHEMANAME.smallint_max', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.smallint_max;

CREATE TABLE @SCHEMANAME.smallint_max (
        id int primary key,
        value smallint
);

INSERT INTO @SCHEMANAME.smallint_max (id, value) VALUES (1, 32767);
