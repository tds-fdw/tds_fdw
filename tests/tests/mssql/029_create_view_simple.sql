CREATE VIEW @SCHEMANAME.view_simple (id, data)
        AS SELECT id, data FROM @SCHEMANAME.table_view_simple;
