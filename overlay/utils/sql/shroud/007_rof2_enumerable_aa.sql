-- RoF2 AA window enumerates numeric rank IDs 0 through 49998, even though
-- its cache accepts larger IDs. Move only the exact local 004 definitions from
-- 50000..50006 to 48000..48006. Apply offline after 001..006, stopping on errors.
-- Existing installations run this upgrade before reapplying revised 004.
-- Never use a batch client's --force option. Active Imp Wizard sessions must
-- be restored first. Retained session JSON, escrow, historical allocations and
-- all unrelated/ordinary AA rows remain unchanged.
CREATE TEMPORARY TABLE shroud_live_expected_ability LIKE aa_ability;
INSERT INTO shroud_live_expected_ability
 (id,name,category,classes,type,grant_only,first_rank_id) VALUES
 (48000,'Eyes Wide Open',3,2048,1,1,48000),
 (48001,'Fire',4,2048,1,1,48001),
 (48002,'Frost',4,2048,1,1,48002),
 (48003,'Thunder',4,2048,1,1,48003),
 (48004,'Flight',4,2048,1,1,48004),
 (48005,'Ultravision',4,2048,1,1,48005),
 (48006,'Spirit Sending',4,2048,1,1,48006);
CREATE TEMPORARY TABLE shroud_live_expected_rank LIKE aa_ranks;
INSERT INTO shroud_live_expected_rank
 (id,upper_hotkey_sid,lower_hotkey_sid,title_sid,desc_sid,cost,level_req,spell,spell_type,recast_time) VALUES
 (48000,12636,12636,12636,12636,0,1,-1,0,0),
 (48001,3231,3231,3231,3231,0,5,7414,1,4),
 (48002,3245,3245,3245,3245,0,5,7428,1,4),
 (48003,3259,3259,3259,3259,0,5,7442,1,4),
 (48004,3482,3482,3482,3482,0,1,7530,1,8),
 (48005,4223,4223,4223,4223,0,1,8244,27,8),
 (48006,3577,3577,3577,3577,0,1,7790,9,900);
CREATE TEMPORARY TABLE shroud_live_expected_template LIKE spirit_shroud_templates;
INSERT INTO shroud_live_expected_template
 (id,progression_id,branch_id,level,race,class_id,gender,hp,mana,endurance,
  base_stat,unlock_xp,name,description,progression_name,branch_name,
  enabled,content_version,verified,test_only,evidence_ref,monster_points,
  stat_str,stat_sta,stat_cha,stat_dex,stat_int,stat_agi,stat_wis,resource_maxima,model_size)
VALUES
 (20105,2,1,5,464,12,2,93,152,100,0,0,'Imp Wizard 5',
  'Modern Live resources, attributes and free Monster abilities; emulator combat formulas.',
  'Aberrations','Imp Wizard',1,2026102401,1,0,
  'Live 2026-10-24: Imp Wizard 5 selection, active stats and all seven initial AA ranks',
  0,58,69,67,66,143,80,81,1,2.708332777);
CREATE TEMPORARY TABLE shroud_live_migration_guard
 (ok TINYINT NOT NULL CHECK (ok=1)) ENGINE=InnoDB;

