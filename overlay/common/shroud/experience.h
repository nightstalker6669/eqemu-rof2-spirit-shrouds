#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace SpiritShroud {
// This is the original character's pending experience, never the temporary
// form's Monster Point balance. Thresholds are captured before transformation
// through GetEXPForLevel, including any configured quest override.
struct ExperienceState {
 uint64_t normal{}, aa{};
 uint32_t aa_points{}, aa_spent{}, level{}, level2{}, skill_points{}, percent_to_aa{}, aa_unit{}, max_level{};
 std::vector<uint32_t> thresholds;
};
inline uint64_t SaturatingAdd(uint64_t a, uint64_t b)
{
 return a + std::min(b, std::numeric_limits<uint64_t>::max()-a);
}
inline void ValidateExperience(const ExperienceState& state)
{
 if (!state.level || state.level>126 || state.level2>126 || state.max_level<state.level ||
     state.max_level>126 || state.thresholds.size()!=state.max_level+2 ||
     state.percent_to_aa>100 || !state.aa_unit || state.aa>=state.aa_unit)
  throw std::invalid_argument("invalid shroud experience state");
 for (size_t i=2;i<state.thresholds.size();++i)
  if (state.thresholds[i]<=state.thresholds[i-1] || state.thresholds[i]==UINT32_MAX)
   throw std::invalid_argument("invalid shroud experience thresholds");
 // Existing profiles can sit just below the current curve after historical
 // integer/float rounding or a configured curve change. Retain their recorded
 // level and exact balance; entering a form must not grant XP or delevel them.
 if (state.normal>UINT32_MAX)
  throw std::invalid_argument("invalid shroud original experience");
}
// Apply the configured character level and unused-AA caps without ever reducing
// a pre-existing over-cap balance. A retry starts from the locked durable row.
inline void ApplyExperience(ExperienceState& state, uint64_t normal, uint64_t aa, int unused_aa_cap)
{
 ValidateExperience(state);
 const uint64_t ceiling=std::max<uint64_t>(state.normal,state.thresholds[state.max_level+1]);
 state.normal=std::min(ceiling,SaturatingAdd(state.normal,normal));
 while (state.level<state.max_level && state.normal>=state.thresholds[state.level+1]) ++state.level;
 if (state.level>state.level2) {
  const uint64_t points=state.level2 ? 5ULL*(state.level-state.level2) : 5;
  state.skill_points=static_cast<uint32_t>(std::min<uint64_t>(UINT32_MAX,SaturatingAdd(state.skill_points,points)));
  state.level2=state.level;
 }
 if (state.level<=50) return;
 const uint32_t cap=unused_aa_cap<0 ? UINT32_MAX : static_cast<uint32_t>(unused_aa_cap);
 if (state.aa_points>=cap) return;
 const uint64_t total=SaturatingAdd(state.aa,aa);
 const uint64_t granted=std::min<uint64_t>(total/state.aa_unit,cap-state.aa_points);
 state.aa_points+=static_cast<uint32_t>(granted);
 state.aa=state.aa_points==cap ? 0 : total%state.aa_unit;
}
}
