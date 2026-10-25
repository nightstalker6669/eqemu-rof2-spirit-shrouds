#include "client.h"
#include "spirit_shroud_destination.h"
#include "zonedb.h"
#include "zone.h"
#include "entity.h"
#include "housing_item_reconstruction.h"
#include "fellowship_campsite_bonuses.h"
#include "../common/patches/rof2.h"
#include "../common/shroud/native_transform.h"
#include "../common/shroud/catalog.h"
#include "../common/misc_functions.h"
#include <cmath>
#include "../common/shroud/aa_preview.h"
#include "../common/repositories/character_buffs_repository.h"
#include "../common/repositories/character_alternate_abilities_repository.h"
#include "../common/repositories/character_disciplines_repository.h"
#include "../common/repositories/inventory_repository.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

extern Zone* zone;
namespace {
uint64_t NativeNumber(const char* value) {
 if(!value || !*value || *value=='-')throw std::runtime_error("native shroud missing number");
 std::string text(value);size_t used=0;auto n=std::stoull(text,&used);
 if(used!=text.size())throw std::runtime_error("native shroud invalid number");return n;
}
auto NativeQuery(ZoneDatabase& db,const std::string& sql) {
 if(db.GetStatus()!=DBcore::Connected)throw std::runtime_error("native shroud disconnected database");
 auto r=db.QueryDatabase(sql,false);if(!r.Success())throw std::runtime_error("native shroud query failed");return r;
}
bool NativeRoot(uint32_t slot) {
 return slot<=EQ::invslot::slotCursor || (slot>=EQ::invslot::BANK_BEGIN && slot<=EQ::invslot::BANK_END);
}
Housing::LiveItems::InventoryRow NativeRow(const InventoryRepository::LoadRow& load) {
 Housing::LiveItems::InventoryRow row;row.values=load.values;
 if(!load.fields[2])row.null_fields|=Housing::LiveItems::NullItemId;
 if(!load.fields[3])row.null_fields|=Housing::LiveItems::NullCharges;
 if(!load.fields[12])row.null_fields|=Housing::LiveItems::NullCustomData;
 if(!load.fields[16])row.null_fields|=Housing::LiveItems::NullGuid;
 return row;
}
// The same equipment-material rules as Mob::GetEquipmentMaterial, evaluated
// against the prepared inventory rather than the still-active Client.
uint32_t NativeMaterial(const EQ::ItemInstance& item,unsigned material) {
 const auto* data=item.GetItem();if(!data)throw std::runtime_error("native shroud missing equipment");
 if(material<EQ::textures::weaponPrimary)return data->Material;
 if(auto* augment=item.GetOrnamentationAugment())data=augment->GetItem();
 else if(item.GetOrnamentationIDFile())return item.GetOrnamentationIDFile();
 if(data && std::strlen(data->IDFile)>2 && Strings::IsNumber(data->IDFile+2))return Strings::ToUnsignedInt(data->IDFile+2);
 return 0;
}
void NativeInnates(PlayerProfile_Struct& profile);
bool NativePreparationFallback(const char* stage) {
 // Only known, valid state whose full actor/wire reconstruction is unsupported
 // may select the zone reload path. Every unlisted failure aborts the enclosing
 // profile transaction rather than committing an incompletely prepared image.
 for(const auto* allowed:{"client-version","permanent-proc","saved-pet",
     "legacy-session","inventory-capacity","buff-actor-effect",
     "buff-proc-capacity","equipment-hero-forge","buff-counter-rune"})
  if(std::strcmp(stage,allowed)==0)return true;
 return false;
}
}