START TRANSACTION;
INSERT INTO shroud_live_migration_guard SELECT IF(
 NOT EXISTS(SELECT 1 FROM spirit_shroud_state WHERE template_id=20105)
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_sessions WHERE template_id=20105 AND phase='active')
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_templates WHERE progression_id=2 AND id<>20105)
 AND NOT EXISTS(SELECT 1 FROM aa_ability a LEFT JOIN shroud_live_expected_ability e ON e.id=a.id
   WHERE a.first_rank_id BETWEEN 48000 AND 48006 AND e.id IS NULL)
 AND NOT EXISTS(SELECT 1 FROM aa_ability a JOIN shroud_live_expected_ability e ON e.id=a.id
   WHERE a.name<>e.name OR a.category<>e.category OR a.classes<>e.classes
    OR a.type<>e.type OR a.grant_only<>e.grant_only OR a.first_rank_id<>e.first_rank_id
    OR a.charges<>0 OR a.enabled<>1 OR a.status<>0
    OR a.races<>e.races OR a.deities<>e.deities OR a.drakkin_heritage<>e.drakkin_heritage
    OR a.reset_on_death<>e.reset_on_death OR a.auto_grant_enabled<>e.auto_grant_enabled)
 AND NOT EXISTS(SELECT 1 FROM aa_ranks a JOIN shroud_live_expected_rank e ON e.id=a.id
   WHERE a.upper_hotkey_sid<>e.upper_hotkey_sid OR a.lower_hotkey_sid<>e.lower_hotkey_sid
    OR a.title_sid<>e.title_sid OR a.desc_sid<>e.desc_sid OR a.cost<>e.cost
    OR a.level_req<>e.level_req OR a.spell<>e.spell OR a.spell_type<>e.spell_type
    OR a.recast_time<>e.recast_time OR a.prev_id<>-1 OR a.next_id<>-1 OR a.expansion<>e.expansion)
 AND NOT EXISTS(SELECT 1 FROM aa_rank_effects WHERE rank_id BETWEEN 48000 AND 48006
   AND NOT(rank_id=48000 AND slot=1 AND effect_id=426 AND base1=8 AND base2=0))
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_templates a JOIN shroud_live_expected_template e ON e.id=a.id
   WHERE a.progression_id<>e.progression_id OR a.branch_id<>e.branch_id OR a.level<>e.level
    OR a.race<>e.race OR a.class_id<>e.class_id OR a.gender<>e.gender
    OR a.hp<>e.hp OR a.mana<>e.mana OR a.endurance<>e.endurance
    OR a.name<>e.name OR a.content_version NOT IN (20261024,2026102401) OR a.test_only<>0
    OR a.verified<>1 OR a.resource_maxima<>1 OR NOT(a.model_size<=>e.model_size) OR a.enabled<>1
    OR a.monster_points<>0 OR a.unlock_xp<>0 OR a.base_stat<>0
    OR NOT(a.stat_str<=>e.stat_str) OR NOT(a.stat_sta<=>e.stat_sta)
    OR NOT(a.stat_cha<=>e.stat_cha) OR NOT(a.stat_dex<=>e.stat_dex)
    OR NOT(a.stat_int<=>e.stat_int) OR NOT(a.stat_agi<=>e.stat_agi)
    OR NOT(a.stat_wis<=>e.stat_wis) OR a.entitlement_key IS NOT NULL
    OR a.native_hp IS NOT NULL OR a.native_mana IS NOT NULL OR a.native_endurance IS NOT NULL
    OR a.native_hp_regen IS NOT NULL OR a.native_mana_regen_sitting IS NOT NULL
    OR a.native_mana_regen_standing IS NOT NULL OR a.native_endurance_regen IS NOT NULL
    OR a.native_cold IS NOT NULL OR a.native_fire IS NOT NULL OR a.native_magic IS NOT NULL
    OR a.native_disease IS NOT NULL OR a.native_poison IS NOT NULL
    OR a.native_additional_resist IS NOT NULL OR a.native_corruption IS NOT NULL
    OR a.native_ac IS NOT NULL OR a.native_attack IS NOT NULL
    OR a.native_unarmed_damage IS NOT NULL OR a.native_unarmed_delay IS NOT NULL)
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_abilities WHERE template_id=20105
   AND ((rank_id NOT BETWEEN 48000 AND 48006 AND rank_id NOT BETWEEN 50000 AND 50006) OR free_grant<>1))
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_spells WHERE template_id=20105)
 AND NOT EXISTS(SELECT 1 FROM aa_rank_prereqs WHERE rank_id BETWEEN 48000 AND 48006 OR aa_id BETWEEN 48000 AND 48006)
 AND NOT EXISTS(SELECT 1 FROM aa_ranks WHERE prev_id BETWEEN 48000 AND 48006 OR next_id BETWEEN 48000 AND 48006)
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_abilities WHERE template_id<>20105 AND rank_id BETWEEN 48000 AND 48006)
 -- The legacy content revision requires its complete old namespace. The new
 -- revision is accepted only as a completed, coherent target (idempotent rerun).
 AND EXISTS(SELECT 1 FROM spirit_shroud_templates t WHERE t.id=20105 AND
  ((t.content_version=20261024 AND
    (SELECT COUNT(*) FROM aa_ability WHERE id BETWEEN 50000 AND 50006)=7)
   OR (t.content_version=2026102401
    AND NOT EXISTS(SELECT 1 FROM aa_ability WHERE id BETWEEN 50000 AND 50006)
    AND (SELECT COUNT(*) FROM aa_ability WHERE id BETWEEN 48000 AND 48006)=7
    AND (SELECT COUNT(*) FROM spirit_shroud_abilities WHERE template_id=20105 AND rank_id BETWEEN 48000 AND 48006 AND free_grant=1)=7
    AND (SELECT COUNT(*) FROM aa_rank_effects WHERE rank_id=48000 AND slot=1 AND effect_id=426 AND base1=8 AND base2=0)=1)))
 -- Old namespace must be either absent or the complete, unmodified 004 set.
 AND (NOT EXISTS(SELECT 1 FROM aa_ability WHERE id BETWEEN 50000 AND 50006)
   AND NOT EXISTS(SELECT 1 FROM aa_ranks WHERE id BETWEEN 50000 AND 50006)
   AND NOT EXISTS(SELECT 1 FROM aa_rank_effects WHERE rank_id BETWEEN 50000 AND 50006)
   AND NOT EXISTS(SELECT 1 FROM spirit_shroud_abilities WHERE rank_id BETWEEN 50000 AND 50006)
   AND NOT EXISTS(SELECT 1 FROM character_alternate_abilities WHERE aa_id BETWEEN 50000 AND 50006)
  OR (SELECT COUNT(*) FROM aa_ability WHERE id BETWEEN 50000 AND 50006)=7
   AND (SELECT COUNT(*) FROM aa_ranks WHERE id BETWEEN 50000 AND 50006)=7
   AND (SELECT COUNT(*) FROM aa_rank_effects WHERE rank_id BETWEEN 50000 AND 50006)=1
   AND (SELECT COUNT(*) FROM spirit_shroud_abilities WHERE template_id=20105 AND rank_id BETWEEN 50000 AND 50006 AND free_grant=1)=7)
 AND NOT EXISTS(SELECT 1 FROM aa_ability a JOIN shroud_live_expected_ability e ON a.id=e.id+2000
   WHERE NOT(a.`name`<=>e.`name`) OR NOT(a.`category`<=>e.`category`) OR NOT(a.`classes`<=>e.`classes`) OR NOT(a.`races`<=>e.`races`) OR NOT(a.`drakkin_heritage`<=>e.`drakkin_heritage`) OR NOT(a.`deities`<=>e.`deities`) OR NOT(a.`status`<=>e.`status`) OR NOT(a.`type`<=>e.`type`) OR NOT(a.`charges`<=>e.`charges`) OR NOT(a.`grant_only`<=>e.`grant_only`) OR NOT(a.`enabled`<=>e.`enabled`) OR NOT(a.`reset_on_death`<=>e.`reset_on_death`) OR NOT(a.`auto_grant_enabled`<=>e.`auto_grant_enabled`) OR a.first_rank_id<>e.first_rank_id+2000)
 AND NOT EXISTS(SELECT 1 FROM aa_ranks a JOIN shroud_live_expected_rank e ON a.id=e.id+2000
   WHERE NOT(a.`upper_hotkey_sid`<=>e.`upper_hotkey_sid`) OR NOT(a.`lower_hotkey_sid`<=>e.`lower_hotkey_sid`) OR NOT(a.`title_sid`<=>e.`title_sid`) OR NOT(a.`desc_sid`<=>e.`desc_sid`) OR NOT(a.`cost`<=>e.`cost`) OR NOT(a.`level_req`<=>e.`level_req`) OR NOT(a.`spell`<=>e.`spell`) OR NOT(a.`spell_type`<=>e.`spell_type`) OR NOT(a.`recast_time`<=>e.`recast_time`) OR NOT(a.`expansion`<=>e.`expansion`) OR NOT(a.`prev_id`<=>e.`prev_id`) OR NOT(a.`next_id`<=>e.`next_id`))
 AND NOT EXISTS(SELECT 1 FROM aa_rank_effects WHERE rank_id BETWEEN 50000 AND 50006
   AND NOT(rank_id=50000 AND slot=1 AND effect_id=426 AND base1=8 AND base2=0))
 -- Do not repurpose references belonging to ordinary or unrelated content.
 AND NOT EXISTS(SELECT 1 FROM aa_ability WHERE
   (first_rank_id BETWEEN 50000 AND 50006 OR first_rank_id BETWEEN 48000 AND 48006)
   AND id NOT BETWEEN 50000 AND 50006 AND id NOT BETWEEN 48000 AND 48006)
 AND NOT EXISTS(SELECT 1 FROM aa_ranks WHERE
   (prev_id BETWEEN 50000 AND 50006 OR next_id BETWEEN 50000 AND 50006
    OR prev_id BETWEEN 48000 AND 48006 OR next_id BETWEEN 48000 AND 48006))
 -- The exact seven free single-rank definitions have no prerequisites.
 -- Refuse custom prerequisite edits rather than changing another AA's semantics.
 AND NOT EXISTS(SELECT 1 FROM aa_rank_prereqs WHERE rank_id BETWEEN 48000 AND 48006
   OR rank_id BETWEEN 50000 AND 50006 OR aa_id BETWEEN 48000 AND 48006 OR aa_id BETWEEN 50000 AND 50006)
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_abilities WHERE template_id<>20105
   AND (rank_id BETWEEN 48000 AND 48006 OR rank_id BETWEEN 50000 AND 50006))
 AND (SELECT COUNT(*) FROM spirit_shroud_templates WHERE id=20105)=1
 AND ((SELECT COUNT(*) FROM aa_ability WHERE id BETWEEN 48000 AND 48006)=0
   OR (SELECT COUNT(*) FROM aa_ability WHERE id BETWEEN 48000 AND 48006)=7)
 AND (SELECT COUNT(*) FROM aa_ability WHERE id BETWEEN 48000 AND 48006)=
   (SELECT COUNT(*) FROM aa_ranks WHERE id BETWEEN 48000 AND 48006)
 -- Existing ownership is remapped only when attributable to a completely
 -- restored 004 shroud, with the exact seven free ranks and no destination rows.
 AND NOT EXISTS(SELECT 1 FROM character_alternate_abilities a
  LEFT JOIN spirit_shroud_state s ON s.character_id=a.id
  WHERE a.aa_id BETWEEN 50000 AND 50006 AND
   (s.character_id IS NULL OR s.template_id<>0 OR s.active_session_id IS NOT NULL
    OR a.aa_value<>1 OR a.charges<>0
    OR NOT EXISTS(SELECT 1 FROM spirit_shroud_sessions h WHERE h.character_id=a.id AND h.template_id=20105 AND h.phase='restored')
    OR EXISTS(SELECT 1 FROM spirit_shroud_sessions h WHERE h.character_id=a.id AND h.phase='active')
    OR (SELECT COUNT(*) FROM character_alternate_abilities b WHERE b.id=a.id AND b.aa_id BETWEEN 50000 AND 50006)<>7
    OR EXISTS(SELECT 1 FROM character_alternate_abilities b WHERE b.id=a.id AND b.aa_id BETWEEN 48000 AND 48006)))
 AND NOT EXISTS(SELECT 1 FROM spirit_shroud_allocations a JOIN spirit_shroud_sessions s ON s.id=a.session_id
   WHERE s.phase='active' AND (a.rank_id BETWEEN 50000 AND 50006 OR a.ability_id BETWEEN 50000 AND 50006))
