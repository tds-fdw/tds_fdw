IF OBJECT_ID('@SCHEMANAME.date', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.date;

CREATE TABLE @SCHEMANAME.date (
        id int primary key,
        value date
);

INSERT INTO @SCHEMANAME.date (id, value) VALUES (1, '2015-10-22');
