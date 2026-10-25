-- Apply offline before enabling the updated Spirit Shroud service. Sessions
-- already active before this migration remain removable, but cannot earn new
-- pending original-profile XP until restored and entered again.
ALTER TABLE spirit_shroud_sessions
 ADD COLUMN IF NOT EXISTS experience_version INT UNSIGNED NOT NULL DEFAULT 0;
CREATE TABLE IF NOT EXISTS spirit_shroud_experience_state (
 session_id BIGINT UNSIGNED NOT NULL PRIMARY KEY,
 normal_exp BIGINT UNSIGNED NOT NULL,
 aa_exp BIGINT UNSIGNED NOT NULL,
 aa_points INT UNSIGNED NOT NULL,
 aa_spent INT UNSIGNED NOT NULL,
 level INT UNSIGNED NOT NULL,
 level2 INT UNSIGNED NOT NULL,
 skill_points INT UNSIGNED NOT NULL,
 percent_to_aa INT UNSIGNED NOT NULL,
 aa_unit INT UNSIGNED NOT NULL,
 max_level INT UNSIGNED NOT NULL,
 thresholds_json MEDIUMTEXT NOT NULL,
 settled_at BIGINT UNSIGNED NULL
) ENGINE=InnoDB;