std::unique_ptr<SpiritShroudDestination> Client::PrepareSpiritShroudDestination(ZoneDatabase& db) {
 const auto failures=db.QueryFailureGeneration();
 auto reject=[this,&db,failures](const char* stage)->std::unique_ptr<SpiritShroudDestination> {
  if(failures!=db.QueryFailureGeneration())throw std::runtime_error("native shroud preparation query failure before fallback");
  if(!NativePreparationFallback(stage))throw std::runtime_error(std::string("native shroud preparation failed: ")+stage);
  LogInfo("Native shroud reload fallback character [{}] stage [{}]",CharacterID(),stage);
  return {};
 };
 try {
  if(!zone || !GetID() || IsCasting())return reject("client-state");
  if(ClientVersion()!=EQ::versions::ClientVersion::RoF2)return reject("client-version");
  // Quest/NPC-only permanent proc actors are not part of the saved player
  // profile. Keep such actors on the existing complete reload lifecycle.
  for(int i=0;i<m_max_procs;++i)if(IsValidSpell(PermaProcs[i].spellID))return reject("permanent-proc");
  auto connection=NativeQuery(db,"SELECT CONNECTION_ID(),@@in_transaction");
  if(connection.RowCount()!=1 || NativeNumber(connection.begin()[1])!=1)return reject("transaction-not-active");
  const auto identity=NativeNumber(connection.begin()[0]);
  auto out=std::make_unique<SpiritShroudDestination>();
  // Session-invariant settings (expansion entitlement, guild, group names, etc.)
  // retain their current values; every swapped domain is reloaded below.
  out->profile=m_pp;out->extended=m_epp;
  auto& pp=out->profile;
  const auto character=std::to_string(CharacterID());
  for(const auto& check:std::vector<std::pair<std::string,std::string>>{
   {"character_bandolier","bandolier_id>="+std::to_string(EQ::profile::BANDOLIERS_SIZE)+" OR bandolier_slot>="+std::to_string(EQ::profile::BANDOLIER_ITEM_COUNT)},
   {"character_potionbelt","potion_id>="+std::to_string(EQ::profile::POTION_BELT_SIZE)},
   {"character_material","slot>="+std::to_string(EQ::textures::materialCount)}}){
   auto invalid=NativeQuery(db,"SELECT COUNT(*) FROM "+check.first+" WHERE id="+character+" AND ("+check.second+")");if(invalid.RowCount()!=1 || NativeNumber(invalid.begin()[0]))return reject("profile-row-index");
  }
  // Pet cache restoration requires its own durable actor lifecycle. Until that
  // DTO is included, retain the complete normal reload behavior for such state.
  auto pets=NativeQuery(db,"SELECT COUNT(*) FROM character_pet_info WHERE char_id="+character+" AND spell_id<>0");
  if(pets.RowCount()!=1)return reject("saved-pet-count");
  if(NativeNumber(pets.begin()[0]))return reject("saved-pet");

  std::memset(pp.aa_array,0,sizeof(pp.aa_array));
  std::memset(pp.item_tint.Slot,0,sizeof(pp.item_tint.Slot));
  std::memset(pp.item_material.Slot,0,sizeof(pp.item_material.Slot));
  std::memset(pp.disciplines.values,0,sizeof(pp.disciplines.values));
  if(!db.LoadCharacterData(CharacterID(),&pp,&out->extended) ||
     !db.LoadCharacterCurrency(CharacterID(),&pp) || !db.LoadCharacterSkills(CharacterID(),&pp) ||
     !db.LoadCharacterSpellBook(CharacterID(),&pp) || !db.LoadCharacterMemmedSpells(CharacterID(),&pp) ||
     !db.LoadCharacterBindPoint(CharacterID(),&pp) || !db.LoadCharacterBandolier(CharacterID(),&pp) ||
     !db.LoadCharacterMaterialColor(CharacterID(),&pp) || !db.LoadCharacterPotionBelt(CharacterID(),&pp))return reject("profile-loader");
  // These domains are not exchanged by the shroud service. They stay attached
  // to the same character and do not need a second, lossy normalization pass.
  if(!pp.level || pp.level>126 || pp.class_<1 || pp.class_>16 || !pp.race || pp.cur_hp<=0)return reject("profile-identity-or-hp");
  pp.x=GetX();pp.y=GetY();pp.z=GetZ();pp.heading=GetHeading();
  pp.zone_id=zone->GetZoneID();pp.zoneInstance=zone->GetInstanceID();pp.entityid=GetID();
  auto state=NativeQuery(db,"SELECT template_id,revision,active_session_id FROM spirit_shroud_state WHERE character_id="+std::to_string(CharacterID())+" FOR UPDATE");
  if(state.RowCount()!=1)return reject("state-row");
  out->template_id=NativeNumber(state.begin()[0]);out->revision=NativeNumber(state.begin()[1]);
  if(state.begin()[2])out->session_id=NativeNumber(state.begin()[2]);
  pp.shroud_native_stats_valid=false;pp.shroud_native_resources_valid=false;
  std::fill(std::begin(pp.shroud_native_stats),std::end(pp.shroud_native_stats),0);
  pp.unknown0112=out->template_id;
  if(out->template_id){
   if(!out->session_id)return reject("legacy-session"); // Legacy recovery remains on the proven reload path.
   auto retained=NativeQuery(db,"SELECT template_json,inventory_policy,entry_revision,content_version FROM spirit_shroud_sessions WHERE id="+std::to_string(out->session_id)+" AND character_id="+std::to_string(CharacterID())+" AND phase='active'");
   if(retained.RowCount()!=1 || !retained.begin()[0] || NativeNumber(retained.begin()[1])>1 || NativeNumber(retained.begin()[2])!=out->revision)return reject("retained-session");
   auto form=SpiritShroud::RestoreSnapshot(retained.begin()[0]);
   if(form.id!=out->template_id || form.content_version!=NativeNumber(retained.begin()[3]) || form.class_id!=pp.class_ || form.level!=pp.level || form.race!=pp.race)return reject("retained-profile-mismatch");
   out->personal_bank=NativeNumber(retained.begin()[1])==1;
   pp.shroud_native_stats_valid=form.has_native_stats;pp.shroud_native_resources_valid=form.has_native_resources || form.has_native_stats;
   std::copy(form.native_stats.begin(),form.native_stats.end(),pp.shroud_native_stats);
   out->allowed_ranks.insert(form.abilities.begin(),form.abilities.end());
   out->definition=std::make_shared<const SpiritShroud::Template>(std::move(form));
  }else if(out->session_id)return reject("original-session-not-cleared");
  out->model_size=out->definition && out->definition->model_size>0?out->definition->model_size:GetRaceGenderDefaultHeight(pp.race,pp.gender);
  if(!std::isfinite(out->model_size) || out->model_size<=0 || out->model_size>100)return reject("model-size");

  auto inventory=InventoryRepository::GetLoadSnapshot(db,CharacterID(),true);
  if(!inventory.success)return reject("inventory-snapshot");
  if(inventory.rows.size()>1000)return reject("inventory-capacity");
  std::map<uint32_t,Housing::LiveItems::InventoryRow> roots;
  for(const auto& load:inventory.rows)if(NativeRoot(load.values.slot_id))roots.emplace(load.values.slot_id,NativeRow(load));
  for(const auto& load:inventory.rows){
   if(NativeRoot(load.values.slot_id))continue;
   const auto slot=load.values.slot_id;
   if(!((slot>=EQ::invbag::GENERAL_BAGS_BEGIN && slot<=EQ::invbag::CURSOR_BAG_END) || (slot>=EQ::invbag::BANK_BAGS_BEGIN && slot<=EQ::invbag::BANK_BAGS_END)))return reject("inventory-slot-domain");
   const auto parent=EQ::InventoryProfile::CalcSlotId(slot);auto found=roots.find(parent);if(found==roots.end())return reject("inventory-orphan-child");
   auto child=NativeRow(load);child.values.slot_id=EQ::InventoryProfile::CalcBagIdx(slot);found->second.children.push_back(std::move(child));
  }
  out->inventory.SetInventoryVersion(ClientVersion());out->inventory.SetGMInventory(m_inv.GMInventory());
  std::vector<std::pair<int16_t,const EQ::ItemInstance*>> wire_items;
  for(auto& [slot,row]:roots){
   // Global database is used ONLY as the immutable item-definition factory.
   // Every ownership, instance and child row above came from the transaction.
   auto rebuilt=Housing::InventoryBridge::ReconstructCharacterProfile(database,row);if(!rebuilt)return reject("inventory-reconstruction");
   if(out->inventory.PushItem(slot,rebuilt.value.item.get())!=int16_t(slot))return reject("inventory-install-slot");
   rebuilt.value.item.release();wire_items.emplace_back(slot,out->inventory.GetItem(slot));
  }
  if(!out->template_id)for(int slot=EQ::invslot::SHARED_BANK_BEGIN;slot<=EQ::invslot::SHARED_BANK_END;++slot)
   if(auto* item=m_inv.GetItem(slot))wire_items.emplace_back(slot,item);
  out->timers.SetCharID(CharacterID());if(!out->timers.Load(&db))return reject("timer-loader");
  for(unsigned i=0;i<EQ::spells::SPELL_GEM_COUNT;++i)pp.spellSlotRefresh[i]=IsValidSpell(pp.mem_spells[i])?out->timers.GetRemainingTime(pTimerSpellStart+pp.mem_spells[i])*1000:0;
  std::fill(std::begin(pp.recastTimers),std::end(pp.recastTimers),0); // Matches the existing normal-profile loader.
  pp.abilitySlotRefresh=0;
  if(pp.class_==Class::ShadowKnight || pp.class_==Class::Paladin){auto remaining=out->timers.GetRemainingTime(pp.class_==Class::ShadowKnight?pTimerHarmTouch:pTimerLayHands);if(remaining<15300)pp.abilitySlotRefresh=remaining*1000;}
  auto disciplines=CharacterDisciplinesRepository::GetWhere(db,"id="+std::to_string(CharacterID()));
  for(const auto& e:disciplines){if(e.slot_id>=MAX_PP_DISCIPLINES || !IsValidSpell(e.disc_id))return reject("discipline-row");pp.disciplines.values[e.slot_id]=e.disc_id;}

  out->buffs.resize(GetMaxTotalSlots());for(auto& b:out->buffs)b.spellid=SPELL_UNKNOWN;
  auto buff_rows=CharacterBuffsRepository::GetWhere(db,"character_id="+std::to_string(CharacterID()));
  std::set<unsigned> buff_slots;
  for(const auto& e:buff_rows){
   if(IsFellowshipCampsiteAuraBuff(e.spell_id,e.caster_name.data(),e.caster_name.size()))continue;
   if(e.slot_id>=out->buffs.size() || !buff_slots.insert(e.slot_id).second || !IsValidSpell(e.spell_id) || e.dot_rune<0 || e.instrument_mod<0)return reject("buff-row");
   // RoF2's per-effect slot_data bookkeeping is not represented by the
   // ordinary scalar counter encoder. Preserve it through normal login.
   if(e.counters || e.melee_rune || e.magic_rune || e.dot_rune)return reject("buff-counter-rune");
   // Transformation/ownership effects need additional actor-state preparation.
   // The reload fallback already handles those; do not guess their cache state.
   for(auto effect:{SpellEffect::Charm,SpellEffect::Illusion,SpellEffect::SummonHorse})if(IsEffectInSpell(e.spell_id,effect))return reject("buff-actor-effect");
   auto& b=out->buffs[e.slot_id];b.spellid=e.spell_id;b.casterlevel=e.caster_level;
   if(auto* caster=entity_list.GetClientByName(e.caster_name.c_str())){b.casterid=caster->GetID();b.client=true;strn0cpy(b.caster_name,caster->GetName(),sizeof(b.caster_name));}
   b.ticsremaining=e.ticsremaining;b.counters=e.counters;b.hit_number=e.numhits;b.melee_rune=e.melee_rune;b.magic_rune=e.magic_rune;b.dot_rune=e.dot_rune;
   b.persistant_buff=e.persistent!=0;b.caston_x=e.caston_x;b.caston_y=e.caston_y;b.caston_z=e.caston_z;b.ExtraDIChance=e.ExtraDIChance;b.instrument_mod=e.instrument_mod;
  }
  for(auto& b:pp.buffs){b={};b.spellid=SPELLBOOK_UNKNOWN;b.bard_modifier=10;}
  int melee_procs=0,defensive_procs=0,ranged_procs=0;
  for(const auto& buff:out->buffs)if(IsValidSpell(buff.spellid)){
   for(int i=0;i<EFFECT_COUNT;++i){
    const auto effect=spells[buff.spellid].effect_id[i];
    if(effect==SpellEffect::AddMeleeProc || effect==SpellEffect::WeaponProc ||
       effect==SpellEffect::DefensiveProc || effect==SpellEffect::RangedProc){
     if(!IsValidSpell(GetProcID(buff.spellid,i)))return reject("buff-proc-spell");
     if(effect==SpellEffect::DefensiveProc)++defensive_procs;
     else if(effect==SpellEffect::RangedProc)++ranged_procs;
     else ++melee_procs;
    }
   }
  }
  if(melee_procs>m_max_procs || defensive_procs>m_max_procs || ranged_procs>m_max_procs)return reject("buff-proc-capacity");
  for(unsigned i=0;i<std::size(pp.buffs) && i<out->buffs.size();++i){const auto& b=out->buffs[i];if(!IsValidSpell(b.spellid))continue;auto& wire=pp.buffs[i];wire.spellid=b.spellid;wire.bard_modifier=b.instrument_mod;wire.effect_type=2;wire.player_id=0x2211;wire.level=b.casterlevel;wire.duration=b.ticsremaining;wire.counters=b.counters;wire.num_hits=b.hit_number;}
  auto owned=CharacterAlternateAbilitiesRepository::GetWhere(db,"id="+std::to_string(CharacterID()));
  unsigned ordinal=0;
  for(const auto& e:owned){
   auto* first=zone->GetAlternateAdvancementRank(e.aa_id);if(!first || !first->base_ability || !e.aa_value || ordinal>=MAX_PP_AA_ARRAY)return reject("owned-aa-row");
   auto* ability=first->base_ability;auto* rank=ability->GetRankByPointsSpent(e.aa_value);if(!rank)return reject("owned-aa-rank");
   if(out->definition){if(!SpiritShroud::PreviewRange(*out->definition,rank).valid)return reject("owned-aa-not-offered");}
   else {
    if(ability->category==3 || ability->category==4 || !(ability->classes & (1U<<pp.class_)))continue;
    const auto expansion=RuleI(Expansion,CurrentExpansion);
    if(RuleB(Expansion,UseCurrentExpansionAAOnly) && expansion>=0 && rank->expansion>expansion)continue;
    if(rank->expansion && (rank->expansion>32 || !(pp.expansions & (1U<<(rank->expansion-1)))))continue;
    auto race_index=GetPlayerRaceValue(pp.race);race_index=race_index>RaceIndex::Drakkin?Race::Human:race_index;
    if(!race_index || !(ability->races & (1U<<(race_index-1))) || !(ability->deities & Deity::GetBitmask(pp.deity)) || Admin()<ability->status)continue;
    if(pp.race==Race::Drakkin && !(ability->drakkin_heritage & (1U<<pp.drakkin_heritage)))continue;
   }
   if(!out->aa_ranks.emplace(ability->id,std::make_pair(e.aa_value,e.charges)).second)return reject("owned-aa-duplicate");
   pp.aa_array[ordinal].AA=e.aa_id;pp.aa_array[ordinal].value=e.aa_value;pp.aa_array[ordinal].charges=e.charges;++ordinal;
  }
  NativeInnates(pp);
  Spawn_Struct spawn{};
  strn0cpy(spawn.name,pp.name,sizeof(spawn.name));strn0cpy(spawn.lastName,pp.last_name,sizeof(spawn.lastName));strn0cpy(spawn.title,pp.title,sizeof(spawn.title));strn0cpy(spawn.suffix,pp.suffix,sizeof(spawn.suffix));
  spawn.spawnId=GetID();spawn.race=pp.race;spawn.gender=pp.gender;spawn.class_=pp.class_;spawn.level=pp.level;spawn.deity=pp.deity;spawn.size=out->model_size;
  spawn.x=FloatToEQ19(GetX());spawn.y=FloatToEQ19(GetY());spawn.z=FloatToEQ19(GetZ());spawn.heading=FloatToEQ12(GetHeading());
  // The native replacement is still a player. A nonzero type enters the
  // client's NPC name path while 0x6562 has cleared its local-player pointer.
  spawn.NPC=0;spawn.max_hp=100;spawn.curHp=100;spawn.show_name=true;spawn.StandState=GetAppearanceValue(GetAppearance());spawn.runspeed=runspeed;spawn.walkspeed=walkspeed;
  spawn.guildID=GuildID();spawn.guildrank=GuildRank();spawn.gm=GetGM();spawn.anon=pp.anon;spawn.showhelm=pp.showhelm;
  spawn.face=pp.face;spawn.haircolor=pp.haircolor;spawn.hairstyle=pp.hairstyle;spawn.beard=pp.beard;spawn.beardcolor=pp.beardcolor;spawn.eyecolor1=pp.eyecolor1;spawn.eyecolor2=pp.eyecolor2;
  spawn.drakkin_heritage=pp.drakkin_heritage;spawn.drakkin_tattoo=pp.drakkin_tattoo;spawn.drakkin_details=pp.drakkin_details;
  for(unsigned i=0;i<EQ::textures::materialCount;++i){auto* item=out->inventory.GetItem(EQ::InventoryProfile::CalcSlotFromMaterial(i));if(!item)continue;const auto* data=item->GetItem();if(!data)return reject("equipment-definition");
   const auto material=NativeMaterial(*item,i);spawn.equipment.Slot[i].Material=material;spawn.equipment.Slot[i].EliteModel=data->EliteMaterial;spawn.equipment_tint.Slot[i].Color=pp.item_tint.Slot[i].UseTint?pp.item_tint.Slot[i].Color:data->Color;pp.item_material.Slot[i].Material=material;
   // Complex hero-forge/texture overrides stay on the complete reload path.
   if(item->GetOrnamentHeroModel() || data->HerosForgeModel || (item->GetOrnamentationAugment() && item->GetOrnamentationAugment()->GetItem()->HerosForgeModel))return reject("equipment-hero-forge");
  }
  for(const auto& b:out->buffs)if(IsValidSpell(b.spellid)){if(IsEffectInSpell(b.spellid,SpellEffect::Levitate))spawn.flymode=2;if(IsEffectInSpell(b.spellid,SpellEffect::Invisibility))spawn.invis=1;}
  auto wire_profile=pp;
  // Compact alternate-profile ownership is RespondAA format in BOTH
  // directions: highest owned rank, cumulative cost, remaining charges.
  for(auto& owned_rank:wire_profile.aa_array)if(owned_rank.AA){
   auto* ability=zone->GetAlternateAdvancementAbilityByRank(owned_rank.AA);
   auto* rank=ability?ability->GetRankByPointsSpent(owned_rank.value):nullptr;if(!rank || rank->total_cost<0)return reject("owned-aa-wire-rank");
   owned_rank.AA=rank->id;owned_rank.value=rank->total_cost;
   if(out->definition)if(auto* presentation=SpiritShroud::CapturedPresentation(*out->definition,rank))owned_rank.value=presentation->total_cost;
  }
  auto profile=RoF2::SerializeShroudNormalProfile(wire_profile);auto items=RoF2::SerializeShroudInventory(wire_items);auto self_spawn=RoF2::SerializeShroudSpawn(spawn);
  if(!profile || !items || !self_spawn)return reject("wire-serialization");
  auto compact=SpiritShroud::Native::CompactFromNormal(*profile);out->self_bytes=SpiritShroud::Native::Envelope(GetID(),*self_spawn,&compact,&*items);
  spawn.NPC=0;auto observer_spawn=RoF2::SerializeShroudSpawn(spawn);if(!observer_spawn)return reject("observer-serialization");
  out->observer_bytes=SpiritShroud::Native::Envelope(GetID(),*observer_spawn);
  auto final=NativeQuery(db,"SELECT CONNECTION_ID(),@@in_transaction");
  if(final.RowCount()!=1 || NativeNumber(final.begin()[0])!=identity || NativeNumber(final.begin()[1])!=1 || failures!=db.QueryFailureGeneration())return reject("transaction-final-identity");
  return out;
 }catch(const std::exception& error){LogError("Native shroud preparation aborted before commit: [{}]",error.what());throw;}
}

