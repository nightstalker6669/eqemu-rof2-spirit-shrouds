-- Temporary starter provenance is retained per committed session. Consumable
-- supplies are deliberately omitted until stack provenance is supported.
CREATE TABLE IF NOT EXISTS spirit_shroud_starter_equipment (
 template_id INT UNSIGNED NOT NULL,
 slot_id INT UNSIGNED NOT NULL,
 item_id INT UNSIGNED NOT NULL,
 PRIMARY KEY(template_id,slot_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS spirit_shroud_starter_provenance (
 session_id BIGINT UNSIGNED NOT NULL,
 guid BIGINT UNSIGNED NOT NULL,
 item_id INT UNSIGNED NOT NULL,
 issued_slot INT UNSIGNED NOT NULL,
 issued_token VARCHAR(63) NULL,
 PRIMARY KEY(session_id,guid)
) ENGINE=InnoDB;
CREATE TEMPORARY TABLE shroud_starter_guard(ok TINYINT NOT NULL CHECK(ok=1)) ENGINE=InnoDB;
START TRANSACTION;
INSERT INTO shroud_starter_guard SELECT IF(
 EXISTS(SELECT 1 FROM items WHERE id=94114 AND name='Rusty Staff' AND itemclass=0 AND itemtype=4)
 AND EXISTS(SELECT 1 FROM items WHERE id=94074 AND name='Backpack' AND itemclass=1 AND bagslots=8)
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_starter_equipment WHERE template_id=20105 AND
  NOT ((slot_id=13 AND item_id=94114) OR (slot_id=23 AND item_id=94074))),1,0);
-- Matching shroud-specific local item definitions: Rusty Staff (94114),
-- Backpack (94074). No normal item definitions are modified.
INSERT INTO spirit_shroud_starter_equipment(template_id,slot_id,item_id)
 SELECT 20105,13,94114 FROM items WHERE id=94114 AND name='Rusty Staff' AND itemclass=0 AND itemtype=4
 ON DUPLICATE KEY UPDATE item_id=VALUES(item_id);
INSERT INTO spirit_shroud_starter_equipment(template_id,slot_id,item_id)
 SELECT 20105,23,94074 FROM items WHERE id=94074 AND name='Backpack' AND itemclass=1 AND bagslots=8
 ON DUPLICATE KEY UPDATE item_id=VALUES(item_id);
INSERT INTO shroud_starter_guard SELECT IF((SELECT COUNT(*) FROM spirit_shroud_starter_equipment WHERE template_id=20105)=2,1,0);
COMMIT;
DROP TEMPORARY TABLE shroud_starter_guard;
