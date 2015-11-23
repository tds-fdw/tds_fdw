IF OBJECT_ID('@SCHEMANAME.time', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.time;

CREATE TABLE @SCHEMANAME.time (
        id int primary key,
        value time
);

INSERT INTO @SCHEMANAME.time (id, value) VALUES (1, '11:01:02');
