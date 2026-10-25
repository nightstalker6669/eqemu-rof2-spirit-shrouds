#include "client.h"
#include "entity.h"
#include "merc.h"
#include "npc.h"
#include "zone.h"
#include "inventory_session_owner.h"
#include "spirit_shroud_destination.h"
#include "housing_item_reconstruction.h"
#include "../common/repositories/inventory_repository.h"
#include "../common/repositories/character_buffs_repository.h"
#include "fellowship_campsite_bonuses.h"
#include "../common/opcodemgr.h"
#include "groups.h"
#include "raids.h"
#include "../common/eqemu_config.h"
#include "../common/rulesys.h"
#include "../common/shroud/catalog.h"
#include "../common/patches/rof2.h"
#include "../common/shroud/aa_preview.h"
#include "../common/shroud/ability_cast.h"
#include "../common/shroud/experience.h"
#include "../common/shroud/starter_inventory.h"
#include "../common/shroud/resource_baseline.h"
#include "../common/housing/coin_pending_guard.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <limits>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>

extern Zone* zone;
namespace {
using SpiritShroud::Bytes;
using SpiritShroud::Template;
using SpiritShroud::Put32;
using SpiritShroud::Put64;
using SpiritShroud::Text;
using SpiritShroud::Token;
using SpiritShroud::Read32;
struct Table {const char* name;const char* key;};
const Table Tables[] = {
 {"character_data","id"},{"inventory","character_id"},{"character_currency","id"},
 {"character_spells","id"},{"character_memmed_spells","id"},{"character_skills","id"},
 {"character_alternate_abilities","id"},{"character_disciplines","id"},{"character_buffs","character_id"},
 {"character_bandolier","id"},{"character_potionbelt","id"},{"character_material","id"},
 {"character_pet_info","char_id"},{"character_pet_buffs","char_id"},{"character_pet_inventory","char_id"},
 {"character_item_recast","id"},{"timers","char_id"}
};
auto Q(Database& db,const std::string& sql) {
 if(db.GetStatus()!=DBcore::Connected)throw std::runtime_error("shroud database disconnected");
 auto r=db.QueryDatabase(sql,false);if(!r.Success())throw std::runtime_error("shroud storage query failed: "+r.ErrorMessage());return r;
}
uint64_t N(const char* text) {
 if(!text||!*text)throw std::runtime_error("shroud null number");
 std::string s(text);size_t end=0;auto v=std::stoull(s,&end);if(end!=s.size()||s[0]=='-')throw std::runtime_error("shroud invalid number");return v;
}
std::string S(uint64_t n){return std::to_string(n);}
std::string PersonalBankDomain() {
 return "(slot_id BETWEEN "+S(EQ::invslot::BANK_BEGIN)+" AND "+S(EQ::invslot::BANK_END)+
  " OR slot_id BETWEEN "+S(EQ::invbag::BANK_BAGS_BEGIN)+" AND "+S(EQ::invbag::BANK_BAGS_END)+")";
}


// Ordinary saves intentionally persist only long buffs. Capture Song slots only
// inside an entering-shroud transaction, so a failed transition cannot change
// ordinary camp/logout persistence. Slot offsets are client constants, not class.
void CaptureSpiritShroudShortBuffs(ZoneDatabase& db, Client& client)
{
 const int first=client.GetMaxBuffSlots(),end=first+client.GetMaxSongSlots();
 if(client.ClientVersion()!=EQ::versions::ClientVersion::RoF2 || first<0 || end<first ||
    end>client.GetMaxTotalSlots() || end>int(UINT8_MAX)+1 || !client.GetBuffs())
  throw std::runtime_error("shroud short buff slot domain");
 std::vector<CharacterBuffsRepository::CharacterBuffs> rows;
 for(int slot=first;slot<end;++slot){
  const auto& b=client.GetBuffs()[slot];
  if(!IsValidSpell(b.spellid) || IsFellowshipCampsiteAuraBuff(b.spellid,b.caster_name,sizeof(b.caster_name)))continue;
  if(!spells[b.spellid].short_buff_box || b.dot_rune>INT32_MAX || b.instrument_mod>INT32_MAX)
   throw std::runtime_error("shroud short buff payload");
  auto e=CharacterBuffsRepository::NewEntity();
  // Runtime spellid/casterlevel already use the repository uint16/uint8 widths.
  e.character_id=client.CharacterID();e.slot_id=uint8_t(slot);e.spell_id=b.spellid;e.caster_level=b.casterlevel;
  e.caster_name=std::string(b.caster_name,strnlen(b.caster_name,sizeof(b.caster_name)));
  if(e.caster_name.size()>=sizeof(b.caster_name))throw std::runtime_error("shroud short buff caster");
  e.ticsremaining=b.ticsremaining;e.counters=b.counters;e.numhits=b.hit_number;
  e.melee_rune=b.melee_rune;e.magic_rune=b.magic_rune;e.persistent=b.persistant_buff;
  e.dot_rune=b.dot_rune;e.caston_x=b.caston_x;e.caston_y=b.caston_y;e.caston_z=b.caston_z;
  e.ExtraDIChance=b.ExtraDIChance;e.instrument_mod=b.instrument_mod;
  rows.push_back(std::move(e));
 }
 // Save(2) ran before this transaction and replaced the ordinary buff rows.
 auto old=Q(db,"SELECT COUNT(*) FROM character_buffs WHERE character_id="+S(client.CharacterID())+" AND slot_id>="+S(first));
 if(old.RowCount()!=1 || N(old.begin()[0]))throw std::runtime_error("shroud short buff checkpoint changed");
 const auto failures=db.QueryFailureGeneration();
 if(!rows.empty() && CharacterBuffsRepository::InsertMany(db,rows)!=int(rows.size()))
  throw std::runtime_error("shroud short buff checkpoint failed");
 if(db.QueryFailureGeneration()!=failures)throw std::runtime_error("shroud short buff checkpoint query failed");
}

uint64_t Now(){return uint64_t(std::time(nullptr));}
std::string Hex(const uint8_t* p,size_t n) {
 const char* digits="0123456789ABCDEF";std::string s;
 for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;
}
void Send(Client& c,EmuOpcode opcode,const Bytes& b) {
 EQApplicationPacket p(opcode,b.size());if(!b.empty())std::copy(b.begin(),b.end(),p.pBuffer);c.QueuePacket(&p);
}
bool Keeper(Client& c,uint32_t expected=0) {
 auto* npc=c.GetTarget();if(!zone||!npc||!npc->IsNPC()||(expected&&expected!=npc->GetID()))return false;
 auto id=npc->GetNPCTypeID();if(id!=202326&&id!=202327&&id!=202329)return false;
 return c.CalculateDistance(npc->GetX(),npc->GetY(),npc->GetZ())<=40.0f;
}
std::vector<Template> Catalog(Database& db, bool include_disabled = false, uint32_t only_id = 0)
{
 auto rows=Q(db,"SELECT id,progression_id,branch_id,level,race,class_id,gender,hp,mana,endurance,base_stat,unlock_xp,name,description,progression_name,branch_name,content_version,verified,test_only,evidence_ref,monster_points,entitlement_key,COALESCE(stat_str,base_stat),COALESCE(stat_sta,base_stat),COALESCE(stat_cha,base_stat),COALESCE(stat_dex,base_stat),COALESCE(stat_int,base_stat),COALESCE(stat_agi,base_stat),COALESCE(stat_wis,base_stat),native_hp,native_mana,native_endurance,native_hp_regen,native_mana_regen_sitting,native_mana_regen_standing,native_endurance_regen,native_cold,native_fire,native_magic,native_disease,native_poison,native_additional_resist,native_corruption,native_ac,native_attack,native_unarmed_damage,native_unarmed_delay,resource_maxima,model_size FROM spirit_shroud_templates" + std::string(only_id ? " WHERE id="+S(only_id) : (include_disabled ? "" : " WHERE enabled=1")) + " ORDER BY progression_id,branch_id,level,id LIMIT 1001");
 if(rows.RowCount()>1000)throw std::runtime_error("shroud catalog too large");
 auto presentation_columns=Q(db,"SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='spirit_shroud_abilities' AND COLUMN_NAME IN ('display_rank','display_max_rank','display_total_cost')");
 const bool has_presentations=N((*presentation_columns.begin())[0])==3;
 std::vector<Template> out;
 for(auto r:rows){
  Template t;uint32_t* values[]={&t.id,&t.progression,&t.branch,&t.level,&t.race,&t.class_id,&t.gender,&t.hp,&t.mana,&t.endurance,&t.stat,&t.unlock_xp};
  for(unsigned i=0;i<12;++i){auto n=N(r[i]);if(n>UINT32_MAX)throw std::runtime_error("shroud catalog overflow");*values[i]=uint32_t(n);}
  t.name=r[12]?r[12]:"";t.description=r[13]?r[13]:"";
  t.progression_name=r[14]?r[14]:"";t.branch_name=r[15]?r[15]:"";
  t.content_version=uint32_t(N(r[16]));t.verified=N(r[17])!=0;t.test_only=N(r[18])!=0;
  t.evidence_ref=r[19]?r[19]:"";t.monster_points=uint32_t(N(r[20]));t.entitlement_key=r[21]?r[21]:"";
  for(unsigned i=0;i<7;++i){auto v=N(r[22+i]);if(v>1000)throw std::runtime_error("shroud attribute overflow");t.attributes[i]=uint32_t(v);}
  t.has_native_stats=true;
  for(unsigned i=0;i<18;++i){if(!r[29+i]){t.has_native_stats=false;continue;}auto v=N(r[29+i]);if(v>1000000)throw std::runtime_error("shroud native baseline overflow");t.native_stats[i]=uint32_t(v);}
  t.has_native_resources=t.has_native_stats;
  t.resource_maxima=N(r[47])!=0;
  t.model_size=r[48]?std::stof(r[48]):0.0f;
  if(t.resource_maxima){
   auto base=zone->GetBaseData(t.level,t.class_id);
   if(base.level!=t.level)throw std::runtime_error("shroud class base data missing");
   auto terms=SpiritShroud::ResourceContributions(t.attributes,t.hp,t.mana,t.endurance,t.class_id,base.hp_fac,base.mana_fac,base.end_fac);
   t.native_stats[SpiritShroud::HP]=terms[0];t.native_stats[SpiritShroud::Mana]=terms[1];t.native_stats[SpiritShroud::Endurance]=terms[2];
   // Unmeasured combat baselines remain the emulator defaults. Do not interpret
   // Live displayed AC/attack as native contributions.
   if(!t.has_native_stats){
    for(auto field:{SpiritShroud::Cold,SpiritShroud::Fire,SpiritShroud::Magic})t.native_stats[field]=25;
    for(auto field:{SpiritShroud::Disease,SpiritShroud::Poison,SpiritShroud::AdditionalResist,SpiritShroud::Corruption})t.native_stats[field]=15;
    t.native_stats[SpiritShroud::HPRegen]=static_cast<uint32_t>(base.hp_regen);
    t.native_stats[SpiritShroud::EnduranceRegen]=static_cast<uint32_t>(base.end_regen);
   }
   t.has_native_resources=true;
  }
  auto aa=Q(db,"SELECT a.rank_id,r.title_sid,a.free_grant"+std::string(has_presentations?",a.display_rank,a.display_max_rank,a.display_total_cost":"")+" FROM spirit_shroud_abilities a LEFT JOIN aa_ranks r ON r.id=a.rank_id WHERE a.template_id="+S(t.id)+" ORDER BY a.rank_id LIMIT 301");
  if(aa.RowCount()>300)throw std::runtime_error("shroud AA count too large");
  for(auto row:aa){auto rank=N(row[0]),title=N(row[1]);if(rank>INT32_MAX||title>INT32_MAX)throw std::runtime_error("shroud AA overflow");t.abilities.push_back(uint32_t(rank));t.ability_names.push_back(uint32_t(title));if(N(row[2]))t.free_abilities.push_back(uint32_t(rank));
   if(has_presentations && N(row[3])){
    auto current=N(row[3]),maximum=N(row[4]);auto cost=N(row[5]);
    if(current>1000 || maximum>1000 || cost>1000000)throw std::runtime_error("shroud rank presentation overflow");
    t.rank_presentations.push_back({uint32_t(rank),uint32_t(current),uint32_t(maximum),uint32_t(cost)});
   }
  }
  auto equipment=Q(db,"SELECT DISTINCT e.item_id,i.name FROM spirit_shroud_starter_equipment e LEFT JOIN items i ON i.id=e.item_id WHERE e.template_id="+S(t.id)+" ORDER BY e.item_id LIMIT 51");
  for(auto item:equipment){auto id=N(item[0]);if(id>UINT32_MAX || !item[1])throw std::runtime_error("shroud equipment definition missing");t.equipment.push_back({uint32_t(id),item[1]});}
  SpiritShroud::Validate(t);
  for(const auto& item:t.rank_presentations)if(!SpiritShroud::CapturedPresentation(t,zone->GetAlternateAdvancementRank(item.rank_id)))throw std::runtime_error("invalid compressed shroud rank adapter");
  out.push_back(std::move(t));
 }
 return out;
}
bool Offered(Database& db, Client& c, const Template& t)
{
 if(!SpiritShroud::Offered(t, RuleB(Character,AllowTestSpiritShrouds)))return false;
 // Retained templates remain restorable; only new entry uses progression gates.
 if (!t.test_only) {
  auto branch=Q(db,"SELECT b.unlock_xp FROM spirit_shroud_branch_progression b JOIN spirit_shroud_progression_definitions d ON d.progression_id=b.progression_id AND d.definition_version=b.definition_version WHERE b.progression_id="+S(t.progression)+" AND b.branch_id="+S(t.branch));
  if(branch.RowCount()!=1 || !branch.begin()[0])return false;
  auto progress=Q(db,"SELECT experience FROM spirit_shroud_progress WHERE character_id="+S(c.CharacterID())+" AND progression_id="+S(t.progression));
  const auto xp=progress.RowCount()?N(progress.begin()[0]):0;
  if(xp<N(branch.begin()[0]) || xp<t.unlock_xp)return false;
 }
 if(t.entitlement_key.empty())return true;
 auto owned=Q(db,"SELECT character_id FROM spirit_shroud_entitlements WHERE character_id="+S(c.CharacterID())+" AND entitlement_key='"+db.Escape(t.entitlement_key)+"'");
 return owned.RowCount()==1;
}
uint64_t Progress(Database& db,uint32_t character,uint32_t progression) {
 auto r=Q(db,"SELECT experience FROM spirit_shroud_progress WHERE character_id="+S(character)+" AND progression_id="+S(progression));return r.RowCount()?N(r.begin()[0]):0;
}
constexpr const char* ExperienceColumns="normal_exp,aa_exp,aa_points,aa_spent,level,level2,skill_points,percent_to_aa,aa_unit,max_level,thresholds_json";
template<typename Row> SpiritShroud::ExperienceState ReadExperience(Row row)
{
 SpiritShroud::ExperienceState state;
 state.normal=N(row[0]);state.aa=N(row[1]);
 uint32_t* fields[]={&state.aa_points,&state.aa_spent,&state.level,&state.level2,&state.skill_points,
  &state.percent_to_aa,&state.aa_unit,&state.max_level};
 for(unsigned i=0;i<std::size(fields);++i){auto value=N(row[i+2]);if(value>UINT32_MAX)throw std::runtime_error("shroud experience overflow");*fields[i]=uint32_t(value);}
 if(!row[10] || std::char_traits<char>::length(row[10])>4096)throw std::runtime_error("shroud thresholds missing");
 auto thresholds=nlohmann::json::parse(row[10]);
 if(!thresholds.is_array() || thresholds.size()>128)throw std::runtime_error("shroud thresholds malformed");
 for(const auto& value:thresholds){if(!value.is_number_unsigned() || value.template get<uint64_t>()>UINT32_MAX)throw std::runtime_error("shroud threshold overflow");state.thresholds.push_back(value.template get<uint32_t>());}
 SpiritShroud::ValidateExperience(state);return state;
}
std::string ExperienceValues(Database& db,const SpiritShroud::ExperienceState& state)
{
 return S(state.normal)+","+S(state.aa)+","+S(state.aa_points)+","+S(state.aa_spent)+","+S(state.level)+","+
  S(state.level2)+","+S(state.skill_points)+","+S(state.percent_to_aa)+","+S(state.aa_unit)+","+
  S(state.max_level)+",'"+db.Escape(nlohmann::json(state.thresholds).dump())+"'";
}
void Schema(Database& db) {
 std::set<std::string> names={"spirit_shroud_state","spirit_shroud_progress","spirit_shroud_templates","spirit_shroud_spells","spirit_shroud_bank_inventory","spirit_shroud_abilities","spirit_shroud_sessions","spirit_shroud_snapshot_manifest","spirit_shroud_entitlements","spirit_shroud_allocations","spirit_shroud_experience_state","spirit_shroud_experience_events","spirit_shroud_starter_equipment","spirit_shroud_starter_provenance","spirit_shroud_mercenary_fees"};
 for(auto t:Tables){names.insert(t.name);names.insert("shroud_original_"+std::string(t.name));}
 std::string list;for(auto& n:names){if(!list.empty())list+=",";list+="'"+n+"'";}
 auto r=Q(db,"SELECT table_name,engine FROM information_schema.tables WHERE table_schema=DATABASE() AND table_name IN ("+list+")");
 if(r.RowCount()!=names.size())throw std::runtime_error("shroud migration missing");
 for(auto row:r)if(!row[1]||std::string(row[1])!="InnoDB")throw std::runtime_error("shroud needs transactional tables");
}
void ValidateSnapshot(Database& db, uint32_t character, uint64_t session)
{
 const auto actor=S(character);
 auto original=Q(db,"SELECT account_id FROM shroud_original_character_data WHERE id="+actor);
 auto owner=Q(db,"SELECT account_id FROM character_data WHERE id="+actor);
 auto wallet=Q(db,"SELECT id FROM shroud_original_character_currency WHERE id="+actor);
 if(original.RowCount()!=1 || owner.RowCount()!=1 || wallet.RowCount()!=1 ||
    N(original.begin()[0])!=N(owner.begin()[0]))throw std::runtime_error("shroud recovery roots missing");
 if(!session)return; // Pre-versioned sessions retain the legacy recovery path.
 auto session_version=Q(db,"SELECT experience_version FROM spirit_shroud_sessions WHERE id="+S(session)+" AND character_id="+actor);
 if(session_version.RowCount()!=1 || N(session_version.begin()[0])>1)throw std::runtime_error("shroud experience version unavailable");
 if(N(session_version.begin()[0])==1){
  auto experience=Q(db,"SELECT session_id FROM spirit_shroud_experience_state WHERE session_id="+S(session)+" AND settled_at IS NULL");
  if(experience.RowCount()!=1)throw std::runtime_error("shroud pending experience missing");
 }
 auto manifest=Q(db,"SELECT table_name,row_count FROM spirit_shroud_snapshot_manifest WHERE session_id="+S(session));
 if(manifest.RowCount()!=std::size(Tables))throw std::runtime_error("shroud snapshot manifest missing");
 std::map<std::string,uint64_t> counts;
 for(auto row:manifest){if(!row[0])throw std::runtime_error("shroud snapshot table missing");counts.emplace(row[0],N(row[1]));}
 for(auto table:Tables){auto found=counts.find(table.name);if(found==counts.end())throw std::runtime_error("shroud snapshot table missing");
  auto rows=Q(db,"SELECT COUNT(*) FROM shroud_original_"+std::string(table.name)+" WHERE "+table.key+"="+actor);
  if(N(rows.begin()[0])!=found->second)throw std::runtime_error("shroud snapshot cardinality changed");
 }
}
void LockActor(Database& db,Client& c) {
 auto actor=S(c.CharacterID());
 auto r=Q(db,"SELECT id FROM character_data WHERE id="+actor+" AND account_id="+S(c.AccountID())+" AND deleted_at IS NULL FOR UPDATE");
 if(r.RowCount()!=1)throw std::runtime_error("shroud actor missing");
 if(RuleB(Housing,EnablePlayerHousing) || RuleB(Mercs,AllowMercs) || c.InventorySessionReleaseTicket()) {
  auto ticket=c.InventorySessionReleaseTicket();auto lease=ticket?ticket->SnapshotFor(c):std::nullopt;
  if(!lease||!c.HasInventorySessionReservation())throw std::runtime_error("shroud writer session missing");
  auto rows=Q(db,"SELECT epoch,HEX(token),HEX(process_key),HEX(client_key),active FROM housing_inventory_sessions WHERE character_id="+actor+" AND account_id="+S(c.AccountID())+" FOR UPDATE");
  if(rows.RowCount()!=1)throw std::runtime_error("shroud writer session missing");auto v=rows.begin();
  if(N(v[0])!=lease->epoch||!v[1]||std::string(v[1])!=Hex(lease->token.data(),lease->token.size())||!v[2]||std::string(v[2])!=Hex(reinterpret_cast<const uint8_t*>(lease->owner.process_key.data()),lease->owner.process_key.size())||!v[3]||std::string(v[3])!=Hex(reinterpret_cast<const uint8_t*>(lease->owner.client_key.data()),lease->owner.client_key.size())||N(v[4])!=1)throw std::runtime_error("shroud stale writer session");
  auto custody=Q(db,"SELECT account_id,pending_key FROM housing_live_item_leases WHERE character_id="+actor+" FOR UPDATE");
  if(custody.RowCount()&&(N(custody.begin()[0])!=c.AccountID()||custody.begin()[1]))throw std::runtime_error("shroud item operation pending");
  if(Housing::CoinPending::Inspect(db,c.CharacterID(),true)!=Housing::CoinPending::State::Clear)throw std::runtime_error("shroud coin operation pending");
 }
}
}

