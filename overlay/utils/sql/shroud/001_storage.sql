-- Additive Spirit Shroud storage. Apply with all users offline before enabling.
-- Original rows survive independently until a checked atomic restoration.
CREATE TABLE IF NOT EXISTS spirit_shroud_state (
 character_id INT UNSIGNED NOT NULL PRIMARY KEY,
 template_id INT UNSIGNED NOT NULL DEFAULT 0,
 revision BIGINT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS spirit_shroud_progress (
 character_id INT UNSIGNED NOT NULL,
 progression_id INT UNSIGNED NOT NULL,
 experience BIGINT UNSIGNED NOT NULL DEFAULT 0,
 PRIMARY KEY(character_id,progression_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS spirit_shroud_templates (
 id INT UNSIGNED NOT NULL PRIMARY KEY,
 progression_id INT UNSIGNED NOT NULL, branch_id INT UNSIGNED NOT NULL,
 level INT UNSIGNED NOT NULL, race INT UNSIGNED NOT NULL, class_id INT UNSIGNED NOT NULL,
 gender INT UNSIGNED NOT NULL DEFAULT 2,
 hp INT UNSIGNED NOT NULL, mana INT UNSIGNED NOT NULL, endurance INT UNSIGNED NOT NULL,
 base_stat INT UNSIGNED NOT NULL, unlock_xp INT UNSIGNED NOT NULL DEFAULT 0,
 name VARCHAR(63) NOT NULL, description VARCHAR(2000) NOT NULL,
 progression_name VARCHAR(63) NOT NULL, branch_name VARCHAR(63) NOT NULL,
 enabled TINYINT UNSIGNED NOT NULL DEFAULT 1
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS spirit_shroud_spells (
 template_id INT UNSIGNED NOT NULL, slot_id INT UNSIGNED NOT NULL,
 spell_id INT UNSIGNED NOT NULL, PRIMARY KEY(template_id,slot_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS shroud_original_character_data LIKE character_data;
CREATE TABLE IF NOT EXISTS shroud_original_inventory LIKE inventory;
CREATE TABLE IF NOT EXISTS shroud_original_character_currency LIKE character_currency;
CREATE TABLE IF NOT EXISTS shroud_original_character_spells LIKE character_spells;
CREATE TABLE IF NOT EXISTS shroud_original_character_memmed_spells LIKE character_memmed_spells;
CREATE TABLE IF NOT EXISTS shroud_original_character_skills LIKE character_skills;
CREATE TABLE IF NOT EXISTS shroud_original_character_alternate_abilities LIKE character_alternate_abilities;
CREATE TABLE IF NOT EXISTS shroud_original_character_disciplines LIKE character_disciplines;
CREATE TABLE IF NOT EXISTS shroud_original_character_buffs LIKE character_buffs;
CREATE TABLE IF NOT EXISTS shroud_original_character_bandolier LIKE character_bandolier;
CREATE TABLE IF NOT EXISTS shroud_original_character_potionbelt LIKE character_potionbelt;
CREATE TABLE IF NOT EXISTS shroud_original_character_material LIKE character_material;
CREATE TABLE IF NOT EXISTS shroud_original_character_pet_info LIKE character_pet_info;
CREATE TABLE IF NOT EXISTS shroud_original_character_pet_buffs LIKE character_pet_buffs;
CREATE TABLE IF NOT EXISTS shroud_original_character_pet_inventory LIKE character_pet_inventory;
CREATE TABLE IF NOT EXISTS shroud_original_character_item_recast LIKE character_item_recast;
CREATE TABLE IF NOT EXISTS shroud_original_timers LIKE timers;
CREATE TABLE IF NOT EXISTS spirit_shroud_bank_inventory LIKE inventory;

CREATE TABLE IF NOT EXISTS spirit_shroud_abilities (
 template_id INT UNSIGNED NOT NULL, rank_id INT UNSIGNED NOT NULL,
 PRIMARY KEY(template_id,rank_id)
) ENGINE=InnoDB;
