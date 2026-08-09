IF OBJECT_ID('@SCHEMANAME.varchar', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.varchar;

CREATE TABLE @SCHEMANAME.varchar (
        id int primary key,
        value varchar(8000)
);

INSERT INTO @SCHEMANAME.varchar (id, value) VALUES (1, 'this is a string with a \ backslash');