void Client::BeginSpiritShroudNativeAcknowledgement(uint64_t revision)
{
 // Called only after the committed destination has replaced every server cache.
 // The native acknowledgement has no revision token; serialize exchanges on this
 // connection and never let it commit or reinstall an old profile.
 if (!m_spirit_shroud_transition || revision != m_spirit_shroud_revision ||
     m_spirit_shroud_native_pending) throw std::runtime_error("shroud native cache revision");
 m_spirit_shroud_native_revision = revision;
 m_spirit_shroud_native_deadline = Now() + 30;
 m_spirit_shroud_native_pending = true;
}

bool Client::ProcessSpiritShroudNativeTransition()
{
 if (!m_spirit_shroud_native_pending) return true;
 if (m_spirit_shroud_native_revision != m_spirit_shroud_revision ||
     Now() > m_spirit_shroud_native_deadline) {
  m_spirit_shroud_native_pending = false;
  m_spirit_shroud_transition = true;
  LogError("Spirit Shroud native completion timeout for character [{}], revision [{}]", CharacterID(), m_spirit_shroud_native_revision);
  Disconnect();
  return false;
 }
 return true;
}

void Client::Handle_OP_SpiritShroudComplete(const EQApplicationPacket* packet)
{
 if (!packet || packet->size || ClientVersion() != EQ::versions::ClientVersion::RoF2 ||
     !m_spirit_shroud_native_pending || !m_spirit_shroud_transition) return;
 if (!ProcessSpiritShroudNativeTransition()) return;
 m_spirit_shroud_native_pending = false;
 m_spirit_shroud_native_deadline = 0;
 m_spirit_shroud_transition = false;
 // Publish the committed active class/level only after its native completion.
 UpdateWho();
 m_spirit_shroud_resource_refreshes = IsSpiritShrouded() ? 3 : 0;
 LogInfo("Spirit Shroud native completion for character [{}], revision [{}], template [{}]", CharacterID(), m_spirit_shroud_revision, m_spirit_shroud_template);
 SendHPUpdate(true); SendManaUpdate(); SendEnduranceUpdate();
 SendSpiritShroudProgress();
}

bool Client::SaveSpiritShroudCheckpoint()
{
 // Save() has void persistence helpers. Observe every underlying query, including
 // transient failures hidden by ordinary reconnect/retry, before taking escrow.
 // Do not wrap Save() in a transaction: task dirty flags advance on successful
 // writes and would be lost if a later profile write rolled that transaction back.
 if (m_spirit_shroud_transition || m_inventory_session_rejected ||
     m_mercenary_persistence_failed || m_fellowship_xp_persistence_failed ||
     m_fellowship_xp_transaction_active || !ClientDataLoaded()) return false;
 const auto failures = database.QueryFailureGeneration();
 try {
  auto before = Q(database, "SELECT CONNECTION_ID(),@@in_transaction,@@autocommit");
  if (before.RowCount() != 1 || (N(before.begin()[1]) != 0 || N(before.begin()[2]) != 1)) return false;
  const auto connection = N(before.begin()[0]);
  if (!SaveAA() || !p_timers.Store(&database) || !Save(2)) return false;
  auto after = Q(database, "SELECT CONNECTION_ID(),@@in_transaction,@@autocommit");
  return after.RowCount() == 1 && N(after.begin()[0]) == connection &&
   N(after.begin()[1]) == 0 && N(after.begin()[2]) == 1 && database.QueryFailureGeneration() == failures;
 } catch (const std::exception&) {
  return false;
 }
}

bool Client::LoadSpiritShroudState()
{
 m_spirit_shroud_restored_short_buffs_pending=false;
 m_spirit_shroud_template=0;m_pp.unknown0112=0;m_spirit_shroud_personal_bank=false;
 m_pp.shroud_native_stats_valid=false;
 m_pp.shroud_native_resources_valid=false;
 std::fill(std::begin(m_pp.shroud_native_stats),std::end(m_pp.shroud_native_stats),0);
 m_spirit_shroud_session_id=0;m_spirit_shroud_revision=0;
 m_spirit_shroud_definition.reset();m_spirit_shroud_aa_ranks.clear();
 auto r=database.QueryDatabase("SELECT template_id,revision,active_session_id FROM spirit_shroud_state WHERE character_id="+S(CharacterID()),false);
 if(!r.Success()){
  if(r.ErrorNumber()==1146&&!RuleB(Character,EnableSpiritShrouds))return true;
  m_spirit_shroud_transition=true;Disconnect();return false;
 }
 try {
  if(r.RowCount()){
   auto row=r.begin();auto id=N(row[0]);if(id>INT32_MAX)throw std::runtime_error("shroud invalid active identity");
   m_spirit_shroud_template=uint32_t(id);m_spirit_shroud_revision=N(row[1]);
   if(row[2])m_spirit_shroud_session_id=N(row[2]);
  }
  if(m_spirit_shroud_template){
   ValidateSnapshot(database,CharacterID(),m_spirit_shroud_session_id);
   Template definition;
   if(m_spirit_shroud_session_id){
    auto session=Q(database,"SELECT template_json,template_id,content_version,entry_revision,inventory_policy FROM spirit_shroud_sessions WHERE id="+S(m_spirit_shroud_session_id)+" AND character_id="+S(CharacterID())+" AND phase='active'");
    if(session.RowCount()!=1)throw std::runtime_error("shroud active session missing");auto row=session.begin();
    if(!row[0]||N(row[1])!=m_spirit_shroud_template||N(row[3])!=m_spirit_shroud_revision)throw std::runtime_error("shroud session identity changed");
    if(N(row[4])>1)throw std::runtime_error("shroud inventory policy unavailable");
    m_spirit_shroud_personal_bank=N(row[4])==1;
    definition=SpiritShroud::RestoreSnapshot(row[0]);
    if(definition.id!=m_spirit_shroud_template||definition.content_version!=N(row[2]))throw std::runtime_error("shroud retained version changed");
    auto allocated=Q(database,"SELECT COALESCE(SUM(paid_points),0) FROM spirit_shroud_allocations WHERE session_id="+S(m_spirit_shroud_session_id));
    if(N(allocated.begin()[0])!=m_pp.aapoints_spent ||
       uint64_t(m_pp.aapoints)+m_pp.aapoints_spent!=definition.monster_points)
     throw std::runtime_error("shroud point accounting changed");
   }else{
    auto legacy=Catalog(database,true,m_spirit_shroud_template);
    if(legacy.size()!=1)throw std::runtime_error("shroud legacy definition missing");definition=legacy.front();
   }
   m_spirit_shroud_definition=std::make_shared<const Template>(std::move(definition));
   for(auto id:m_spirit_shroud_definition->abilities)m_spirit_shroud_aa_ranks.insert(id);
   m_pp.unknown0112=m_spirit_shroud_template;
   m_pp.shroud_native_stats_valid=m_spirit_shroud_definition->has_native_stats;
   m_pp.shroud_native_resources_valid=m_spirit_shroud_definition->has_native_resources || m_spirit_shroud_definition->has_native_stats;
   std::copy(m_spirit_shroud_definition->native_stats.begin(),m_spirit_shroud_definition->native_stats.end(),m_pp.shroud_native_stats);
  }else if(m_spirit_shroud_session_id){throw std::runtime_error("shroud inactive session mismatch");}
  return true;
 }catch(const std::exception& error){
  LogError("Spirit Shroud recovery refused for character [{}]: [{}]",CharacterID(),error.what());
  m_spirit_shroud_transition=true;Disconnect();return false;
 }
}

