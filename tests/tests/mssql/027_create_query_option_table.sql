IF OBJECT_ID('@SCHEMANAME.query_option', 'U') IS NOT NULL
        DROP TABLE @SCHEMANAME.query_option;

CREATE TABLE @SCHEMANAME.query_option (
        id int primary key,
        data varchar(50),
        owner varchar(50)
);

INSERT INTO @SCHEMANAME.query_option (id, data, owner) VALUES 
	(1, 'geoff''s data', 'geoff'),
	(2, 'alice''s data', 'alice'),
	(3, 'bob''s data', 'bob');
