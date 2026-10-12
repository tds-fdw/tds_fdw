IF OBJECT_ID('@SCHEMANAME.binary_text', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.binary_text;

CREATE TABLE @SCHEMANAME.binary_text (
        id int primary key,
        img image,
        vb varbinary(4)
);

INSERT INTO @SCHEMANAME.binary_text (id, img, vb) VALUES (1, REPLICATE(0xab, 4000), 0x41424344);
