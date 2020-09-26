IF OBJECT_ID('@SCHEMANAME.view_simple', 'V') IS NOT NULL
        DROP VIEW @SCHEMANAME.view_simple;

CREATE VIEW @SCHEMANAME.view_simple (id, data)
        AS SELECT id, data FROM @SCHEMANAME.table_view_simple;
