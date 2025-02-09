IF OBJECT_ID('@SCHEMANAME.smallmoney_max', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.smallmoney_max;

CREATE TABLE @SCHEMANAME.smallmoney_max (
        id int primary key,
        value smallmoney
);

INSERT INTO @SCHEMANAME.smallmoney_max (id, value) VALUES (1, 214748.3647);
