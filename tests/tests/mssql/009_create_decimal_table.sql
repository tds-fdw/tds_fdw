IF OBJECT_ID('@SCHEMANAME.decimal', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.decimal;

CREATE TABLE @SCHEMANAME.decimal (
        id int primary key,
        value decimal(9, 2)
);

INSERT INTO @SCHEMANAME.decimal (id, value) VALUES (1, 1000.01);