// Connect calls this after ordinary LoadBuffs and before the player profile and
// CalcBonuses are built. Only the exact latest completed restoration authorizes
// reading Song rows. A later ordinary Save removes those rows as usual; normal
// characters do not acquire general short-buff logout persistence.
bool Client::LoadSpiritShroudRestoredShortBuffs()
{
 m_spirit_shroud_restored_short_buffs_pending=false;
 if(ClientVersion()!=EQ::versions::ClientVersion::RoF2 || m_spirit_shroud_template ||
    m_spirit_shroud_session_id || !m_spirit_shroud_revision)return true;
 const auto failures=database.QueryFailureGeneration();
 try{
  auto latest=Q(database,"SELECT entry_revision,phase FROM spirit_shroud_sessions WHERE character_id="+S(CharacterID())+" ORDER BY entry_revision DESC LIMIT 1");
  if(!latest.RowCount())return true;
  if(latest.RowCount()!=1 || !latest.begin()[1])throw std::runtime_error("shroud short buff restoration identity");
  const auto entry=N(latest.begin()[0]);
  if(entry==UINT64_MAX || entry+1!=m_spirit_shroud_revision || std::string(latest.begin()[1])!="restored")return true;
  const int first=GetMaxBuffSlots(),end=first+GetMaxSongSlots();
  if(first<0 || end<first || end>GetMaxTotalSlots() || end>int(UINT8_MAX)+1 || !buffs)throw std::runtime_error("shroud short buff restoration slots");
  auto rows=CharacterBuffsRepository::GetWhere(database,"character_id="+S(CharacterID())+" AND slot_id>="+S(first)+" AND slot_id<"+S(end));
  if(database.QueryFailureGeneration()!=failures)throw std::runtime_error("shroud short buff restoration query failed");
  if(rows.empty())return true;
  // Match LockActor's feature/retained-ticket fence. Both features disabled
  // continue using ordinary authenticated character admission, not a new
  // housing/mercenary prerequisite merely because a past session exists.
  if((RuleB(Housing,EnablePlayerHousing) || RuleB(Mercs,AllowMercs) || InventorySessionReleaseTicket()) &&
     !HasInventorySessionReservation())throw std::runtime_error("shroud short buff restoration ownership");
  std::set<unsigned> slots;
  for(const auto& e:rows){
   if(e.character_id!=CharacterID() || e.slot_id<unsigned(first) || e.slot_id>=unsigned(end) ||
      !slots.insert(e.slot_id).second || !IsValidSpell(e.spell_id) || !spells[e.spell_id].short_buff_box ||
      e.caster_name.size()>=sizeof(buffs[0].caster_name) || e.persistent>1 || e.dot_rune<0 || e.instrument_mod<0)
    throw std::runtime_error("shroud short buff restoration payload");
  }
  for(const auto& e:rows){
   if(IsFellowshipCampsiteAuraBuff(e.spell_id,e.caster_name.data(),e.caster_name.size()))continue;
   // Keep the ordinary loader's actor-effect exclusions on reload fallback.
   if(IsEffectInSpell(e.spell_id,SpellEffect::Charm) ||
      (IsEffectInSpell(e.spell_id,SpellEffect::Illusion) && !e.persistent))continue;
   auto& b=buffs[e.slot_id];b={};b.spellid=e.spell_id;b.casterlevel=e.caster_level;
   if(auto* caster=entity_list.GetClientByName(e.caster_name.c_str())){
    b.casterid=caster->GetID();b.client=true;strn0cpy(b.caster_name,caster->GetName(),sizeof(b.caster_name));
   }
   b.ticsremaining=e.ticsremaining;b.counters=e.counters;b.hit_number=e.numhits;
   b.melee_rune=e.melee_rune;b.magic_rune=e.magic_rune;b.persistant_buff=e.persistent!=0;
   b.dot_rune=e.dot_rune;b.caston_x=e.caston_x;b.caston_y=e.caston_y;b.caston_z=e.caston_z;
   b.ExtraDIChance=e.ExtraDIChance;b.instrument_mod=e.instrument_mod;
   m_spirit_shroud_restored_short_buffs_pending=true;
  }
  return true;
 }catch(const std::exception& e){
  m_spirit_shroud_restored_short_buffs_pending=false;
  LogError("Spirit Shroud short buff recovery refused for character [{}]: [{}]",CharacterID(),e.what());
  m_spirit_shroud_transition=true;Disconnect();return false;
 }
}

bool Client::TrySpiritShroudChat(const char* message) {
 if(ClientVersion()!=EQ::versions::ClientVersion::RoF2||!Keeper(*this))return false;
 auto text=Strings::ToLower(message?message:"");
 if(text.find("remove")!=std::string::npos){ChangeSpiritShroud(0);return true;}
 if(text.find("bank")!=std::string::npos){OpenSpiritShroudBank();return true;}
 if(!RuleB(Character,EnableSpiritShrouds))return false;
 if(text.find("hail")!=std::string::npos||text.find("shroud")!=std::string::npos){OpenSpiritShrouds();return true;}
 return false;
}

void Client::OpenSpiritShrouds() {
 if(!Keeper(*this)||m_spirit_shroud_transition||IsDead()||IsEngaged())return;
 try {
  if(IsSpiritShrouded()){Message(Chat::Yellow,"Ask me to remove your shroud, or to open your shroud bank.");return;}
  auto bank=Q(database,"SELECT slot_id FROM spirit_shroud_bank_inventory WHERE character_id="+S(CharacterID())+" LIMIT 1");
  if(bank.RowCount()){Message(Chat::Yellow,"Empty your shroud bank before choosing another form. Ask me for your bank.");return;}
  auto all=Catalog(database);if(all.empty()){Message(Chat::Yellow,"No Spirit Shroud forms have been configured.");return;}
  std::string tree;uint32_t last_p=0,last_b=0;
  for(auto& t:all){if(t.level>GetLevel()||!Offered(database,*this,t))continue;auto xp=Progress(database,CharacterID(),t.progression);
   if(last_p!=t.progression){tree+=Token("PROGRESSION",'\a')+Token(S(t.progression),'\a')+Token(t.progression_name,'\a')+Token("Spirit Shroud progression",'\a');last_p=t.progression;last_b=0;}
   if(last_b!=t.branch){tree+=Token("BRANCH",'\a')+Token(S(t.branch),'\a')+Token(t.branch_name,'\a')+Token(t.description,'\a')+Token("100",'\a');last_b=t.branch;}
   tree+=Token("TEMPLATE",'\a')+Token(t.name,'\a')+Token(S(t.level),'\a')+Token(S(t.id),'\a')+Token(xp>=t.unlock_xp?"1":"0",'\a');
  }
  if(tree.empty()){Message(Chat::Yellow,"No verified Spirit Shroud forms are available.");return;}
  m_spirit_shroud_keeper=GetTarget()->GetID();m_spirit_shroud_menu_until=Now()+120;
  Bytes out;Put32(out,m_spirit_shroud_keeper);Put32(out,0);Put32(out,0);Text(out,tree);Send(*this,OP_SpiritShroudSelection,out);
 } catch(const std::exception&){Message(Chat::Red,"Spirit Shroud storage is unavailable. Your character has not changed.");}
}

void Client::Handle_OP_SpiritShroudRequest(const EQApplicationPacket* p) {
 if(p->size!=4||!Keeper(*this,m_spirit_shroud_keeper)||Now()>m_spirit_shroud_menu_until)return;
 try{auto id=Read32(p->pBuffer);for(auto& t:Catalog(database))if(t.id==id&&Offered(database,*this,t)&&t.level<=GetLevel()){auto preview=SpiritShroud::SelectionAbilities(t,[](uint32_t rank){return zone->GetAlternateAdvancementRank(rank);});Send(*this,OP_SpiritShroudDetails,SpiritShroud::Details(t,&preview));return;}}catch(...){}
}
void Client::Handle_OP_SpiritShroudSelect(const EQApplicationPacket* p) {
 if(p->size!=4)return;
 if(Now()<=m_spirit_shroud_menu_until && Keeper(*this,m_spirit_shroud_keeper))ChangeSpiritShroud(Read32(p->pBuffer));
 if(!m_spirit_shroud_transition && ClientVersion()==EQ::versions::ClientVersion::RoF2){
  // RoF2 enters a modal item-operation wait before sending 0x1cd0. Its
  // empty 0x7786 completion clears that wait on a rejected/rolled-back choice.
  EQApplicationPacket finished(OP_Unknown,0);finished.SetOpcodeBypass(0x7786);QueuePacket(&finished);
 }
}
void Client::Handle_OP_SpiritShroudCancel(const EQApplicationPacket* p) {
 if(p->size==0){m_spirit_shroud_keeper=0;m_spirit_shroud_menu_until=0;}
}
void Client::Handle_OP_SpiritShroudPreview(const EQApplicationPacket* p) {
 // Preview is bound to the offered template and its advertised ranks.
 if(p->size!=8||Now()>m_spirit_shroud_menu_until||!Keeper(*this,m_spirit_shroud_keeper))return;
 if(p->GetOpcode()==OP_SpiritShroudEquipmentRequest){
  const auto template_id=Read32(p->pBuffer),item_id=Read32(p->pBuffer+4);
  try{for(const auto& t:Catalog(database))if(t.id==template_id && Offered(database,*this,t) && t.level<=GetLevel()){
   auto allowed=std::find_if(t.equipment.begin(),t.equipment.end(),[&](const auto& item){return item.item_id==item_id;});
   if(allowed==t.equipment.end())return;
   std::unique_ptr<EQ::ItemInstance> item(database.CreateItem(item_id,1));if(!item)return;
   auto serialized=RoF2::SerializeShroudPreviewItem(*item);if(!serialized)return;
   Bytes body;Put32(body,template_id);Put32(body,item_id);body.insert(body.end(),serialized->begin(),serialized->end());
   Send(*this,OP_SpiritShroudEquipment,body);return;
  }}catch(const std::exception& error){LogError("Spirit Shroud equipment preview rejected for character [{}]: [{}]",CharacterID(),error.what());}
  return;
 }
 if(p->GetOpcode()!=OP_SpiritShroudAbilityRequest)return;
 int32_t requested=int32_t(Read32(p->pBuffer));if(requested==INT32_MIN)return;
 auto template_id=uint32_t(requested<0?-requested:requested),rank_id=Read32(p->pBuffer+4);
 try{for(auto& t:Catalog(database))if(t.id==template_id && Offered(database,*this,t) && t.level<=GetLevel() && std::find(t.abilities.begin(),t.abilities.end(),rank_id)!=t.abilities.end()){
  auto* rank=zone->GetAlternateAdvancementRank(rank_id);if(!rank||!rank->base_ability)return;
  SendAlternateAdvancementRank(rank->base_ability->id,rank->current_value,&t);
  Bytes ready;Put32(ready,template_id);Put32(ready,rank_id);Send(*this,OP_SpiritShroudAbilityReady,ready);return;}}
 catch(...){}
}

