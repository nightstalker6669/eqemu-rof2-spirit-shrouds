# Structures and serialization

Scalar builders in `common/shroud/wire.h` write little-endian bytes; text is NUL-terminated. Preview tokens are escaped against delimiters. Selection carries keeper spawn ID, two zero DWORDs and the selection tree. Details carry template ID, DWORD1 and the caret-delimited header/resources/attributes/abilities/equipment. Equipment preview uses the ordinary RoF2 serialized ItemPtr without an ItemPacket-type DWORD. Source builders are the authoritative variable-length schema; host C++ padding is not a wire layout.

## Native transformation

`0x6562` begins with DWORD spawn ID and WORD profile offset, followed by the normally encoded single-spawn body. The offset equals six bytes plus that body length. The observer path contains this spawn envelope only. The self path appends a compact **0x4ff8-byte profile**, then DWORD inventory root count and native serialized inventory. It adds no ItemPacket-type DWORD.

The serializer captures the existing normal RoF2 profile encoder and translates its counted fields through the recovered compact layout. Armor records swap the two recovered middle words; bandolier/potion names use bounded native buffers; timers preserve their proved units. Compact AA ownership uses highest owned rank ID, cumulative cost and charges, while server persistence retains ordinary local rank counts.

The compact affect area contains only the proved empty-record representation. The installed backend buffs are reconstructed from authoritative saved fields. Supported simple affects are replayed through existing OP_Buff0x659c with explicit operation3 (slot creation), then the buff-icon list. Ordinary operation2 updates cannot populate empty slots; operation1 retains fade semantics. Duration, hit count, caster and cast coordinates use known fields. Nonzero counter/rune state requires unrecovered per-effect slot_data and takes reload fallback rather than scalar-only replay. Unknown active-affect bytes are not guessed. The matching client returns an empty **C→S 0x7786** after applying the transform.

Preparation runs before commit, on the destination database transaction. It must produce all self/observer bytes and owned caches first. Unsupported item/appearance/pet states choose the reload fallback. Postcommit cache installation preserves entity identity, group, raid, target, position and the inventory writer lease. Packet handling and saves stay fenced until a valid pending completion; a failure after commit recovers from committed storage, rather than guessing which profile won.

This native path defaults off. The documented self/observer/group/raid cases passed, and long-buff288 replay passed, while the new short-buff checkpoint correction and remaining failure boundaries still require physical acceptance. Supported bank retrieval uses the separate item/cache preparation and native completion below.


## Bank retrieval completion

The native Bank action2 completion is16bytes: action u32=2 at+0, reserved u32=0 at+4, original item key u64 at+8. The matching reader at0x61dacb removes the matching bank row and decrements the outstanding Get counter. An all-ones u64 key skips removal while completing a failed request. A successful item is delivered separately through ordinary ItemPacketTrade before this completion. The supported service path prepares item wire/cache before transaction commit and avoids a zone reload; unsupported reconstruction falls back to the existing reload path. The plain-item native path passed actual item 1001 inspection, row-removal acknowledgement without reload and custody conservation; expired-Get denial preserved the row/item and completed the wait. Bag/child and remaining failure cases remain unverified.

Reload restoration of validated Song rows also needs explicit operation3 scalar replay after the login profile; a private one-shot flag queues it before icons. This scoped path leaves ordinary saves unchanged. Source/ordering checks pass; plain native288/6886 cases passed; the separate saved-pet reload-fallback plain long/short-buff case also passed.

The scoped short-buff loader preserves server counter/rune fields, but the RoF2 operation3 opaque per-effect slot_data remains unrecovered. Complex short-buff counter/rune client representation is unproven; the passed plain spell288/6886 cases do not establish it. Native counter/rune fallback and ordinary SaveBuffs policy remain unchanged.