,1,0);


INSERT INTO aa_ability SELECT * FROM shroud_live_expected_ability ON DUPLICATE KEY UPDATE id=VALUES(id);
INSERT INTO aa_ranks SELECT * FROM shroud_live_expected_rank ON DUPLICATE KEY UPDATE id=VALUES(id);
INSERT INTO aa_rank_effects(rank_id,slot,effect_id,base1,base2)
 VALUES(48000,1,426,8,0) ON DUPLICATE KEY UPDATE rank_id=VALUES(rank_id);
-- No prerequisites are expected; both rank and ability reference domains are
-- nevertheless mapped explicitly after the guards above.
UPDATE aa_rank_prereqs SET rank_id=IF(rank_id BETWEEN 50000 AND 50006,rank_id-2000,rank_id),
 aa_id=IF(aa_id BETWEEN 50000 AND 50006,aa_id-2000,aa_id)
 WHERE rank_id BETWEEN 50000 AND 50006 OR aa_id BETWEEN 50000 AND 50006;
INSERT INTO spirit_shroud_abilities(template_id,rank_id,free_grant)
 SELECT 20105,id,1 FROM shroud_live_expected_rank ON DUPLICATE KEY UPDATE rank_id=VALUES(rank_id);
UPDATE character_alternate_abilities SET aa_id=aa_id-2000 WHERE aa_id BETWEEN 50000 AND 50006;
UPDATE spirit_shroud_templates SET content_version=2026102401 WHERE id=20105;
DELETE FROM spirit_shroud_abilities WHERE template_id=20105 AND rank_id BETWEEN 50000 AND 50006;
DELETE FROM aa_rank_effects WHERE rank_id BETWEEN 50000 AND 50006;
DELETE FROM aa_ranks WHERE id BETWEEN 50000 AND 50006;
DELETE FROM aa_ability WHERE id BETWEEN 50000 AND 50006;
COMMIT;
DROP TEMPORARY TABLE shroud_live_migration_guard;
DROP TEMPORARY TABLE shroud_live_expected_template;
DROP TEMPORARY TABLE shroud_live_expected_rank;
DROP TEMPORARY TABLE shroud_live_expected_ability;