void Client::ChangeSpiritShroud(uint32_t id) {
 if((id&&!RuleB(Character,EnableSpiritShrouds))||ClientVersion()!=EQ::versions::ClientVersion::RoF2||!Keeper(*this)||m_spirit_shroud_transition)return;
 if((id&&IsSpiritShrouded())||(!id&&!IsSpiritShrouded()))return;
 if(IsDead()||GetHP()<=0||IsCasting()||HasActiveSong()||IsMezzed()||IsStunned()||IsEngaged()||GetAggroCount()||
    (GetPet() && (!GetPet()->IsNPC() || GetPet()->IsCharmed() || GetPet()->IsEngaged()))||IsTrader()||IsBuyer()||IsLooting()||GetTradeskillObject()||(trade && trade->state!=TradeNone)||!GetInv().CursorEmpty()||m_pp.copper_cursor||m_pp.silver_cursor||m_pp.gold_cursor||m_pp.platinum_cursor){
  Message(Chat::Yellow,"Leave combat, stop casting or singing, release any charmed pet, close trading, and empty your item and coin cursor before changing forms.");return;
 }
 // Suspend through the existing durable lifecycle before taking the snapshot.
 // Failure leaves the current form intact; success never dismisses the contract.
 if(auto* merc=GetMerc()){if(!merc->Suspend()){Message(Chat::Red,"Your mercenary could not be suspended; your form has not changed.");return;}}
 if(!SaveSpiritShroudCheckpoint()){Message(Chat::Red,"Your character could not be saved; your form has not changed.");return;}
 ZoneDatabase db;auto* cfg=EQEmuConfig::get();bool started=false,committing=false;
 uint32_t raid_profile_id=0;uint8_t raid_profile_class=0,raid_profile_level=0;
 try {
  if(!cfg||!db.Connect(cfg->DatabaseHost,cfg->DatabaseUsername,cfg->DatabasePassword,cfg->DatabaseDB,cfg->DatabasePort,"spirit-shroud"))throw std::runtime_error("shroud connection");
  Schema(db);Q(db,"START TRANSACTION");started=true;
  auto connection=N(Q(db,"SELECT CONNECTION_ID()").begin()[0]);LockActor(db,*this);auto actor=S(CharacterID());
  auto cursor_coins=Q(db,"SELECT copper_cursor,silver_cursor,gold_cursor,platinum_cursor FROM character_currency WHERE id="+actor+" FOR UPDATE");
  if(cursor_coins.RowCount()!=1)throw std::runtime_error("shroud currency root missing");
  for(unsigned i=0;i<4;++i)if(N(cursor_coins.begin()[i]))throw std::runtime_error("empty the coin cursor before changing forms");
  Q(db,"INSERT INTO spirit_shroud_state(character_id) VALUES ("+actor+") ON DUPLICATE KEY UPDATE character_id=character_id");
  auto state=Q(db,"SELECT template_id,revision,active_session_id FROM spirit_shroud_state WHERE character_id="+actor+" FOR UPDATE");
  if(state.RowCount()!=1||N(state.begin()[0])!=m_spirit_shroud_template||N(state.begin()[1])!=m_spirit_shroud_revision)throw std::runtime_error("shroud state changed");
  // Active contracts must have been durably suspended before the profile swap.
  auto mercs=Q(db,"SELECT MercID FROM mercs WHERE OwnerCharacterID="+actor+" AND IsSuspended=0 FOR UPDATE");
  if(mercs.RowCount())throw std::runtime_error("suspend every active mercenary before shrouding");
  if(id) {
   auto all=Catalog(db);auto found=std::find_if(all.begin(),all.end(),[&](const Template& t){return t.id==id;});
   if(found==all.end()||!Offered(db,*this,*found)||found->level>GetLevel()||Progress(db,CharacterID(),found->progression)<found->unlock_xp)throw std::runtime_error("shroud template not available");auto t=*found;
   auto bank=Q(db,"SELECT slot_id FROM spirit_shroud_bank_inventory WHERE character_id="+actor+" FOR UPDATE");if(bank.RowCount())throw std::runtime_error("empty the shroud bank first");
   Q(db,"INSERT INTO spirit_shroud_sessions(character_id,entry_revision,template_id,content_version,template_json,entered_at,experience_version,inventory_policy) VALUES ("+actor+","+S(m_spirit_shroud_revision+1)+","+S(t.id)+","+S(t.content_version)+",'"+db.Escape(SpiritShroud::Snapshot(t))+"',"+S(Now())+",1,1)");
   auto session_id=N(Q(db,"SELECT LAST_INSERT_ID()").begin()[0]);
   CaptureSpiritShroudShortBuffs(db,*this);
   for(auto table:Tables){std::string name=table.name,key=table.key;auto where=" WHERE "+key+"="+actor;
    if(name=="inventory")where+=" AND NOT "+PersonalBankDomain();
    auto old=Q(db,"SELECT "+key+" FROM shroud_original_"+name+" WHERE "+key+"="+actor+" FOR UPDATE");if(old.RowCount())throw std::runtime_error("shroud recovery snapshot already exists");
    Q(db,"INSERT INTO shroud_original_"+name+" SELECT * FROM "+name+where);
    Q(db,"INSERT INTO spirit_shroud_snapshot_manifest(session_id,table_name,row_count) SELECT "+S(session_id)+",'"+name+"',COUNT(*) FROM shroud_original_"+name+where);
    if(name!="character_data"&&name!="character_currency")Q(db,"DELETE FROM "+name+where);
   }
   SpiritShroud::ExperienceState experience;
   experience.normal=GetEXP();experience.aa=GetAAXP();experience.aa_points=GetAAPoints();experience.aa_spent=GetSpentAA();
   experience.level=GetLevel();experience.level2=m_pp.level2;experience.skill_points=m_pp.points;
   experience.percent_to_aa=GetLevel()>50?std::min<uint32_t>(100,GetAAPercent()):0;
   experience.aa_unit=GetRequiredAAExperience();
   auto maximum=RuleI(Character,MaxExpLevel);if(maximum<1)maximum=RuleI(Character,MaxLevel);
   if(GetClientMaxLevel())maximum=GetClientMaxLevel();
   // Never delevel an original character which already exceeds a changed cap.
   experience.max_level=std::max<int>(GetLevel(),maximum);
   if(!experience.max_level || experience.max_level>126)throw std::runtime_error("shroud original level cap invalid");
   experience.thresholds.push_back(0);
   for(uint32_t level=1;level<=experience.max_level+1;++level)experience.thresholds.push_back(GetEXPForLevel(level));
   SpiritShroud::ValidateExperience(experience);
   Q(db,"INSERT INTO spirit_shroud_experience_state(session_id,"+std::string(ExperienceColumns)+") VALUES ("+S(session_id)+","+ExperienceValues(db,experience)+")");
   Q(db,"UPDATE character_data SET race="+S(t.race)+",class="+S(t.class_id)+",gender="+S(t.gender)+",level="+S(t.level)+",level2="+S(t.level)+",exp="+S(GetEXPForLevel(t.level))+",aa_points="+S(t.monster_points)+",aa_points_spent=0,aa_exp=0,e_percent_to_aa=0,e_expended_aa_spent=0,e_aa_effects=0,cur_hp="+S(t.hp)+",mana="+S(t.mana)+",endurance="+S(t.endurance)+",str="+S(t.attributes[0])+",sta="+S(t.attributes[1])+",cha="+S(t.attributes[2])+",dex="+S(t.attributes[3])+",`int`="+S(t.attributes[4])+",agi="+S(t.attributes[5])+",wis="+S(t.attributes[6])+" WHERE id="+actor);
   Q(db,"UPDATE character_currency SET copper=0,silver=0,gold=0,platinum=0,copper_cursor=0,silver_cursor=0,gold_cursor=0,platinum_cursor=0 WHERE id="+actor);
   // Class-native skills are bounded by the server's own skill cap tables.
   std::string skills;for(unsigned skill=0;skill<100;++skill){auto cap=MaxSkill(EQ::skills::SkillType(skill),t.class_id,t.level);if(cap){if(!skills.empty())skills+=",";skills+="("+actor+","+S(skill)+","+S(cap)+")";}}
   if(!skills.empty())Q(db,"INSERT INTO character_skills(id,skill_id,value) VALUES "+skills);
   Q(db,"INSERT INTO character_spells(id,slot_id,spell_id) SELECT "+actor+",slot_id,spell_id FROM spirit_shroud_spells WHERE template_id="+S(id));
   std::set<uint32_t> granted_abilities;
   for(auto rank_id:t.abilities){auto* rank=zone->GetAlternateAdvancementRank(rank_id);
    if(!rank||!rank->base_ability||rank->cost<0||(rank->base_ability->category!=3&&rank->base_ability->category!=4)||rank->level_req>t.level||!(rank->base_ability->classes & (1U<<t.class_id)))throw std::runtime_error("invalid shroud AA definition");
    unsigned chain=0;
    for(auto* predecessor=rank;predecessor;predecessor=predecessor->prev){
     if(++chain>300 || predecessor->base_ability!=rank->base_ability ||
        std::find(t.abilities.begin(),t.abilities.end(),predecessor->id)==t.abilities.end())
      throw std::runtime_error("incomplete shroud AA chain");
    }
    if(chain!=rank->current_value)throw std::runtime_error("invalid shroud AA rank topology");
    if(std::find(t.free_abilities.begin(),t.free_abilities.end(),rank_id)!=t.free_abilities.end()){
     granted_abilities.insert(rank->base_ability->id);
     if(granted_abilities.size()>MAX_PP_AA_ARRAY)throw std::runtime_error("shroud ownership capacity exceeded");
     Q(db,"INSERT INTO character_alternate_abilities(id,aa_id,aa_value,charges) VALUES ("+actor+","+S(rank->base_ability->first_rank_id)+","+S(rank->current_value)+",0) ON DUPLICATE KEY UPDATE aa_value=GREATEST(aa_value,VALUES(aa_value))");
    }
   }
   auto starters=Q(db,"SELECT slot_id,item_id FROM spirit_shroud_starter_equipment WHERE template_id="+S(id)+" ORDER BY slot_id FOR UPDATE");
   if(starters.RowCount()>20)throw std::runtime_error("too many shroud starter items");
   for(auto starter:starters){
    const auto slot=N(starter[0]),item_id=N(starter[1]);
    if(slot>=EQ::invslot::slotCursor || item_id>UINT32_MAX)throw std::runtime_error("invalid shroud starter slot");
    std::unique_ptr<EQ::ItemInstance> item(database.CreateItem(uint32_t(item_id),1));
    if(!item || item->IsStackable() || item->IsEvolving())throw std::runtime_error("unsupported shroud starter definition");
    item->SetCustomData(SpiritShroud::StarterSessionKey,S(session_id));
    auto guid=uint64_t(item->GetSerialNumber());if(!guid || guid>INT32_MAX)throw std::runtime_error("invalid shroud starter identity");
    const auto token=SpiritShroud::StarterToken(session_id,guid);
    item->SetCustomData(SpiritShroud::StarterTokenKey,token);
    auto collision=Q(db,"SELECT guid FROM shroud_original_inventory WHERE character_id="+actor+" AND guid="+S(guid)+" UNION ALL SELECT guid FROM inventory WHERE character_id="+actor+" AND guid="+S(guid));
    if(collision.RowCount())throw std::runtime_error("shroud starter identity collision");
    Q(db,"INSERT INTO inventory(character_id,slot_id,item_id,charges,instnodrop,custom_data,guid) VALUES ("+actor+","+S(slot)+","+S(item_id)+",1,1,'"+db.Escape(item->GetCustomDataString())+"',"+S(guid)+")");
    Q(db,"INSERT INTO spirit_shroud_starter_provenance(session_id,guid,item_id,issued_slot,issued_token) VALUES ("+S(session_id)+","+S(guid)+","+S(item_id)+","+S(slot)+",'"+db.Escape(token)+"')");
   }
   Q(db,"UPDATE spirit_shroud_state SET template_id="+S(id)+",active_session_id="+S(session_id)+",revision=revision+1 WHERE character_id="+actor);
  } else {
   ValidateSnapshot(db,CharacterID(),m_spirit_shroud_session_id);
   bool personal_bank=false;
   if(m_spirit_shroud_session_id){auto policy=Q(db,"SELECT inventory_policy FROM spirit_shroud_sessions WHERE id="+S(m_spirit_shroud_session_id)+" AND character_id="+actor+" FOR UPDATE");if(policy.RowCount()!=1 || N(policy.begin()[0])>1)throw std::runtime_error("shroud inventory policy unavailable");personal_bank=N(policy.begin()[0])==1;}
   const auto carried=personal_bank?" AND NOT "+PersonalBankDomain():std::string();
   auto bank=Q(db,"SELECT slot_id FROM spirit_shroud_bank_inventory WHERE character_id="+actor+" FOR UPDATE");if(bank.RowCount())throw std::runtime_error("unexpected occupied shroud bank");
   if(m_spirit_shroud_session_id){
    std::vector<SpiritShroud::StarterIssuedItem> issued;
    for(auto row:Q(db,"SELECT guid,item_id,issued_slot,issued_token FROM spirit_shroud_starter_provenance WHERE session_id="+S(m_spirit_shroud_session_id)+" FOR UPDATE"))issued.push_back({N(row[0]),uint32_t(N(row[1])),uint32_t(N(row[2])),row[3]?std::optional<std::string>(row[3]):std::nullopt});
    std::vector<SpiritShroud::StarterHeldItem> held;
    for(auto row:Q(db,"SELECT slot_id,item_id,guid,custom_data FROM inventory WHERE character_id="+actor+carried+" FOR UPDATE")){
     auto* data=database.GetItem(uint32_t(N(row[1])));if(!data)throw std::runtime_error("shroud item definition missing");
     held.push_back({uint32_t(N(row[0])),uint32_t(N(row[1])),N(row[2]),data->ItemClass==EQ::item::ItemClassBag,row[3]?row[3]:""});
    }
    auto removal=SpiritShroud::PlanStarterRemoval(m_spirit_shroud_session_id,held,issued,EQ::invslot::slotCursor,[](uint32_t slot){
     std::vector<uint32_t> children;
     if(slot>=EQ::invslot::GENERAL_BEGIN && slot<=EQ::invslot::GENERAL_END)
      for(unsigned i=0;i<EQ::invbag::SLOT_COUNT;++i)children.push_back(EQ::InventoryProfile::CalcSlotId(slot,i));
     return children;
    });
    for(auto slot:removal.discard)Q(db,"DELETE FROM inventory WHERE character_id="+actor+" AND slot_id="+S(slot));
    for(auto slots:removal.promote)Q(db,"UPDATE inventory SET slot_id="+S(slots.second)+" WHERE character_id="+actor+" AND slot_id="+S(slots.first));
   }
   Q(db,"INSERT INTO spirit_shroud_bank_inventory SELECT * FROM inventory WHERE character_id="+actor+carried);
   // New sessions retain the live personal-bank domain. Legacy sessions still
   // restore the exact bank snapshot they took before this policy existed.
   if(personal_bank)Q(db,"UPDATE shroud_original_character_currency b JOIN character_currency c ON c.id=b.id SET b.copper_bank=c.copper_bank,b.silver_bank=c.silver_bank,b.gold_bank=c.gold_bank,b.platinum_bank=c.platinum_bank WHERE b.id="+actor);
   // Earned carried coins return with the player.
   auto wallet=Q(db,"SELECT b.copper+c.copper,b.silver+c.silver,b.gold+c.gold,b.platinum+c.platinum FROM shroud_original_character_currency b JOIN character_currency c ON c.id=b.id WHERE b.id="+actor+" FOR UPDATE");
   if(wallet.RowCount()!=1)throw std::runtime_error("shroud wallet snapshot missing");for(unsigned c=0;c<4;++c)if(N(wallet.begin()[c])>INT32_MAX)throw std::runtime_error("shroud wallet overflow");
   Q(db,"UPDATE shroud_original_character_currency b JOIN character_currency c ON c.id=b.id SET b.copper=b.copper+c.copper,b.silver=b.silver+c.silver,b.gold=b.gold+c.gold,b.platinum=b.platinum+c.platinum WHERE b.id="+actor);
   if(m_spirit_shroud_session_id){
    auto pending=Q(db,"SELECT "+std::string(ExperienceColumns)+" FROM spirit_shroud_experience_state WHERE session_id="+S(m_spirit_shroud_session_id)+" AND settled_at IS NULL FOR UPDATE");
    if(pending.RowCount()){
     auto experience=ReadExperience(pending.begin());
     // Pending rewards and the restored profile are committed together. A crash
     // cannot acknowledge the session while leaving an award to replay later.
     Q(db,"UPDATE shroud_original_character_data SET exp="+S(experience.normal)+",aa_exp="+S(experience.aa)+",aa_points="+S(experience.aa_points)+",level="+S(experience.level)+",level2="+S(experience.level2)+",points="+S(experience.skill_points)+" WHERE id="+actor);
     Q(db,"UPDATE spirit_shroud_experience_state SET settled_at="+S(Now())+" WHERE session_id="+S(m_spirit_shroud_session_id)+" AND settled_at IS NULL");
     Q(db,"UPDATE spirit_shroud_experience_events SET settled_at="+S(Now())+" WHERE session_id="+S(m_spirit_shroud_session_id)+" AND settled_at IS NULL");
    }
   }
   const char* fields="race,class,gender,level,level2,points,exp,aa_points,aa_points_spent,aa_exp,e_percent_to_aa,e_expended_aa_spent,e_aa_effects,cur_hp,mana,endurance,str,sta,cha,dex,int,agi,wis";
   std::string assignments;for(auto& f:Strings::Split(fields,',')){if(!assignments.empty())assignments+=",";assignments+="c.`"+f+"`=b.`"+f+"`";}
   auto original=Q(db,"SELECT id FROM shroud_original_character_data WHERE id="+actor+" FOR UPDATE");if(original.RowCount()!=1)throw std::runtime_error("shroud original profile missing");
   Q(db,"UPDATE character_data c JOIN shroud_original_character_data b ON c.id=b.id SET "+assignments+" WHERE c.id="+actor);
   if(m_spirit_shroud_session_id){
    auto level=Q(db,"SELECT level FROM character_data WHERE id="+actor);if(level.RowCount()!=1 || N(level.begin()[0])>255)throw std::runtime_error("shroud restored level invalid");
    for(auto fee:Q(db,"SELECT merc_id,template_id FROM spirit_shroud_mercenary_fees WHERE character_id="+actor+" AND session_id="+S(m_spirit_shroud_session_id)+" AND paid_at IS NULL FOR UPDATE")){
     if(!zone->GetMercTemplate(uint32_t(N(fee[1]))))continue; // Preserve the recorded liability if content is temporarily unavailable.
     const auto cost=RuleB(Mercs,ChargeMercPurchaseCost)?uint64_t(Merc::CalcPurchaseCost(uint32_t(N(fee[1])),uint8_t(N(level.begin()[0]))))*100:0;
     Q(db,"UPDATE spirit_shroud_mercenary_fees SET original_level="+S(N(level.begin()[0]))+",fee_copper="+S(cost)+" WHERE merc_id="+S(N(fee[0]))+" AND character_id="+actor+" AND paid_at IS NULL");
    }
   }
   for(auto table:Tables){std::string name=table.name,key=table.key;auto where=" WHERE "+key+"="+actor;
    if(name!="character_data"){Q(db,"DELETE FROM "+name+where+(name=="inventory"?carried:std::string()));Q(db,"INSERT INTO "+name+" SELECT * FROM shroud_original_"+name+where);}
    Q(db,"DELETE FROM shroud_original_"+name+where);
   }
   if(m_spirit_shroud_session_id){
    auto settled=Q(db,"UPDATE spirit_shroud_sessions SET phase='restored',restored_at="+S(Now())+" WHERE id="+S(m_spirit_shroud_session_id)+" AND character_id="+actor+" AND phase='active'");
    if(settled.RowsAffected()!=1)throw std::runtime_error("shroud restoration already settled");
   }
   Q(db,"UPDATE spirit_shroud_state SET template_id=0,active_session_id=NULL,revision=revision+1 WHERE character_id="+actor);
  }
  // Raid class/level are independent persisted/cache fields. Change them in
  // the profile transaction for both native and reload transitions; retain
  // every membership, group, leadership and loot-role column verbatim.
  auto raid_rows=Q(db,"SELECT raidid,name FROM raid_members WHERE charid="+actor+" AND bot_id=0 FOR UPDATE");
  auto* current_raid=GetRaid();
  if(raid_rows.RowCount()>1 || bool(raid_rows.RowCount())!=bool(current_raid))throw std::runtime_error("shroud raid membership changed");
  if(current_raid){
   auto engine=Q(db,"SELECT ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='raid_members'");
   if(engine.RowCount()!=1 || !engine.begin()[0] || std::string(engine.begin()[0])!="InnoDB")throw std::runtime_error("shroud raid storage must be transactional");
   if(N(raid_rows.begin()[0])!=current_raid->GetID() || !raid_rows.begin()[1] || std::string(raid_rows.begin()[1])!=GetName() || !current_raid->IsRaidMember(this))throw std::runtime_error("shroud raid identity changed");
   auto profile=Q(db,"SELECT class,level FROM character_data WHERE id="+actor);
   if(profile.RowCount()!=1 || N(profile.begin()[0])<1 || N(profile.begin()[0])>16 || N(profile.begin()[1])<1 || N(profile.begin()[1])>126)throw std::runtime_error("shroud raid destination profile invalid");
   raid_profile_id=current_raid->GetID();raid_profile_class=N(profile.begin()[0]);raid_profile_level=N(profile.begin()[1]);
   Q(db,"UPDATE raid_members r JOIN character_data c ON c.id=r.charid SET r._class=c.class,r.level=c.level WHERE r.raidid="+S(raid_profile_id)+" AND r.charid="+actor+" AND r.bot_id=0 AND r.name='"+db.Escape(GetName())+"'");
  }
  // Prepare complete destination caches and all wire bytes while rollback is
  // still possible. Unsupported state deliberately selects the proven reload
  // route; no partially serialized native packet is ever sent after commit.
  auto destination=RuleB(Character,NativeSpiritShroudTransitions)?PrepareSpiritShroudDestination(db):nullptr;
  auto identity=Q(db,"SELECT CONNECTION_ID(),@@in_transaction");if(N(identity.begin()[0])!=connection||N(identity.begin()[1])!=1)throw std::runtime_error("shroud connection changed");
  m_spirit_shroud_transition=true;committing=true;auto result=db.QueryDatabase(std::string("COMMIT"),false);
  if(!result.Success()){Disconnect();return;}started=false;
  // The checkpoint persisted a regular pet before its rows entered escrow.
  // Detach the old actor only after commit, without saving its stale owner
  // cache back over the destination. Original saved pets return via the full
  // reload fallback until their native actor bundle is supported.
  if(auto* pet=GetPet()){
   if(!pet->IsNPC() || pet->IsCharmed())throw std::runtime_error("shroud committed pet actor changed");
   pet->SetOwnerID(0);SetPetID(0);pet->CastToNPC()->Depop(false);
  }
  m_petinfo={};m_suspendedminion={};
  m_spirit_shroud_keeper=0;m_spirit_shroud_menu_until=0;
  if(destination){
   if(!InstallSpiritShroudDestination(*destination))throw std::runtime_error("shroud committed cache installation failed");
   BeginSpiritShroudNativeAcknowledgement(destination->revision);
   SendAlternateAdvancementTable();
   Send(*this,OP_SpiritShroudTransform,destination->self_bytes);
   SendAlternateAdvancementPoints();SendAlternateAdvancementStats();SendAlternateAdvancementTimers();
   // The compact profile uses only the proved empty-affect representation.
   // Rebuild supported active affects with caster, duration, hit count and
   // cast coordinates. Counter/rune bookkeeping uses the reload fallback.
   for(int slot=0;slot<GetMaxTotalSlots();++slot)
    if(IsValidSpell(buffs[slot].spellid))SendBuffDurationPacket(buffs[slot],slot,true);
   auto* icons=MakeBuffsPacket(false);FastQueuePacket(&icons);
   EQApplicationPacket targets(OP_XTargetResponse,8);
   targets.WriteUInt32(GetMaxXTargets());targets.WriteUInt32(0);QueuePacket(&targets);
   SendXTargetUpdates();
   const auto lower=GetEXPForLevel(GetLevel()),upper=GetEXPForLevel(GetLevel()+1);
   if(lower!=UINT32_MAX && upper!=UINT32_MAX && upper>lower){
    const auto ratio=std::clamp((double(GetEXP())-lower)/double(upper-lower),0.0,1.0);
    EQApplicationPacket xp(OP_ExpUpdate,sizeof(ExpUpdate_Struct));
    reinterpret_cast<ExpUpdate_Struct*>(xp.pBuffer)->exp=uint32_t(330.0*ratio);QueuePacket(&xp);
   }
   RemoveAllNimbusEffects();
   EQApplicationPacket observer(OP_SpiritShroudTransform,destination->observer_bytes.size());
   std::copy(destination->observer_bytes.begin(),destination->observer_bytes.end(),observer.pBuffer);
   entity_list.QueueClients(this,&observer,true);
   for(int slot=0;slot<GetMaxTotalSlots();++slot)if(IsValidSpell(buffs[slot].spellid)){
    const auto nimbus=GetSpellNimbusEffect(buffs[slot].spellid);
    if(nimbus && !IsNimbusEffectActive(nimbus))SendSpellEffect(nimbus,500,0,1,3000,true);
   }
   if(auto* group=GetGroup()){group->UpdatePlayer(this);group->SendHPManaEndPacketsTo(this);}
   if(raid_profile_id){
    auto* raid=GetRaid();
    if(!raid || raid->GetID()!=raid_profile_id || !raid->RefreshMemberProfile(this,raid_profile_class,raid_profile_level))throw std::runtime_error("shroud raid profile publication failed");
    raid->SendHPManaEndPacketsFrom(this);raid->SendHPManaEndPacketsTo(this);
   }
   LogInfo("Spirit Shroud native transition queued for character [{}], revision [{}], template [{}]",CharacterID(),m_spirit_shroud_revision,m_spirit_shroud_template);
   return;
  }
  // Fallback reconstructs every cache from committed rows. The old Client is
  // kept write-fenced until deletion, exactly as in the previous implementation.
  if(raid_profile_id){
   auto* raid=GetRaid();
   if(!raid || raid->GetID()!=raid_profile_id || !raid->RefreshMemberProfile(this,raid_profile_class,raid_profile_level))throw std::runtime_error("shroud raid reload publication failed");
  }
  MovePC(zone->GetZoneID(),zone->GetInstanceID(),GetX(),GetY(),GetZ(),GetHeading(),1,EvacToSafeCoords);
 }catch(const std::exception& error){
  LogError("Spirit Shroud transition failed: [{}]",error.what());
  if(started&&db.GetStatus()==DBcore::Connected)db.QueryDatabase(std::string("ROLLBACK"),false);
  if(committing){m_spirit_shroud_transition=true;Disconnect();return;}
  Message(Chat::Red,"Your form has not changed. Check your shroud bank, suspended mercenaries, and Spirit Shroud storage.");
 }
}

