# Known gaps and compatibility policies

This is modern Live content adapted to the May 10, 2013 RoF 2 client. Actual acceptance establishes the bounded cases in [status](STATUS.md), not complete modern Live or exact historical parity.

## Content, stats and progression

Only eight first branches /112 level variants are enabled. The other 26 branches /364 variants, special forms and Runed Gargoyle entitlement path remain incomplete. Family completion and later-branch unlock thresholds remain NULL/unmeasured. Observed first-branch availability does not imply completed families. Unmeasured combat fields and skills retain emulator policy rather than invented Live values; full starter consumable stacks remain incomplete.

All 47 captured paid AA groups /329 ranks are represented. Attack Speed purchases/persistence and first/max timing, Improved Plane Shift endpoint casts and Enduring Voice endpoint casts passed. Complete intermediate-rank, stacking and shared-cooldown matrices remain unverified. The old RoF 2 haste display can omit a server-applied shroud AA contribution. Warlord's Tenacity and selected free spells retain documented RoF 2/modern-Live semantic differences; ordinary spell definitions remain unchanged.

## XP, death and resurrection

Bounded solo/group/quest/100%-AA reward and exactly-once restoration cases passed, including saved ordinary peer rewards and allocation restoration. Other reward scaling, raid edges, caps, level-up event hooks, fellowship and evolving-item XP integration remain incomplete or unverified. These tests establish configured emulator formulas, not Live reward balance.

Same-session and older restored-session corpse loss/refund cases passed, including 90% resurrection and tested restart persistence. Other corpse combinations and failure boundaries remain unverified. The death formula is emulator policy, not a measured Live conversion. Temporary possessions remain with the active shroud on death as an explicit conservation policy; Live corpse inventory semantics are not established. Legacy/untrusted corpse rows cannot authorize a new refundable loss.

## Interactions

Earned-item bilateral trade, exact-price merchant sale and one inventory click with actual relog/durable persistence passed. Starter trade/sale negatives established client-visible restriction and custody; they do not identify the server rejection branch. A nine-stage personal-bank bag/child and two-way coin roundtrip suite passed, including starter-bag rejection. Earned-child insertion into an issued starter bag and blocked bag transfer also passed with custody intact. Other transfer/item-click forms, full-inventory and additional bank failure boundaries remain unverified. Bulk Ctrl bank transfers, shared bank, parcels/world drops/tribute/augmentation and tradeskills remain restricted where provenance or behavior is not established. Earned gear may be carried but cannot contribute as equipped shroud gear.

Deferred mercenary hire liability, insufficient funds, restart and upkeep-only subsequent resume passed with housing disabled. An existing paid contract also passed native level 70 → 5 → 70 scaling and automatic suspension with no additional charges, unchanged paid marker and conserved original custody. Insufficient funds never prevent original-profile restoration: the affected contract stays suspended with durable debt until normal resume can pay. Existing contracts do not acquire another hire fee. This is a compatibility policy; exact Live insufficient-funds and HP-rescaling responses remain unmeasured.

Original pet checkpoint/escrow and one correctly owned saved-pet restoration passed through reload fallback. Charmed/engaged pets block entry; pet/charm form powers are not granted without evidence. In-place pet-actor reconstruction is not implemented. Bank expiry deletion remains disabled because lifetime/deletion semantics are unverified.

## Native transformation and recovery

`Character:NativeSpiritShroudTransitions` defaults false. With it enabled, supported profiles use precommit packet/cache preparation and committed-state installation. Legacy/unversioned forms, saved destination pets, charm/illusion/mount buffs, hero-forge overrides and unsupported item/slot states deliberately use reload fallback. Failed queries, stale identity and malformed state abort before commit instead of silently selecting fallback. Completion 0x 7786 has no embedded revision; bounded pending state associates it with the current transition.

Actual coverage includes 40 self-client cases /80  directions /280 checks, Imp 5/Bear 70 observer directions and bounded two-client group/raid continuity. Raid combat, loot, task credit and leadership bonuses remain unverified. All enabled combinations, level/rank boundaries and ordinary zoning after every purchase are outside complete coverage.

Real process deaths before and after entry commit and before and after restoration commit passed committed-profile recovery, durable-domain comparison and actual native reconnect. A final repeated removal/relog preserved a measured reward exactly once. A separate injected loader-failure rollback passed. This does not cover every database failure, packet loss, ACK timeout or reconnect timing.

Plain long 288/short 6886 buffs passed both native and saved-pet reload restoration, including client HP/effect slots and conserved original custody. Shroud-only Song capture and exact-latest-restoration loading leave ordinary SaveBuffs policy unchanged. One ordinary AA 487 cooldown escrow/return passed; complete Live buff/cooldown suspension semantics remain unmeasured, and absolute cooldown timestamps continue to age.

Existing RoF 2 OP_Buff 0x 659c operation 3 creates restored slots; operation 2 updates existing slots. Opaque per-effect slot_data remains unrecovered. Nonzero saved counter/rune states retain native reload fallback. Although the scoped short loader preserves server counter/rune fields, complex short-buff counter/rune client representation remains unproven; plain 288/6886 tests do not establish it.

Native recovery-bank plain-item Get, inspection, expiry denial and row-removal acknowledgement passed without reload, including a nine-charge earned click item after restoration. Bag/child retrieval, full inventory and interrupted Get commit cases remain unverified; unsupported reconstruction retains reload fallback.

## Combat and deployment

Exact current-session starter weapons bypass only monster-race equipment masks during damage eligibility; normal/acquired gear, class, level and immunity checks retain their paths. Actual corrected-build combat passed 19 bounded landed cases across eight 70 forms and six classes. Full balance, taunt/hate, avoidance/mitigation, positional mechanics and charm/pet combat remain unverified.

Housing-disabled empty-inventory login passed for main and genuine-empty peer on the final integrated build. This does not enable housing gameplay or alter multi-account login rules. Full integrated world/zone builds passed; clean pinned apply/identity and changed-translation-unit checks remain distinct from a full clean-source link, which was not run.
