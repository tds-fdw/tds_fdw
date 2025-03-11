IF OBJECT_ID('@SCHEMANAME.smallmoney_min', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.smallmoney_min;

CREATE TABLE @SCHEMANAME.smallmoney_min (
        id int primary key,
        value smallmoney
);

INSERT INTO @SCHEMANAME.smallmoney_min (id, value) VALUES (1, -214748.3648);