void Client::AwardSpiritShroudExperience(ExpSource source,uint64_t amount,uint8 conlevel,bool already_calculated,uint64_t aa_amount) {
 if(!IsSpiritShrouded()||!m_spirit_shroud_session_id||!m_spirit_shroud_definition||
    m_spirit_shroud_transition||m_inventory_session_rejected||!IsEXPEnabled()||(!amount&&!aa_amount)||
    (RuleB(Housing,EnablePlayerHousing)&&!HasInventorySessionReservation()))return;
 Database db;auto* cfg=EQEmuConfig::get();bool started=false,committing=false;
 try {
  if(!cfg||!db.Connect(cfg->DatabaseHost,cfg->DatabaseUsername,cfg->DatabasePassword,cfg->DatabaseDB,cfg->DatabasePort,"shroud-experience"))throw std::runtime_error("shroud experience connection");
  Schema(db);Q(db,"START TRANSACTION");started=true;
  auto connection=N(Q(db,"SELECT CONNECTION_ID()").begin()[0]);LockActor(db,*this);const auto actor=S(CharacterID()),session=S(m_spirit_shroud_session_id);
  auto identity=Q(db,"SELECT template_id,revision,active_session_id FROM spirit_shroud_state WHERE character_id="+actor+" FOR UPDATE");
  if(identity.RowCount()!=1 || N(identity.begin()[0])!=m_spirit_shroud_template || N(identity.begin()[1])!=m_spirit_shroud_revision || !identity.begin()[2] || N(identity.begin()[2])!=m_spirit_shroud_session_id)throw std::runtime_error("shroud experience session changed");
  auto active=Q(db,"SELECT id FROM spirit_shroud_sessions WHERE id="+session+" AND character_id="+actor+" AND phase='active' FOR UPDATE");
  if(active.RowCount()!=1)throw std::runtime_error("shroud experience session inactive");
  auto rows=Q(db,"SELECT "+std::string(ExperienceColumns)+" FROM spirit_shroud_experience_state WHERE session_id="+session+" AND settled_at IS NULL FOR UPDATE");
  if(rows.RowCount()!=1)throw std::runtime_error("shroud experience state unavailable; restore and enter again");
  auto pending=ReadExperience(rows.begin());const auto before=pending;
  uint64_t normal=amount,aa=aa_amount,progression=SpiritShroud::SaturatingAdd(amount,aa_amount);
  if(!already_calculated){
   if(amount>UINT32_MAX)throw std::runtime_error("shroud experience award too large");
   CalculateSpiritShroudExperience(amount,conlevel,pending.level>50?pending.percent_to_aa:0,
    uint64_t(pending.aa_spent)+pending.aa_points,normal,aa,progression);
  }
  if(normal>INT64_MAX || aa>INT64_MAX || progression>INT64_MAX)throw std::runtime_error("shroud experience award overflow");
  SpiritShroud::ApplyExperience(pending,normal,aa,RuleI(AA,UnusedAAPointCap));
  const uint64_t normal_delta=pending.normal-before.normal;
  const uint64_t aa_delta=uint64_t(pending.aa_points-before.aa_points)*pending.aa_unit+pending.aa-before.aa;
  auto progress=Q(db,"SELECT experience FROM spirit_shroud_progress WHERE character_id="+actor+" AND progression_id="+S(m_spirit_shroud_definition->progression)+" FOR UPDATE");
  const uint64_t previous_progress=progress.RowCount()?N(progress.begin()[0]):0;
  const uint64_t next_progress=SpiritShroud::SaturatingAdd(previous_progress,progression);
  if(next_progress==previous_progress && !normal_delta && !aa_delta){Q(db,"ROLLBACK");started=false;return;}
  Q(db,"INSERT INTO spirit_shroud_progress(character_id,progression_id,experience) VALUES ("+actor+","+S(m_spirit_shroud_definition->progression)+","+S(next_progress)+") ON DUPLICATE KEY UPDATE experience=VALUES(experience)");
  Q(db,"UPDATE spirit_shroud_experience_state SET normal_exp="+S(pending.normal)+",aa_exp="+S(pending.aa)+",aa_points="+S(pending.aa_points)+",level="+S(pending.level)+",level2="+S(pending.level2)+",skill_points="+S(pending.skill_points)+" WHERE session_id="+session+" AND settled_at IS NULL");
  Q(db,"INSERT INTO spirit_shroud_experience_events(session_id,event_key,source,progression_delta,normal_delta,aa_delta,created_at) VALUES ("+session+",UNHEX(REPLACE(UUID(),'-','')),"+S(static_cast<uint32_t>(source))+","+S(next_progress-previous_progress)+","+S(normal_delta)+","+S(aa_delta)+","+S(Now())+")");
  auto check=Q(db,"SELECT CONNECTION_ID(),@@in_transaction");if(N(check.begin()[0])!=connection || N(check.begin()[1])!=1)throw std::runtime_error("shroud experience connection changed");
  committing=true;auto commit=db.QueryDatabase(std::string("COMMIT"),false);
  if(!commit.Success()){m_spirit_shroud_transition=true;Disconnect();return;}started=false;
  if(Now()>m_spirit_shroud_progress_sent+5)SendSpiritShroudProgress();
  Message(Chat::Experience,"You gain Spirit Shroud experience. Your character's earned experience will be applied when you remove the shroud.");
 } catch(const std::exception& error){
  if(started&&db.GetStatus()==DBcore::Connected)db.QueryDatabase(std::string("ROLLBACK"),false);
  LogError("Spirit Shroud experience rejected for character [{}], session [{}]: [{}]",CharacterID(),m_spirit_shroud_session_id,error.what());
  if(committing){m_spirit_shroud_transition=true;Disconnect();return;}
  Message(Chat::Red,"Your Spirit Shroud experience could not be saved; this award was withheld.");
 }
}