bool Client::InstallSpiritShroudDestination(SpiritShroudDestination& next) {
 if(!m_spirit_shroud_transition || next.self_bytes.empty() || next.observer_bytes.empty() || next.buffs.size()!=size_t(GetMaxTotalSlots()) || !buffs)return false;
 // All authoritative state and ownership allocations were prepared before
 // COMMIT. No Save/Load, lease takeover, queue release or SQL occurs here.
 m_pp=next.profile;m_epp=next.extended;
 m_inv.SwapProfileInventory(next.inventory);p_timers.Swap(next.timers);aa_ranks.swap(next.aa_ranks);
 m_spirit_shroud_template=next.template_id;m_spirit_shroud_revision=next.revision;m_spirit_shroud_session_id=next.session_id;
 m_spirit_shroud_personal_bank=next.personal_bank;m_spirit_shroud_definition=next.definition;m_spirit_shroud_aa_ranks.swap(next.allowed_ranks);
 std::copy(next.buffs.begin(),next.buffs.end(),buffs);
 std::memset(GetPetInfo(PetInfoType::Current),0,sizeof(PetInfo));std::memset(GetPetInfo(PetInfoType::Suspended),0,sizeof(PetInfo));
 class_=m_pp.class_;level=m_pp.level;race=base_race=m_pp.race;gender=base_gender=m_pp.gender;deity=m_pp.deity;
 haircolor=m_pp.haircolor;beardcolor=m_pp.beardcolor;eyecolor1=m_pp.eyecolor1;eyecolor2=m_pp.eyecolor2;hairstyle=m_pp.hairstyle;luclinface=m_pp.face;beard=m_pp.beard;
 drakkin_heritage=m_pp.drakkin_heritage;drakkin_tattoo=m_pp.drakkin_tattoo;drakkin_details=m_pp.drakkin_details;
 base_size=size=next.model_size;mob_texture_profile={};use_model=0;
 InitInnates();Numhits(false);
 // Unlike zoning, an in-place exchange does not construct a fresh Mob. Clear
 // spell-granted actor caches, then reproduce CompleteConnect's buff reapply
 // rules against the committed destination instead of retaining the old form.
 RemoveProcFromWeapon(SPELL_UNKNOWN,true);
 RemoveDefensiveProc(SPELL_UNKNOWN,true);
 RemoveRangedProc(SPELL_UNKNOWN,true);
 silenced=false;amnesiad=false;invulnerable=gminvul;
 for(const auto& buff:next.buffs){
  if(!IsValidSpell(buff.spellid))continue;
  for(int effect=0;effect<EFFECT_COUNT;++effect){
   const auto& spell=spells[buff.spellid];
   switch(spell.effect_id[effect]){
    case SpellEffect::Silence:silenced=true;break;
    case SpellEffect::Amnesia:amnesiad=true;break;
    case SpellEffect::DivineAura:invulnerable=true;break;
    case SpellEffect::AddMeleeProc:
    case SpellEffect::WeaponProc:
     if(!AddProcToWeapon(GetProcID(buff.spellid,effect),false,100+spell.limit_value[effect],buff.spellid,buff.casterlevel,GetSpellProcLimitTimer(buff.spellid,ProcType::MELEE_PROC)))return false;
     break;
    case SpellEffect::DefensiveProc:
     if(!AddDefensiveProc(GetProcID(buff.spellid,effect),100+spell.limit_value[effect],buff.spellid,GetSpellProcLimitTimer(buff.spellid,ProcType::DEFENSIVE_PROC)))return false;
     break;
    case SpellEffect::RangedProc:
     if(!AddRangedProc(GetProcID(buff.spellid,effect),100+spell.limit_value[effect],buff.spellid,GetSpellProcLimitTimer(buff.spellid,ProcType::RANGED_PROC)))return false;
     break;
   }
  }
 }
 hidden=false;improved_hidden=false;sneaking=false;
 for(auto& timer:aa_timers)timer.Disable();
 // Avoid CalcBonuses' XTarget Save/packet side effect while fenced. The root
 // sends authoritative ownership/UI state after the transformation packet.
 CalcAABonuses(&aabonuses);MaxXTargets=std::min<unsigned>(XTARGET_HARDCAP,5+aabonuses.extra_xtargets);
 CalcSpellBonuses(&spellbonuses);invisible=std::max(spellbonuses.invisibility,nobuff_invisible);
 CalcBonuses();
 if(GetMaxHP()<1 || GetMaxMana()<0 || GetMaxEndurance()<0)return false;
 current_hp=std::clamp<int64>(m_pp.cur_hp,1,GetMaxHP());current_mana=std::clamp<int64>(m_pp.mana,0,GetMaxMana());current_endurance=std::clamp<int64>(m_pp.endurance,0,GetMaxEndurance());
 UpdateEquipmentLight();UpdateActiveLight();SetAttackTimer();
 return true;
}

