DROP FOREIGN TABLE IF EXISTS @PSCHEMANAME.binary_text;

CREATE FOREIGN TABLE @PSCHEMANAME.binary_text (
        id int,
        img text,
        vb text
)
        SERVER mssql_svr
        OPTIONS (table '@MSCHEMANAME.binary_text', row_estimate_method 'showplan_all');

DO $$
DECLARE
        r record;
BEGIN
        SELECT img, vb INTO r FROM @PSCHEMANAME.binary_text;

        IF r.img <> repeat('ab', 4000) THEN
                RAISE EXCEPTION 'image read as text: got % characters, expected 8000', length(r.img);
        END IF;

        IF r.vb <> 'ABCD' THEN
                RAISE EXCEPTION 'varbinary read as text: got %, expected ABCD', r.vb;
        END IF;
END
$$;