void Client::OpenSpiritShroudBank() {
 if(!Keeper(*this)||IsSpiritShrouded()||m_spirit_shroud_transition)return;
 try {
  auto rows=Q(database,"SELECT slot_id,item_id,charges FROM spirit_shroud_bank_inventory WHERE character_id="+S(CharacterID())+" AND slot_id<"+S(EQ::invslot::slotCursor)+" ORDER BY slot_id LIMIT 1001");
  if(rows.RowCount()>1000)throw std::runtime_error("shroud bank too large");
  m_spirit_shroud_keeper=GetTarget()->GetID();m_spirit_shroud_menu_until=Now()+120;
  Bytes open;Put32(open,0);Put32(open,m_spirit_shroud_keeper);Send(*this,OP_SpiritShroudBank,open);
  Bytes list;Put32(list,0);Put32(list,rows.RowCount());
  for(auto r:rows){auto* item=database.GetItem(uint32_t(N(r[1])));if(!item)throw std::runtime_error("shroud bank item definition missing");Text(list,item->Name);Put32(list,item->Icon);Put64(list,N(r[0])+1);Put32(list,item->Stackable?std::max<uint64_t>(1,N(r[2])):1);}
  Send(*this,OP_SpiritShroudBankItems,list);
 }catch(...){Message(Chat::Red,"Your shroud bank is unavailable; its items remain stored.");}
}

void Client::Handle_OP_SpiritShroudBank(const EQApplicationPacket* p) {
 if(p->size<4||IsSpiritShrouded()||m_spirit_shroud_transition)return;
 auto action=Read32(p->pBuffer);
 if(action==0&&p->size==8&&Keeper(*this,Read32(p->pBuffer+4))){OpenSpiritShroudBank();return;}
 if(action==5 && p->size==24 && Now()<=m_spirit_shroud_menu_until && Keeper(*this,m_spirit_shroud_keeper)) {
  uint64_t key=Read32(p->pBuffer+8)|(uint64_t(Read32(p->pBuffer+12))<<32);if(!key||key>9000)return;
  try{auto rows=Q(database,"SELECT item_id,charges,augment_one,augment_two,augment_three,augment_four,augment_five,augment_six,instnodrop,custom_data,ornament_icon,ornament_idfile,ornament_hero_model FROM spirit_shroud_bank_inventory WHERE character_id="+S(CharacterID())+" AND slot_id="+S(key-1));
   if(rows.RowCount()!=1)return;auto r=rows.begin();std::unique_ptr<EQ::ItemInstance> item(database.CreateItem(N(r[0]),int16_t(std::stoi(r[1])),N(r[2]),N(r[3]),N(r[4]),N(r[5]),N(r[6]),N(r[7]),N(r[8])!=0,r[9]?r[9]:"",N(r[10]),N(r[11]),N(r[12])));
   if(item)SendItemPacket(EQ::invslot::slotCursor,item.get(),ItemPacketViewLink);
  }catch(...){}return;
 }
 if(action!=2||p->size!=16)return;
 const auto decline=[this]() {
  // The proven all-ones key leaves rows intact while completing one pending
  // Get request (0x61dad9 -> 0x61db47). Do not strand the native wait counter.
  Bytes response;Put32(response,2);Put32(response,0);Put64(response,std::numeric_limits<uint64_t>::max());
  Send(*this,OP_SpiritShroudBank,response);
 };
 if(Now()>m_spirit_shroud_menu_until||!Keeper(*this,m_spirit_shroud_keeper)){decline();return;}
 uint64_t key=Read32(p->pBuffer+8)|(uint64_t(Read32(p->pBuffer+12))<<32);if(!key||key>9000){decline();return;}
 auto old=int16_t(key-1);if(old>=EQ::invslot::slotCursor){decline();return;}
 int16_t free=GetInv().FindFreeSlot(true,false);if(free<EQ::invslot::GENERAL_BEGIN||free>EQ::invslot::GENERAL_END){Message(Chat::Yellow,"Make room in your main inventory first.");decline();return;}
 Database db;auto* cfg=EQEmuConfig::get();bool started=false,committing=false;
 try{
  if(!cfg||!db.Connect(cfg->DatabaseHost,cfg->DatabaseUsername,cfg->DatabasePassword,cfg->DatabaseDB,cfg->DatabasePort,"shroud-bank"))throw std::runtime_error("bank connect");
  Schema(db);Q(db,"START TRANSACTION");started=true;auto connection=N(Q(db,"SELECT CONNECTION_ID()").begin()[0]);LockActor(db,*this);auto actor=S(CharacterID());
  auto state=Q(db,"SELECT template_id FROM spirit_shroud_state WHERE character_id="+actor+" FOR UPDATE");if(state.RowCount()!=1||N(state.begin()[0]))throw std::runtime_error("bank state changed");
  std::vector<std::pair<int16_t,int16_t>> slots={{old,free}};
  for(unsigned i=0;i<EQ::invbag::SLOT_COUNT;++i)slots.emplace_back(EQ::InventoryProfile::CalcSlotId(old,i),EQ::InventoryProfile::CalcSlotId(free,i));
  auto root=Q(db,"SELECT item_id FROM spirit_shroud_bank_inventory WHERE character_id="+actor+" AND slot_id="+S(old)+" FOR UPDATE");if(root.RowCount()!=1)throw std::runtime_error("bank stale item");
  auto columns=Q(db,"SELECT column_name FROM information_schema.columns WHERE table_schema=DATABASE() AND table_name='inventory' ORDER BY ordinal_position");
  std::string cols,select;for(auto r:columns){std::string col=r[0];if(!cols.empty()){cols+=",";select+=",";}cols+="`"+col+"`";select+=(col=="slot_id"?"SLOT_PLACEHOLDER":"`"+col+"`");}
  for(auto pair:slots){if(pair.first<0||pair.second<0)continue;auto occupied=Q(db,"SELECT slot_id FROM inventory WHERE character_id="+actor+" AND slot_id="+S(pair.second)+" FOR UPDATE");if(occupied.RowCount())throw std::runtime_error("bank destination changed");
   auto values=select;auto at=values.find("SLOT_PLACEHOLDER");values.replace(at,16,S(pair.second));
   Q(db,"INSERT INTO inventory ("+cols+") SELECT "+values+" FROM spirit_shroud_bank_inventory WHERE character_id="+actor+" AND slot_id="+S(pair.first));
   Q(db,"DELETE FROM spirit_shroud_bank_inventory WHERE character_id="+actor+" AND slot_id="+S(pair.first));
  }
  // Stage the exact committed destination tree and normal item wire before
  // crossing COMMIT. Unsupported legacy state retains full reload recovery.
  std::unique_ptr<EQ::ItemInstance> prepared_item;
  std::optional<Bytes> prepared_wire;
  uint16_t item_opcode=0;
  if(ClientVersion()==EQ::versions::ClientVersion::RoF2 && eqs && eqs->GetOpcodeManager()) {
   item_opcode=eqs->GetOpcodeManager()->EmuToEQ(OP_ItemPacket);
   auto snapshot=InventoryRepository::GetLoadSnapshot(db,CharacterID(),true);
   if(snapshot.success && item_opcode) {
    const auto row_from=[](const InventoryRepository::LoadRow& load) {
     Housing::LiveItems::InventoryRow row;row.values=load.values;
     if(!load.fields[2])row.null_fields|=Housing::LiveItems::NullItemId;
     if(!load.fields[3])row.null_fields|=Housing::LiveItems::NullCharges;
     if(!load.fields[12])row.null_fields|=Housing::LiveItems::NullCustomData;
     if(!load.fields[16])row.null_fields|=Housing::LiveItems::NullGuid;
     return row;
    };
    const InventoryRepository::LoadRow* root_row=nullptr;
    for(const auto& load:snapshot.rows)if(load.values.slot_id==uint32_t(free))root_row=&load;
    if(root_row) {
     auto destination=row_from(*root_row);
     for(const auto& load:snapshot.rows)if(load.values.slot_id!=uint32_t(free) && EQ::InventoryProfile::CalcSlotId(load.values.slot_id)==free) {
      auto child=row_from(load);child.values.slot_id=EQ::InventoryProfile::CalcBagIdx(load.values.slot_id);destination.children.push_back(std::move(child));
     }
     auto rebuilt=Housing::InventoryBridge::ReconstructCharacterProfile(database,destination);
     if(rebuilt) {
      prepared_item=std::move(rebuilt.value.item);
      prepared_wire=RoF2::SerializeShroudBankItem(*prepared_item,free);
      if(!prepared_wire)prepared_item.reset();
     }
    }
   }
  }
  if(prepared_item) {
   // Runtime serials are ephemeral handles, not item custody identity. Persist
   // this exact newly reconstructed tree's handles in the same owned transaction
   // so ordinary inventory saves/lease checks see the same projection.
   Q(db,"UPDATE inventory SET guid="+S(prepared_item->GetSerialNumber())+" WHERE character_id="+actor+" AND slot_id="+S(free));
   for(unsigned i=0;i<EQ::invbag::SLOT_COUNT;++i)if(const auto* child=prepared_item->GetItem(i))
    Q(db,"UPDATE inventory SET guid="+S(child->GetSerialNumber())+" WHERE character_id="+actor+" AND slot_id="+S(EQ::InventoryProfile::CalcSlotId(free,i)));
  }
  auto identity=Q(db,"SELECT CONNECTION_ID(),@@in_transaction");if(N(identity.begin()[0])!=connection||N(identity.begin()[1])!=1)throw std::runtime_error("bank connection changed");
  m_spirit_shroud_transition=true;committing=true;auto commit=db.QueryDatabase(std::string("COMMIT"),false);if(!commit.Success()){Disconnect();return;}started=false;
  if(!prepared_item) {
   LogInfo("Shroud bank retrieval reload fallback character [{}]",CharacterID());
   MovePC(zone->GetZoneID(),zone->GetInstanceID(),GetX(),GetY(),GetZ(),GetHeading(),1,EvacToSafeCoords);return;
  }
  if(!eqs || GetInv().GetItem(free))throw std::runtime_error("bank destination cache changed");
  auto* installed=prepared_item.get();
  if(m_inv.PushItem(free,installed)!=free)throw std::runtime_error("bank destination cache install failed");
  prepared_item.release();
  EQApplicationPacket item_packet(OP_Unknown,prepared_wire->size());
  std::copy(prepared_wire->begin(),prepared_wire->end(),item_packet.pBuffer);item_packet.SetOpcodeBypass(item_opcode);QueuePacket(&item_packet);
  // Matching RoF2 0x61dacb consumes action2, reserved DWORD, and original
  // uint64 bank key: remove that row and decrement pending retrieval count.
  Bytes acknowledgement;Put32(acknowledgement,2);Put32(acknowledgement,0);Put64(acknowledgement,key);
  Send(*this,OP_SpiritShroudBank,acknowledgement);
  m_spirit_shroud_transition=false;
  CalcBonuses(); // Same post-inventory refresh as ordinary item admission.
  LogInfo("Native shroud bank retrieval completed character [{}]",CharacterID());
 }catch(...){if(started&&db.GetStatus()==DBcore::Connected)db.QueryDatabase(std::string("ROLLBACK"),false);if(committing){m_spirit_shroud_transition=true;Disconnect();}else {Message(Chat::Red,"The item remains in your shroud bank.");decline();}}
}


