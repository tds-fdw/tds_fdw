IF OBJECT_ID('@SCHEMANAME.null_datetime2', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.null_datetime2;

CREATE TABLE @SCHEMANAME.null_datetime2 (
        id int primary key,
        value datetime2
);

INSERT INTO @SCHEMANAME.null_datetime2 (id, value) VALUES (1, NULL);
