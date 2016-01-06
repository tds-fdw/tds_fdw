IF OBJECT_ID('@SCHEMANAME.varbinary4', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.varbinary4;

CREATE TABLE @SCHEMANAME.varbinary4 (
        id int primary key,
        value varbinary(4)
);

INSERT INTO @SCHEMANAME.varbinary4 (id, value) VALUES (1, 0x01020304);