bool Client::HasSpiritShroudRank(uint32_t rank) const {return IsSpiritShrouded() && m_spirit_shroud_aa_ranks.count(rank);}
void Client::SendSpiritShroudProgress() {
 if(!RuleB(Character,EnableSpiritShrouds)||m_spirit_shroud_transition)return;
 try {
  auto families=Q(database,"SELECT progression_id,name,completion_xp,branch_count,definition_version FROM spirit_shroud_progression_definitions ORDER BY progression_id");
  if(families.RowCount()>64)throw std::runtime_error("invalid shroud progression family count");
  Bytes out;Put32(out,families.RowCount());std::string text;
  for(auto row:families){
   const auto family=N(row[0]),count=N(row[3]),version=N(row[4]);
   if(!family||!row[1]||!count||count>64||!version)throw std::runtime_error("invalid shroud progression definition");
   const auto xp=Progress(database,CharacterID(),family);
   auto branches=Q(database,"SELECT branch_id,unlock_xp FROM spirit_shroud_branch_progression WHERE progression_id="+S(family)+" AND definition_version="+S(version)+" ORDER BY branch_id");
   if(branches.RowCount()!=count)throw std::runtime_error("incomplete shroud progression definition");
   uint64_t unlocked=0;for(auto branch:branches)if(branch[1]&&xp>=N(branch[1]))++unlocked;
   uint64_t percentage=0;
   std::string detail="Spirit experience: "+S(xp);
   if(row[2]&&N(row[2])){
    const auto target=N(row[2]);
    percentage=xp>=target?100:static_cast<uint64_t>(100.0L*xp/target);
    detail+=" / "+S(target);
   }else detail+=" (completion threshold unmeasured)";
   text+=Token("PROGRESSION",'\a')+Token(S(family),'\a')+Token(row[1],'\a')+Token(detail,'\a')+Token(S(unlocked),'\a')+Token(S(count),'\a')+Token("",'\a')+Token("",'\a')+Token(S(percentage),'\a');
  }
  Text(out,text);Send(*this,OP_SpiritShroudProgressReplace,out);m_spirit_shroud_progress_sent=Now();
 }catch(const std::exception& error){LogError("Spirit Shroud progress display unavailable: [{}]",error.what());}
}

void Client::PurchaseSpiritShroudRank(uint32_t rank_id, bool saved_set)
{
 if(!IsSpiritShrouded() || !m_spirit_shroud_session_id || !m_spirit_shroud_definition ||
    m_spirit_shroud_transition || IsDead() || IsEngaged() || GetAggroCount())return;
 auto* target=zone->GetAlternateAdvancementRank(rank_id);
 if(!target || !target->base_ability ||
    (target->base_ability->category!=3 && target->base_ability->category!=4) ||
    !(target->base_ability->classes & (1U<<GetClass())) || target->base_ability->grant_only ||
    target->base_ability->charges || !HasSpiritShroudRank(rank_id) ||
    target->current_value>target->base_ability->GetMaxLevel(this))return;
 const auto root=target->base_ability->first_rank_id;
 const auto owned=GetAA(root);
 if(owned>=target->current_value || (!owned && aa_ranks.size()>=MAX_PP_AA_ARRAY))return;
 std::vector<AA::Rank*> missing;
 for(auto* rank=target;rank && rank->current_value>owned;rank=rank->prev){
  if(missing.size()>=300 || rank->base_ability!=target->base_ability ||
     !HasSpiritShroudRank(rank->id) || rank->level_req>GetLevel() || rank->cost<0)return;
  missing.push_back(rank);
 }
 if(missing.empty() || (!saved_set && missing.size()!=1))return;
 std::reverse(missing.begin(),missing.end());
 if(missing.front()->current_value!=owned+1)return;
 uint64_t cost=0;
 for(auto* rank:missing){
  cost+=rank->cost;if(cost>m_pp.aapoints)return;
  for(const auto& prereq:rank->prereqs){
   auto* ability=zone->GetAlternateAdvancementAbility(prereq.first);
   if(!ability)return;
   auto have=ability->first_rank_id==root ? rank->current_value-1 : GetAA(ability->first_rank_id);
   if(have<prereq.second)return;
  }
 }
 if(!SaveSpiritShroudCheckpoint())return;
 Database db;auto* cfg=EQEmuConfig::get();bool started=false,committing=false;
 try{
  if(!cfg||!db.Connect(cfg->DatabaseHost,cfg->DatabaseUsername,cfg->DatabasePassword,cfg->DatabaseDB,cfg->DatabasePort,"shroud-purchase"))throw std::runtime_error("shroud purchase connection");
  Q(db,"START TRANSACTION");started=true;
  const auto connection=N(Q(db,"SELECT CONNECTION_ID()").begin()[0]);LockActor(db,*this);
  const auto actor=S(CharacterID());
  auto state=Q(db,"SELECT template_id,revision,active_session_id FROM spirit_shroud_state WHERE character_id="+actor+" FOR UPDATE");
  if(state.RowCount()!=1 || N(state.begin()[0])!=m_spirit_shroud_template || N(state.begin()[1])!=m_spirit_shroud_revision || !state.begin()[2] || N(state.begin()[2])!=m_spirit_shroud_session_id)throw std::runtime_error("shroud purchase session changed");
  auto points=Q(db,"SELECT aa_points,aa_points_spent FROM character_data WHERE id="+actor+" FOR UPDATE");
  if(points.RowCount()!=1 || N(points.begin()[0])!=m_pp.aapoints || N(points.begin()[1])!=m_pp.aapoints_spent || cost>N(points.begin()[0]) || N(points.begin()[1])+cost>UINT32_MAX)throw std::runtime_error("shroud point balance changed");
  auto rank=Q(db,"SELECT aa_value,charges FROM character_alternate_abilities WHERE id="+actor+" AND aa_id="+S(root)+" FOR UPDATE");
  if((rank.RowCount()?N(rank.begin()[0]):0)!=owned || (rank.RowCount() && N(rank.begin()[1])))throw std::runtime_error("shroud ownership changed");
  Q(db,"INSERT INTO character_alternate_abilities(id,aa_id,aa_value,charges) VALUES ("+actor+","+S(root)+","+S(target->current_value)+",0) ON DUPLICATE KEY UPDATE aa_value=VALUES(aa_value),charges=0");
  Q(db,"UPDATE character_data SET aa_points=aa_points-"+S(cost)+",aa_points_spent=aa_points_spent+"+S(cost)+" WHERE id="+actor);
  Q(db,"INSERT INTO spirit_shroud_allocations(session_id,ability_id,rank_id,paid_points) VALUES ("+S(m_spirit_shroud_session_id)+","+S(target->base_ability->id)+","+S(target->id)+","+S(cost)+") ON DUPLICATE KEY UPDATE rank_id=VALUES(rank_id),paid_points=paid_points+VALUES(paid_points)");
  auto identity=Q(db,"SELECT CONNECTION_ID(),@@in_transaction");
  if(N(identity.begin()[0])!=connection || N(identity.begin()[1])!=1)throw std::runtime_error("shroud purchase connection changed");
  m_spirit_shroud_transition=true;committing=true;
  auto result=db.QueryDatabase(std::string("COMMIT"),false);
  if(!result.Success()){Disconnect();return;}started=false;
  if(!SetAA(root,target->current_value,0)){Disconnect();return;}
  m_pp.aapoints-=uint32_t(cost);m_pp.aapoints_spent+=uint32_t(cost);
  m_spirit_shroud_transition=false;
  CalcBonuses();SendAlternateAdvancementTable();SendAlternateAdvancementPoints();SendAlternateAdvancementStats();
 }catch(const std::exception& error){
  if(started && db.GetStatus()==DBcore::Connected)db.QueryDatabase(std::string("ROLLBACK"),false);
  if(committing){m_spirit_shroud_transition=true;Disconnect();return;}
  LogError("Spirit Shroud purchase rejected for character [{}]: [{}]",CharacterID(),error.what());
  Message(Chat::Red,"Your Monster Point purchase was not applied.");
 }
}

bool Client::TrySpiritShroudAbility(uint32_t action,uint32_t ability) {
 if(!IsSpiritShrouded())return false;
 // Saved sets are local to the native client; Load sends existing action 7.
 if(action==3 || action==7){PurchaseSpiritShroudRank(ability,action==7);return true;}
 // The active pool belongs to this session; ordinary XP allocation is frozen.
 if(action!=0)return true;
 if(ability!=3577)return false;
 if(!HasSpiritShroudRank(3577)||!GetAA(3577)||IsDead()||IsEngaged()||GetAggroCount())return true;
 try {auto r=Q(database,"SELECT zone_id,zone_instance,x,y,z,heading FROM shroud_original_character_data WHERE id="+S(CharacterID()));
  if(r.RowCount()!=1)return true;auto row=r.begin();
  auto* rank=zone->GetAlternateAdvancementRank(3577);if(!rank||rank->spell_type<=0||rank->spell_type>pTimerAAEnd-pTimerAAStart)return true;
  auto timer=rank->spell_type;
  if(!p_timers.Expired(&database,pTimerAAStart+timer,false))return true;
  auto z=uint32_t(N(row[0])),instance=uint32_t(N(row[1]));float x=std::stof(row[2]),y=std::stof(row[3]),height=std::stof(row[4]),h=std::stof(row[5]);
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(height)||!std::isfinite(h))return true;
  p_timers.Start(pTimerAAStart+timer,90);SendAlternateAdvancementTimer(timer,time(nullptr),time(nullptr)+90);
  MovePC(z,instance,x,y,height,h,1);return true;
 }catch(...){Message(Chat::Red,"Your shroud master cannot be reached right now.");return true;}
}


bool Client::CompleteSpiritShroudReturn(uint32_t rank_id)
{
 // Each verified form may use its own class-bound Spirit Sending adapter.
 // Membership, ownership, active-cast context and exact spell authenticate it.
 if(!IsSpiritShrouded() || !m_spirit_shroud_session_id ||
    m_spirit_shroud_transition || !HasSpiritShroudRank(rank_id) || IsDead() ||
    IsEngaged() || GetAggroCount())return false;
 auto* rank=zone->GetAlternateAdvancementRank(rank_id);
 if(!rank || !rank->base_ability || rank->base_ability->category!=4 ||
    rank->spell!=7790 || !CanUseAlternateAdvancementRank(rank) ||
    GetAA(rank_id)<uint32_t(rank->current_value))return false;
 try {
  auto result=Q(database,"SELECT b.zone_id,b.zone_instance,b.x,b.y,b.z,b.heading FROM shroud_original_character_data b JOIN character_data c ON c.id=b.id AND c.account_id=b.account_id JOIN spirit_shroud_state s ON s.character_id=b.id JOIN spirit_shroud_sessions p ON p.id=s.active_session_id AND p.character_id=b.id AND p.phase='active' WHERE b.id="+S(CharacterID())+" AND c.account_id="+S(AccountID())+" AND s.template_id="+S(m_spirit_shroud_template)+" AND s.revision="+S(m_spirit_shroud_revision)+" AND s.active_session_id="+S(m_spirit_shroud_session_id));
  if(result.RowCount()!=1)throw std::runtime_error("shroud return session changed");
  auto row=result.begin();auto destination_zone=uint32_t(N(row[0]));auto instance=uint32_t(N(row[1]));
  const float x=std::stof(row[2]),y=std::stof(row[3]),z=std::stof(row[4]),heading=std::stof(row[5]);
  if(!destination_zone || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(heading))
   throw std::runtime_error("shroud return destination invalid");
  MovePC(destination_zone,instance,x,y,z,heading,1);
  return true;
 }catch(const std::exception& error){
  LogError("Spirit Shroud return rejected for character [{}]: [{}]",CharacterID(),error.what());
  Message(Chat::Red,"Your shroud master cannot be reached right now.");
  return false;
 }
}


bool Client::IsSpiritShroudStarterItem(const EQ::ItemInstance* item) const
{
 if(!item || !IsSpiritShrouded() || !m_spirit_shroud_session_id || !m_spirit_shroud_definition)return false;
 const auto* data=item->GetItem();
 if(!data || std::none_of(m_spirit_shroud_definition->equipment.begin(),m_spirit_shroud_definition->equipment.end(),
    [data](const auto& entry){return entry.item_id==data->ID;}))return false;
 try {
  const auto marker=SpiritShroud::ReadStarterMetadata(item->GetCustomDataString());
  if(!marker.session || *marker.session!=S(m_spirit_shroud_session_id))return false;
  // Pre-token retained sessions used only the session marker. Their custody
  // is checked against issuance records on restoration. New tokens keep their
  // original issuance identity when loaded item serial numbers change.
  if(!marker.token)return true;
  const auto prefix="v1:"+S(m_spirit_shroud_session_id)+":";
  if(marker.token->compare(0,prefix.size(),prefix)!=0)return false;
  const auto identity=marker.token->substr(prefix.size());
  const auto issued=N(identity.c_str());
  return issued && issued<=INT32_MAX && *marker.token==SpiritShroud::StarterToken(m_spirit_shroud_session_id,issued);
 }catch(...){return false;}
}

float Client::SpiritShroudModelSize() const
{
 return m_spirit_shroud_definition?m_spirit_shroud_definition->model_size:0.0f;
}

std::optional<uint32_t> Client::SpiritShroudAbilityMinimumLevel(uint16_t spell_id, uint32_t rank_id) const
{
 if(!rank_id || !IsSpiritShrouded() || !m_spirit_shroud_session_id || !zone)
  return std::nullopt;
 const auto* rank=zone->GetAlternateAdvancementRank(rank_id);
 return SpiritShroud::AbilityMinimumLevel(SpiritShroudDefinition(),rank,spell_id,
  GetAA(rank_id),GetClass(),GetLevel(),m_spirit_shroud_transition);
}

namespace {
void ShroudDeathSchema(Database& db)
{
 auto engines=Q(db,"SELECT TABLE_NAME,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('character_corpses','spirit_shroud_corpse_state')");
 if(engines.RowCount()!=2)throw std::runtime_error("shroud corpse storage missing");
 for(auto r:engines)if(!r[1]||std::string(r[1])!="InnoDB")throw std::runtime_error("shroud corpse storage must be transactional");
 auto fields=Q(db,"SELECT COUNT(*) FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='spirit_shroud_corpse_state' AND COLUMN_NAME IN ('provenance_version','created_at','rez_claimed_at','rez_spell_id')");
 if(N(fields.begin()[0])!=4)throw std::runtime_error("shroud corpse migration missing");
}
std::string ShroudCorpseEventKey(uint64_t session,uint32_t corpse,unsigned kind)
{
 return "UNHEX(CONCAT(LPAD(HEX("+S(session)+"),16,'0'),LPAD(HEX("+S(corpse)+"),8,'0'),LPAD(HEX("+S(kind)+"),8,'0')))";
}
}