namespace {
// Mirrors the existing normal-login innate initialization without mutating
// the active Client while the destination transaction is still uncommitted.
void NativeInnates(PlayerProfile_Struct& profile)
{
	// this function on the client also inits the level one innate skills (like swimming, hide, etc)
	// we won't do that here, lets just do the InnateSkills for now. Basically translation of what the client is doing
	// A lot of these we could probably have ignored because they have no known use or are 100% client side
	// but I figured just in case we'll do them all out
	//
	// The client calls this in a few places. When you remove a vision buff and in SetHeights, which is called in
	// illusions, mounts, and a bunch of other cases. All of the calls to InitInnates are wrapped in restoring regen
	// besides the call initializing the first time
	auto race   = profile.race;
	auto class_ = profile.class_;

	for (int i = 0; i < InnateSkillMax; ++i) {
		profile.InnateSkills[i] = InnateDisabled;
	}

	profile.InnateSkills[InnateInspect] = InnateEnabled;
	profile.InnateSkills[InnateOpen] = InnateEnabled;

	if (race >= Race::Froglok2) {
		if (race == Race::Skeleton2 || race == Race::Froglok2) {
			profile.InnateSkills[InnateUltraVision] = InnateEnabled;
		} else {
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
		}
	}

	switch (race) {
		case Race::Barbarian:
		case Race::HalasCitizen:
			profile.InnateSkills[InnateSlam] = InnateEnabled;
			break;
		case Race::Erudite:
		case Race::EruditeCitizen:
			profile.InnateSkills[InnateLore] = InnateEnabled;
			break;
		case Race::WoodElf:
		case Race::Fayguard:
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			break;
		case Race::Gnome:
		case Race::HighElf:
		case Race::Felguard:
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			profile.InnateSkills[InnateLore]        = InnateEnabled;
			break;
		case Race::Troll:
		case Race::GrobbCitizen:
			profile.InnateSkills[InnateRegen]       = InnateEnabled;
			profile.InnateSkills[InnateSlam]        = InnateEnabled;
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			break;
		case Race::Dwarf:
		case Race::KaladimCitizen:
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			break;
		case Race::Ogre:
		case Race::OggokCitizen:
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			profile.InnateSkills[InnateSlam]        = InnateEnabled;
			profile.InnateSkills[InnateNoBash]      = InnateEnabled;
			profile.InnateSkills[InnateBashDoor]    = InnateEnabled;
			break;
		case Race::Halfling:
		case Race::RivervaleCitizen:
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			break;
		case Race::Iksar:
			profile.InnateSkills[InnateRegen]       = InnateEnabled;
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			break;
		case Race::VahShir:
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			break;
		case Race::DarkElf:
		case Race::NeriakCitizen:
		case Race::ElfVampire:
		case Race::FroglokGhoul:
		case Race::Ghost:
		case Race::Ghoul:
		case Race::Skeleton:
		case Race::Vampire:
		case Race::Wisp:
		case Race::Zombie:
		case Race::Spectre:
		case Race::DwarfGhost:
		case Race::EruditeGhost:
		case Race::DragonSkeleton:
		case Race::Innoruuk:
			profile.InnateSkills[InnateUltraVision] = InnateEnabled;
			break;
		case Race::Human:
		case Race::FreeportGuard:
		case Race::HumanBeggar:
		case Race::HighpassCitizen:
		case Race::QeynosCitizen:
		case Race::Froglok2: // client does froglok weird, but this should work out fine
			break;
		default:
			profile.InnateSkills[InnateInfravision] = InnateEnabled;
			break;
	}

	switch (class_) {
		case Class::Druid:
			profile.InnateSkills[InnateHarmony] = InnateEnabled;
			break;
		case Class::Bard:
			profile.InnateSkills[InnateReveal] = InnateEnabled;
			break;
		case Class::Rogue:
			profile.InnateSkills[InnateSurprise] = InnateEnabled;
			profile.InnateSkills[InnateReveal]   = InnateEnabled;
			break;
		case Class::Ranger:
			profile.InnateSkills[InnateAwareness] = InnateEnabled;
			break;
		case Class::Monk:
			profile.InnateSkills[InnateSurprise]  = InnateEnabled;
			profile.InnateSkills[InnateAwareness] = InnateEnabled;
		default:
			break;
	}
}

}
