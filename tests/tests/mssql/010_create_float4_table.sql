IF OBJECT_ID('@SCHEMANAME.float4', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.float4;

CREATE TABLE @SCHEMANAME.float4 (
        id int primary key,
        value float(24)
);

INSERT INTO @SCHEMANAME.float4 (id, value) VALUES (1, 1000.01);
