IF OBJECT_ID('@SCHEMANAME.money_max', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.money_max;

CREATE TABLE @SCHEMANAME.money_max (
        id int primary key,
        value money
);

INSERT INTO @SCHEMANAME.money_max (id, value) VALUES (1, 922337203685477.5807);
