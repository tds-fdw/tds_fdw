IF OBJECT_ID('@SCHEMANAME.datetime', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.datetime;

CREATE TABLE @SCHEMANAME.datetime (
        id int primary key,
        value datetime
);

INSERT INTO @SCHEMANAME.datetime (id, value) VALUES (1, '2015-10-22 11:01:02');
