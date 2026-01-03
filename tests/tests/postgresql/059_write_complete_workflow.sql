-- Complete workflow test: INSERT -> SELECT -> UPDATE -> SELECT -> DELETE -> SELECT
-- This test demonstrates a realistic usage pattern

-- Phase 1: Initial state verification
SELECT COUNT(*) AS initial_count FROM @PSCHEMANAME.write_test;

-- Phase 2: INSERT new records with different data patterns
INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (300, 'workflow_start', 1000, CURRENT_TIMESTAMP);

INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (301, 'workflow_item_1', 1001, CURRENT_TIMESTAMP);

INSERT INTO @PSCHEMANAME.write_test (id, name, value, created_at)
VALUES (302, 'workflow_item_2', 1002, CURRENT_TIMESTAMP);

-- Verify inserts
SELECT COUNT(*) AS after_insert_count 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 300 AND 302;

SELECT (COUNT(*) = 3) AS phase1_insert_success 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 300 AND 302;

-- Phase 3: UPDATE - Modify values based on conditions
UPDATE @PSCHEMANAME.write_test 
SET value = value * 2,
    name = name || '_doubled',
    modified_at = CURRENT_TIMESTAMP
WHERE id IN (301, 302);

-- Verify updates
SELECT id, name, value 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 300 AND 302
ORDER BY id;

SELECT (COUNT(*) = 2) AS phase2_update_success 
FROM @PSCHEMANAME.write_test 
WHERE id IN (301, 302) 
  AND name LIKE '%_doubled'
  AND value IN (2002, 2004);

-- Phase 4: Conditional UPDATE - Update only rows matching criteria
UPDATE @PSCHEMANAME.write_test 
SET name = 'workflow_completed',
    modified_at = CURRENT_TIMESTAMP
WHERE id = 300 AND name = 'workflow_start';

-- Verify conditional update
SELECT (name = 'workflow_completed') AS phase3_conditional_update_success 
FROM @PSCHEMANAME.write_test 
WHERE id = 300;

-- Phase 5: DELETE - Remove specific rows
DELETE FROM @PSCHEMANAME.write_test WHERE id = 301;

-- Verify partial delete
SELECT (COUNT(*) = 2) AS phase4_partial_delete_success 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 300 AND 302;

-- Phase 6: DELETE - Conditional delete based on value
DELETE FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 300 AND 302 AND value > 2000;

-- Verify conditional delete
SELECT (COUNT(*) = 1) AS phase5_conditional_delete_success 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 300 AND 302;

SELECT (id = 300) AS phase5_correct_row_remaining 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 300 AND 302;

-- Phase 7: Final cleanup
DELETE FROM @PSCHEMANAME.write_test WHERE id = 300;

-- Verify complete cleanup
SELECT (COUNT(*) = 0) AS phase6_cleanup_success 
FROM @PSCHEMANAME.write_test 
WHERE id BETWEEN 300 AND 302;

-- Final validation: Ensure original data is still intact
SELECT (COUNT(*) >= 2) AS original_data_preserved 
FROM @PSCHEMANAME.write_test 
WHERE id IN (2, 3);
