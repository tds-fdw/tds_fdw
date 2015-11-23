IF OBJECT_ID('@SCHEMANAME.datetime2', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.datetime2;

CREATE TABLE @SCHEMANAME.datetime2 (
        id int primary key,
        value datetime2
);

INSERT INTO @SCHEMANAME.datetime2 (id, value) VALUES (1, '2015-10-22 11:01:02');
