IF OBJECT_ID('@SCHEMANAME.money_min', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.money_min;

CREATE TABLE @SCHEMANAME.money_min (
        id int primary key,
        value money
);

INSERT INTO @SCHEMANAME.money_min (id, value) VALUES (1, -922337203685477.5808);
