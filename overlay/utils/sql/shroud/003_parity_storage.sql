-- Additive storage for evidence-gated, versioned shroud sessions.
-- Apply offline. Existing starter templates remain accessible through AllowTestSpiritShrouds.
ALTER TABLE spirit_shroud_templates
 ADD COLUMN IF NOT EXISTS content_version INT UNSIGNED NOT NULL DEFAULT 1,
 ADD COLUMN IF NOT EXISTS verified TINYINT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN IF NOT EXISTS test_only TINYINT UNSIGNED NOT NULL DEFAULT 1,
 ADD COLUMN IF NOT EXISTS evidence_ref VARCHAR(255) NULL,
 ADD COLUMN IF NOT EXISTS monster_points INT UNSIGNED NOT NULL DEFAULT 0,
 ADD COLUMN IF NOT EXISTS entitlement_key VARCHAR(63) NULL,
 ADD COLUMN IF NOT EXISTS stat_str INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS stat_sta INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS stat_cha INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS stat_dex INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS stat_int INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS stat_agi INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS stat_wis INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_hp INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_mana INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_endurance INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_hp_regen INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_mana_regen_sitting INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_mana_regen_standing INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_endurance_regen INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_cold INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_fire INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_magic INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_disease INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_poison INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_additional_resist INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_corruption INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_ac INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_attack INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_unarmed_damage INT UNSIGNED NULL,
 ADD COLUMN IF NOT EXISTS native_unarmed_delay INT UNSIGNED NULL;
ALTER TABLE spirit_shroud_abilities
 ADD COLUMN IF NOT EXISTS free_grant TINYINT UNSIGNED NOT NULL DEFAULT 1;
ALTER TABLE spirit_shroud_state
 ADD COLUMN IF NOT EXISTS active_session_id BIGINT UNSIGNED NULL;

CREATE TABLE IF NOT EXISTS spirit_shroud_sessions (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 character_id INT UNSIGNED NOT NULL,
 entry_revision BIGINT UNSIGNED NOT NULL,
 template_id INT UNSIGNED NOT NULL,
 content_version INT UNSIGNED NOT NULL,
 template_json MEDIUMTEXT NOT NULL,
 entered_at BIGINT UNSIGNED NOT NULL,
 restored_at BIGINT UNSIGNED NULL,
 phase VARCHAR(16) NOT NULL DEFAULT 'active',
 UNIQUE KEY character_revision(character_id,entry_revision)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS spirit_shroud_snapshot_manifest (
 session_id BIGINT UNSIGNED NOT NULL,
 table_name VARCHAR(64) NOT NULL,
 row_count BIGINT UNSIGNED NOT NULL,
 PRIMARY KEY(session_id,table_name)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS spirit_shroud_entitlements (
 character_id INT UNSIGNED NOT NULL,
 entitlement_key VARCHAR(63) NOT NULL,
 granted_at BIGINT UNSIGNED NOT NULL,
 PRIMARY KEY(character_id,entitlement_key)
) ENGINE=InnoDB;
-- Reserved accounting domains are kept separate. No undocumented Live XP
-- conversion or corpse policy is enabled merely by creating these tables.
CREATE TABLE IF NOT EXISTS spirit_shroud_experience_events (
 session_id BIGINT UNSIGNED NOT NULL,
 event_key BINARY(16) NOT NULL,
 source INT UNSIGNED NOT NULL,
 progression_delta BIGINT NOT NULL DEFAULT 0,
 normal_delta BIGINT NOT NULL DEFAULT 0,
 aa_delta BIGINT NOT NULL DEFAULT 0,
 created_at BIGINT UNSIGNED NOT NULL,
 settled_at BIGINT UNSIGNED NULL,
 PRIMARY KEY(session_id,event_key)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS spirit_shroud_corpse_state (
 corpse_id INT UNSIGNED NOT NULL PRIMARY KEY,
 character_id INT UNSIGNED NOT NULL,
 session_id BIGINT UNSIGNED NOT NULL,
 applied_loss BIGINT UNSIGNED NOT NULL,
 refunded_loss BIGINT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS spirit_shroud_allocations (
 session_id BIGINT UNSIGNED NOT NULL,
 ability_id INT UNSIGNED NOT NULL,
 rank_id INT UNSIGNED NOT NULL,
 paid_points INT UNSIGNED NOT NULL,
 PRIMARY KEY(session_id,ability_id)
) ENGINE=InnoDB;