bool Client::RecordSpiritShroudDeath(uint32_t corpse_id,uint64_t requested_loss,uint64_t& applied_loss)
{
 applied_loss=0;
 if(!IsSpiritShrouded()||!m_spirit_shroud_session_id||!corpse_id||m_spirit_shroud_transition||m_inventory_session_rejected)return false;
 Database db;auto* cfg=EQEmuConfig::get();bool started=false,committing=false;
 try {
  if(!cfg||!db.Connect(cfg->DatabaseHost,cfg->DatabaseUsername,cfg->DatabasePassword,cfg->DatabaseDB,cfg->DatabasePort,"shroud-death"))throw std::runtime_error("shroud death connection");
  Schema(db);ShroudDeathSchema(db);Q(db,"START TRANSACTION");started=true;
  const auto connection=N(Q(db,"SELECT CONNECTION_ID()").begin()[0]);LockActor(db,*this);
  const auto actor=S(CharacterID()),session=S(m_spirit_shroud_session_id),corpse=S(corpse_id);
  auto state=Q(db,"SELECT s.template_id,s.revision,s.active_session_id,p.phase FROM spirit_shroud_state s JOIN spirit_shroud_sessions p ON p.id=s.active_session_id AND p.character_id=s.character_id WHERE s.character_id="+actor+" FOR UPDATE");
  if(state.RowCount()!=1||N(state.begin()[0])!=m_spirit_shroud_template||N(state.begin()[1])!=m_spirit_shroud_revision||N(state.begin()[2])!=m_spirit_shroud_session_id||!state.begin()[3]||std::string(state.begin()[3])!="active")throw std::runtime_error("shroud death session changed");
  auto body=Q(db,"SELECT charid,exp,is_rezzed FROM character_corpses WHERE id="+corpse+" FOR UPDATE");
  if(body.RowCount()!=1||N(body.begin()[0])!=CharacterID())throw std::runtime_error("shroud death corpse missing or foreign");
  auto old=Q(db,"SELECT character_id,session_id,applied_loss,provenance_version FROM spirit_shroud_corpse_state WHERE corpse_id="+corpse+" FOR UPDATE");
  if(old.RowCount()){
   if(N(old.begin()[0])!=CharacterID()||N(old.begin()[1])!=m_spirit_shroud_session_id||N(old.begin()[3])!=1)throw std::runtime_error("shroud death provenance changed");
   applied_loss=N(old.begin()[2]);Q(db,"ROLLBACK");return true;
  }
  if(N(body.begin()[1])||N(body.begin()[2]))throw std::runtime_error("shroud death corpse already has experience");
  auto rows=Q(db,"SELECT "+std::string(ExperienceColumns)+" FROM spirit_shroud_experience_state WHERE session_id="+session+" AND settled_at IS NULL FOR UPDATE");
  if(rows.RowCount()!=1)throw std::runtime_error("shroud death pending experience missing");
  auto pending=ReadExperience(rows.begin());
  // Use the server's existing active-form death formula and configured level
  // floor. This is documented emulator policy; no Live multiplier is invented.
  applied_loss=SpiritShroud::ApplyDeathLoss(pending,std::min<uint64_t>(requested_loss,INT32_MAX),RuleB(Character,DeathKeepLevel));
  Q(db,"UPDATE spirit_shroud_experience_state SET normal_exp="+S(pending.normal)+",level="+S(pending.level)+" WHERE session_id="+session+" AND settled_at IS NULL");
  Q(db,"INSERT INTO spirit_shroud_corpse_state(corpse_id,character_id,session_id,applied_loss,refunded_loss,provenance_version,created_at) VALUES("+corpse+","+actor+","+session+","+S(applied_loss)+",0,1,"+S(Now())+")");
  Q(db,"UPDATE character_corpses SET entity_variables=JSON_OBJECT('spirit_shroud_corpse','1'),exp="+S(applied_loss)+",is_rezzed=0 WHERE id="+corpse+" AND charid="+actor);
  Q(db,"INSERT INTO spirit_shroud_experience_events(session_id,event_key,source,progression_delta,normal_delta,aa_delta,created_at) VALUES("+session+","+ShroudCorpseEventKey(m_spirit_shroud_session_id,corpse_id,1)+","+S(static_cast<uint32_t>(ExpSource::Death))+",0,-"+S(applied_loss)+",0,"+S(Now())+")");
  auto check=Q(db,"SELECT CONNECTION_ID(),@@in_transaction");if(N(check.begin()[0])!=connection||N(check.begin()[1])!=1)throw std::runtime_error("shroud death connection changed");
  committing=true;auto result=db.QueryDatabase(std::string("COMMIT"),false);
  if(!result.Success()){m_spirit_shroud_transition=true;Disconnect();return false;}started=false;
  return true;
 }catch(const std::exception& error){
  if(started&&db.GetStatus()==DBcore::Connected)db.QueryDatabase(std::string("ROLLBACK"),false);
  LogError("Spirit Shroud death accounting rejected for character [{}], corpse [{}]: [{}]",CharacterID(),corpse_id,error.what());
  if(committing)m_spirit_shroud_transition=true;
  Message(Chat::Red,"Your shroud death could not be saved. Reconnect before continuing.");Disconnect();return false;
 }
}

bool Client::CanAcceptSpiritShroudResurrection(uint32_t corpse_id)
{
 if(!IsSpiritShrouded())return true;
 if(!corpse_id||m_spirit_shroud_transition||m_inventory_session_rejected)return false;
 auto r=database.QueryDatabase("SELECT p.corpse_id FROM spirit_shroud_corpse_state p JOIN character_corpses c ON c.id=p.corpse_id AND c.charid=p.character_id WHERE p.corpse_id="+S(corpse_id)+" AND p.character_id="+S(CharacterID())+" AND p.provenance_version=1");
 return r.Success()&&r.RowCount()==1;
}

bool Client::SettleSpiritShroudResurrection(uint32_t corpse_id,uint32_t spell_id,uint64_t offered_loss,bool& handled)
{
 handled=false;
 // A normal corpse keeps its existing path. Missing optional storage is also
 // harmless to ordinary characters when Spirit Shrouds have never been enabled.
 auto known=database.QueryDatabase("SELECT provenance_version FROM spirit_shroud_corpse_state WHERE corpse_id="+S(corpse_id));
 if(!known.Success())return !IsSpiritShrouded()&&!RuleB(Character,EnableSpiritShrouds);
 if(!known.RowCount())return !IsSpiritShrouded();
 handled=true;
 if(N(known.begin()[0])!=1||!corpse_id||m_spirit_shroud_transition||m_inventory_session_rejected||
    PendingRezzXP<0||PendingRezzDBID!=corpse_id||PendingRezzSpellID!=spell_id||!IsValidSpell(spell_id))return false;
 int percent=-1;
 for(int slot=0;slot<EFFECT_COUNT;++slot)if(spells[spell_id].effect_id[slot]==SpellEffect::Revive){percent=spells[spell_id].base_value[slot];break;}
 if(percent<0||percent>100)return false;
 Database db;auto* cfg=EQEmuConfig::get();bool started=false,committing=false,normal_profile=false;SpiritShroud::ExperienceState credited;
 try {
  if(!cfg||!db.Connect(cfg->DatabaseHost,cfg->DatabaseUsername,cfg->DatabasePassword,cfg->DatabaseDB,cfg->DatabasePort,"shroud-resurrection"))throw std::runtime_error("shroud resurrection connection");
  Schema(db);ShroudDeathSchema(db);Q(db,"START TRANSACTION");started=true;
  const auto connection=N(Q(db,"SELECT CONNECTION_ID()").begin()[0]);LockActor(db,*this);const auto actor=S(CharacterID()),corpse=S(corpse_id);
  auto state=Q(db,"SELECT template_id,revision,active_session_id FROM spirit_shroud_state WHERE character_id="+actor+" FOR UPDATE");
  if(state.RowCount()!=1)throw std::runtime_error("shroud resurrection state missing");
  if(IsSpiritShrouded()){
   if(N(state.begin()[0])!=m_spirit_shroud_template||N(state.begin()[1])!=m_spirit_shroud_revision||!state.begin()[2]||N(state.begin()[2])!=m_spirit_shroud_session_id)throw std::runtime_error("shroud resurrection session changed");
  }else if(N(state.begin()[0])||state.begin()[2])throw std::runtime_error("shroud resurrection original profile not active");
  auto provenance=Q(db,"SELECT p.character_id,p.session_id,p.applied_loss,p.refunded_loss,p.provenance_version,p.rez_claimed_at,s.phase FROM spirit_shroud_corpse_state p JOIN spirit_shroud_sessions s ON s.id=p.session_id AND s.character_id=p.character_id WHERE p.corpse_id="+corpse+" FOR UPDATE");
  if(provenance.RowCount()!=1||N(provenance.begin()[0])!=CharacterID()||N(provenance.begin()[4])!=1)throw std::runtime_error("shroud resurrection provenance missing or foreign");
  const auto source_session=N(provenance.begin()[1]),applied=N(provenance.begin()[2]);
  if(applied>INT32_MAX||N(provenance.begin()[3])>applied)throw std::runtime_error("shroud resurrection invalid debit");
  if(!provenance.begin()[6]||(std::string(provenance.begin()[6])!="restored"&&!(IsSpiritShrouded()&&source_session==m_spirit_shroud_session_id&&std::string(provenance.begin()[6])=="active")))throw std::runtime_error("shroud resurrection source session unresolved");
  auto body=Q(db,"SELECT charid,exp,is_rezzed,rezzable FROM character_corpses WHERE id="+corpse+" FOR UPDATE");
  if(body.RowCount()!=1||N(body.begin()[0])!=CharacterID())throw std::runtime_error("shroud resurrection corpse missing or foreign");
  // Repeated acceptance and zero-XP corpse gates cannot credit the debit twice.
  if(provenance.begin()[5]){Q(db,"ROLLBACK");return true;}
  if(N(body.begin()[2])||!N(body.begin()[3])||N(body.begin()[1])!=applied||offered_loss!=applied)throw std::runtime_error("shroud resurrection offer changed or expired");
  uint64_t destination_session=source_session;
  if(IsSpiritShrouded()){
   destination_session=m_spirit_shroud_session_id;
   auto pending=Q(db,"SELECT "+std::string(ExperienceColumns)+" FROM spirit_shroud_experience_state WHERE session_id="+S(destination_session)+" AND settled_at IS NULL FOR UPDATE");
   if(pending.RowCount()!=1)throw std::runtime_error("shroud resurrection pending experience missing");credited=ReadExperience(pending.begin());
  }else{
   normal_profile=true;credited.normal=GetEXP();credited.aa=GetAAXP();credited.aa_points=GetAAPoints();credited.aa_spent=GetSpentAA();
   credited.level=GetLevel();credited.level2=m_pp.level2;credited.skill_points=m_pp.points;credited.percent_to_aa=GetLevel()>50?std::min<uint32_t>(100,GetAAPercent()):0;credited.aa_unit=GetRequiredAAExperience();
   auto maximum=RuleI(Character,MaxExpLevel);if(maximum<1)maximum=RuleI(Character,MaxLevel);if(GetClientMaxLevel())maximum=GetClientMaxLevel();credited.max_level=std::max<int>(GetLevel(),maximum);
   if(!credited.max_level||credited.max_level>126)throw std::runtime_error("shroud resurrection level cap invalid");credited.thresholds.push_back(0);
   for(uint32_t i=1;i<=credited.max_level+1;++i)credited.thresholds.push_back(GetEXPForLevel(i));
  }
  const auto refund=SpiritShroud::ApplyResurrection(credited,applied,uint32_t(percent),RuleI(AA,UnusedAAPointCap));
  if(normal_profile)Q(db,"UPDATE character_data SET exp="+S(credited.normal)+",level="+S(credited.level)+",level2="+S(credited.level2)+",points="+S(credited.skill_points)+" WHERE id="+actor);
  else Q(db,"UPDATE spirit_shroud_experience_state SET normal_exp="+S(credited.normal)+",level="+S(credited.level)+",level2="+S(credited.level2)+",skill_points="+S(credited.skill_points)+" WHERE session_id="+S(destination_session)+" AND settled_at IS NULL");
  auto claim=Q(db,"UPDATE spirit_shroud_corpse_state SET refunded_loss="+S(refund)+",rez_claimed_at="+S(Now())+",rez_spell_id="+S(spell_id)+" WHERE corpse_id="+corpse+" AND rez_claimed_at IS NULL");if(claim.RowsAffected()!=1)throw std::runtime_error("shroud resurrection already claimed");
  Q(db,"UPDATE character_corpses SET exp=0,is_rezzed=1 WHERE id="+corpse+" AND charid="+actor);
  Q(db,"INSERT INTO spirit_shroud_experience_events(session_id,event_key,source,progression_delta,normal_delta,aa_delta,created_at,settled_at) VALUES("+S(destination_session)+","+ShroudCorpseEventKey(destination_session,corpse_id,2)+","+S(static_cast<uint32_t>(ExpSource::Resurrection))+",0,"+S(refund)+",0,"+S(Now())+","+(normal_profile?S(Now()):"NULL")+")");
  auto check=Q(db,"SELECT CONNECTION_ID(),@@in_transaction");if(N(check.begin()[0])!=connection||N(check.begin()[1])!=1)throw std::runtime_error("shroud resurrection connection changed");
  committing=true;auto result=db.QueryDatabase(std::string("COMMIT"),false);if(!result.Success()){m_spirit_shroud_transition=true;Disconnect();return false;}started=false;
  if(normal_profile){
   m_pp.exp=static_cast<uint32_t>(credited.normal);
   if(GetLevel()!=credited.level)SetLevel(static_cast<uint8_t>(credited.level),false);
   auto* update=new EQApplicationPacket(OP_ExpUpdate,sizeof(ExpUpdate_Struct));auto* value=reinterpret_cast<ExpUpdate_Struct*>(update->pBuffer);
   const auto bottom=GetEXPForLevel(GetLevel()),top=GetEXPForLevel(GetLevel()+1);
   value->exp=top>bottom?static_cast<uint32_t>(330.0*std::min<uint64_t>(top-bottom,m_pp.exp>=bottom?m_pp.exp-bottom:0)/(top-bottom)):0;QueuePacket(update);safe_delete(update);
  }
  return true;
 }catch(const std::exception& error){
  if(started&&db.GetStatus()==DBcore::Connected)db.QueryDatabase(std::string("ROLLBACK"),false);
  LogError("Spirit Shroud resurrection rejected for character [{}], corpse [{}]: [{}]",CharacterID(),corpse_id,error.what());
  if(committing){m_spirit_shroud_transition=true;Disconnect();}
  Message(Chat::Red,"Your resurrection could not be settled. The corpse claim has not been acknowledged.");return false;
 }
}
