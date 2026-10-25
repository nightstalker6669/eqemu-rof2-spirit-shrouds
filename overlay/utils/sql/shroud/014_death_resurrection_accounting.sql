-- Apply offline after the shroud session and pending-XP migrations.
-- Version zero rows are legacy/untrusted and never authorize a new XP refund.
-- No character, original-profile, inventory or ordinary-AA rows are rewritten.
ALTER TABLE spirit_shroud_corpse_state
 ADD COLUMN IF NOT EXISTS provenance_version INT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN IF NOT EXISTS created_at BIGINT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS rez_claimed_at BIGINT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS rez_spell_id INT UNSIGNED NULL;

-- NULL means unmeasured, not zero and not completed. These rows deliberately
-- do not estimate Live XP requirements. First branches are observed selectable.
CREATE TABLE IF NOT EXISTS spirit_shroud_progression_definitions (
 progression_id INT UNSIGNED NOT NULL PRIMARY KEY,
 name VARCHAR(64) NOT NULL,
 completion_xp BIGINT UNSIGNED NULL,
 branch_count INT UNSIGNED NOT NULL,
 definition_version BIGINT UNSIGNED NOT NULL,
 evidence_ref VARCHAR(255) NOT NULL
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS spirit_shroud_branch_progression (
 progression_id INT UNSIGNED NOT NULL,
 branch_id INT UNSIGNED NOT NULL,
 unlock_xp BIGINT UNSIGNED NULL,
 definition_version BIGINT UNSIGNED NOT NULL,
 evidence_ref VARCHAR(255) NOT NULL,
 PRIMARY KEY(progression_id,branch_id)
) ENGINE=InnoDB;
INSERT IGNORE INTO spirit_shroud_progression_definitions
(progression_id,name,completion_xp,branch_count,definition_version,evidence_ref) VALUES
(2,'Aberrations',NULL,5,2026102501,'Live selection catalog; exact completion XP unmeasured'),
(3,'Animals',NULL,4,2026102501,'Live selection catalog; exact completion XP unmeasured'),
(4,'Elementals',NULL,4,2026102501,'Live selection catalog; exact completion XP unmeasured'),
(5,'Goblinoid',NULL,5,2026102501,'Live selection catalog; exact completion XP unmeasured'),
(6,'Humanoid',NULL,4,2026102501,'Live selection catalog; exact completion XP unmeasured'),
(7,'Nature Spirits',NULL,5,2026102501,'Live selection catalog; exact completion XP unmeasured'),
(8,'Reptiles',NULL,3,2026102501,'Live selection catalog; exact completion XP unmeasured'),
(9,'Undead',NULL,4,2026102501,'Live selection catalog; exact completion XP unmeasured');
INSERT IGNORE INTO spirit_shroud_branch_progression
(progression_id,branch_id,unlock_xp,definition_version,evidence_ref)
SELECT d.progression_id,b.branch_id,IF(b.branch_id=1,0,NULL),2026102501,
 IF(b.branch_id=1,'Live unlocked first-branch inspection','Live selection catalog; exact branch unlock XP unmeasured')
FROM spirit_shroud_progression_definitions d
JOIN (SELECT 1 branch_id UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5) b
 ON b.branch_id<=d.branch_count
WHERE d.progression_id BETWEEN 2 AND 9;
