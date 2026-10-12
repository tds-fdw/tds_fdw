IF OBJECT_ID('@SCHEMANAME.unconvertible_chars', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.unconvertible_chars;

CREATE TABLE @SCHEMANAME.unconvertible_chars (
        id int primary key,
        value nvarchar(10)
);

INSERT INTO @SCHEMANAME.unconvertible_chars (id, value) VALUES (1, NCHAR(26085) + NCHAR(26412));
