IF OBJECT_ID('@SCHEMANAME.null_datetime', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.null_datetime;

CREATE TABLE @SCHEMANAME.null_datetime (
        id int primary key,
        value datetime
);

INSERT INTO @SCHEMANAME.null_datetime (id, value) VALUES (1, NULL);
