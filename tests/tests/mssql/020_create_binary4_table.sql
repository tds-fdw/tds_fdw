IF OBJECT_ID('@SCHEMANAME.binary4', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.binary4;

CREATE TABLE @SCHEMANAME.binary4 (
        id int primary key,
        value binary(4)
);

INSERT INTO @SCHEMANAME.binary4 (id, value) VALUES (1, 0x01020304);
