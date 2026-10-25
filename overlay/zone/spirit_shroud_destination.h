#pragma once
#include "../common/eq_packet_structs.h"
#include "../common/extprofile.h"
#include "../common/inventory_profile.h"
#include "../common/ptimer.h"
#include "../common/shroud/wire.h"
#include "common.h"
#include <memory>
#include <unordered_map>
#include <set>
#include <vector>

// Prepared exclusively from the destination transaction before commit. All
// native bytes exist before any irreversible profile swap. Destruction owns
// only its own inventory/timer allocations; no Client/lease is constructed.
struct SpiritShroudDestination {
 PlayerProfile_Struct profile{};
 ExtendedProfile_Struct extended{};
 EQ::InventoryProfile inventory;
 PTimerList timers;
 std::vector<Buffs_Struct> buffs;
 std::unordered_map<uint32,std::pair<uint32,uint32>> aa_ranks;
 std::shared_ptr<const SpiritShroud::Template> definition;
 std::set<uint32_t> allowed_ranks;
 uint64_t revision=0,session_id=0;
 uint32_t template_id=0;
 bool personal_bank=false;
 float model_size=0;
 std::vector<uint8_t> self_bytes,observer_bytes;
};
