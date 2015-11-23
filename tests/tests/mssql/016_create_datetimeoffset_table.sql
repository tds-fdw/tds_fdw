IF OBJECT_ID('@SCHEMANAME.datetimeoffset', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.datetimeoffset;

CREATE TABLE @SCHEMANAME.datetimeoffset (
        id int primary key,
        value datetimeoffset
);

INSERT INTO @SCHEMANAME.datetimeoffset (id, value) VALUES (1, '2015-10-22 11:01:02 -07:00');
