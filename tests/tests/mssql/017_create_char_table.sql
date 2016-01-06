IF OBJECT_ID('@SCHEMANAME.char', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.char;

CREATE TABLE @SCHEMANAME.char (
        id int primary key,
        value char(8000)
);

INSERT INTO @SCHEMANAME.char (id, value) VALUES (1, 'this is a string');
