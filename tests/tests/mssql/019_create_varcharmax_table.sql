IF OBJECT_ID('@SCHEMANAME.varcharmax', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.varcharmax;

CREATE TABLE @SCHEMANAME.varcharmax (
        id int primary key,
        value varchar(max)
);

INSERT INTO @SCHEMANAME.varcharmax (id, value) VALUES (1, 'this is a string');
