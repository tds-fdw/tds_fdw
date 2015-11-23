IF OBJECT_ID('@SCHEMANAME.varbinarymax', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.varbinarymax;

CREATE TABLE @SCHEMANAME.varbinarymax (
        id int primary key,
        value varbinary(max)
);

INSERT INTO @SCHEMANAME.varbinarymax (id, value) VALUES (1, 0x01000304);
