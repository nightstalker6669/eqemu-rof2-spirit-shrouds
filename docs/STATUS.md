# Status

This production source extension implements Spirit Shrouds for the May 10, 2013 RoF 2 client using captured modern Live content and explicit emulator compatibility policies. Actual client/server acceptance covers the cases below. Complete modern Live or exact historical 2013 parity is not claimed.

## Enabled content and behavior

The catalog enables 112 form/level combinations: eight first branches at levels 5–70 in five-level steps, with no GM-only gate.

| Family | Enabled form | Class |
| --- | --- | --- |
| Aberrations | Imp Wizard | Wizard |
| Animals | Bear Beast | Warrior |
| Elementals | Earth Elemental Fighter | Warrior |
| Goblinoid | Goblin Rogue | Rogue |
| Humanoid | Kobold Cleric | Cleric |
| Nature Spirits | Fairy Trickster | Enchanter |
| Reptiles | Basilisk Beast | Berserker |
| Undead | Skeleton Wizard | Wizard |

The other 26 standard branches / 364 variants and special-form entitlement coverage remain incomplete. Paid Monster content contains 47 captured groups / 329 ranks; free grants and availability remain specific to each observed form/level. Unknown family completion and branch unlock thresholds remain NULL.

Implemented behavior includes native selection and previews; resource/attribute/model targets; Monster AA ownership, purchases and allocation persistence; transactional original-profile escrow; temporary-item provenance; earned-item recovery; original XP/AA and death/resurrection ledgers; personal banking; guarded earned-item trade/sales/clicks; mercenary suspension and deferred hire liabilities; and optional native entry/restoration. `Character:NativeSpiritShroudTransitions` defaults false and retains supported reload fallback.

## Actual client/server acceptance

### Transformation, recovery and custody

- A self-client native batch passed 40 form/level cases, 80 entry/removal directions and 280 checks. A separate observer pass verified Imp 5 and Bear 70 in both directions, preserving entity identity, XY, floor contact and original appearance/height on return. Raw Z varies with model height.
- Two-client group cases at selected levels 65 and 70 preserved membership, updated class/level in both native caches and shared real kill rewards. Two-client raid cases for Imp 5 and Bear 70 preserved roles, both native caches, database class/level, observer identity and original custody. Raid combat, loot, tasks and leadership bonuses were not tested.
- Four real zone-process interruption cases passed: entering before commit, entering after commit, restoring before commit and restoring after commit. Each used a distinct confirmed process-death/ownership boundary, durable-domain comparisons and actual native reconnect. Precommit cases retained the prior committed profile; postcommit cases recovered the new committed profile. Seventeen durable domains, retained history and pending rewards were checked. A final repeated removal/relog passed exactly-once settlement after one measured reward of 327 normal XP, 360 AA XP and 520 progression XP. This bounded matrix does not cover every database, packet-loss or acknowledgement-timeout failure.
- A separately injected skills-loader failure passed safe precommit abort, unchanged revision/durable domains and subsequent normal-profile camp/server-restart/relog conservation.
- Ordinary AA 487 cooldown escrow/return passed through native Imp 5 entry, dwell and restoration, preserving saved start/duration. Original summoned-pet checkpoint/escrow and restoration of exactly one owned saved pet passed through supported reload fallback; in-place pet-actor reconstruction is not claimed.
- Plain long spell 288 and short spell 6886 passed native entry/restoration without reload, with native Buff/Song slots, client HP/maxHP 3348 and original custody conserved. Both also passed the saved-pet reload fallback. Shrouded time was excluded from their saved durations. Existing OP_Buff 0x 659c operation 3 creates restored slots; ordinary update/fade behavior and ordinary SaveBuffs policy remain unchanged. Complex counter/rune client representation remains unproven.

### Abilities and combat

- Native inspection/entry/restoration previously covered 33 distinct combinations across all six classes, including a paid purchase surviving relog/restoration. Subsequent combat passed 19 landed offensive/melee/heal/root/mez cases across eight level-70 forms and all six underlying classes, with original custody conserved after every restoration. These bounded cases do not establish full Live balance, hate/taunt, mitigation, avoidance, positional mechanics or charm behavior.
- Attack Speed all eight ranks were purchased for 56 Monster Points; rank 1 survived relog. Runtime haste/timer endpoints were 105/3333 and 140/2500. Improved Plane Shift five-rank purchases and four endpoint cast/timer cases passed: first Ethereal Plane 1680 seconds and all three maximum-rank roots 1200 seconds. Enduring Voice five-rank interleaved purchases with its prerequisite cost 28 combined points; first/max cast/timer endpoints 870/750 seconds passed with a 900-second storage horizon. Restoration conserved ordinary AA and custody. Complete intermediate timing, stacking and shared-cooldown coverage remains unverified.

### XP, death and resurrection

- A solo kill produced independently expected 702292 normal/progression XP with fixed selected level 70. An acknowledged quest reward passed its independent 520-XP expectation and restoration with housing gameplay disabled.
- Group 65/group 70 kills each produced expected 625039 normal/progression XP; the ordinary peer's 625039 XP was also verified saved after camping in both cases. These establish configured emulator calculations, not Live numeric reward parity.
- A 100%-AA kill produced 26335966 AA XP, converting to 12 points plus 2624332 remainder, with zero normal XP and 14045848 progression XP. Restoration, repeated removal and relog retained the result exactly once; original 37% allocation was restored and saved.
- Imp 70 death applied 2613332 XP loss; native 90% resurrection refunded 2351998 exactly once, closed hover and restored living HP. Normal restoration retained the expected net 261334 loss. An older restored-session corpse refunded into another active session exactly once. Tested loss/refund cases also passed restart persistence and item/currency/ordinary-AA conservation. Other corpse combinations and Live death-conversion policy remain unmeasured.

### Banking, earned items and mercenaries

- Native recovery-bank opening, inspection and plain-item Get passed with row-removal acknowledgement and no reload. An expired Get preserved row/item and released the pending native wait. The recovered item appeared exactly once with its instance payload preserved apart from ephemeral GUID.
- A nine-stage personal-bank suite passed bag withdrawal/deposit with children, individual bank-bag child transfers, coin pickup/deposit in both directions and starter-bag rejection, conserving item trees and coins. A separate earned-child-in-issued-starter-bag case passed insertion and blocked bag transfer with custody intact.
- A tagged earned cap passed bilateral native trade and return, then an exact 189-copper merchant quote/sale. Starter-bag trade and sale attempts were blocked with inventory and balances unchanged; these prove client-visible restriction and custody, not execution of a specific server rejection branch.
- An acquired inventory click consumed one charge from 10 to 9 and granted spell 86. Native return after an actual relog and durable item/effect persistence passed. Restoration settled the sale credit and conserved the original profile. The remaining click item passed native shroud-bank recovery and ordinary-bank deposit. Guarded cleanup removed only the tagged fixture and exact 189-copper test proceeds.
- Mercenary fiscal checks passed with housing disabled: normal 70 quote 18100 copper; a new Imp 5 contract deferred its additional original-level hire liability. Restoration suspended it; insufficient funds charged nothing; funded resume charged 36200 hire-liability-plus-upkeep. After restart, native level 70 and custody passed; the next resume charged upkeep only 18100 without repeating liability. Exact Live insufficient-funds and HP-rescaling behavior remains unmeasured. An existing paid contract also passed native level 70 → 5 → 70 scaling, automatic entry/return suspension, zero additional charges and unchanged paid-liability marker, with original custody, XP/AA and allocation conserved. Final suspension was verified independently.
- Version-safe inventory loading passed actual housing-disabled main and genuinely empty peer logins. This does not enable housing gameplay or change account login rules.

Temporary acceptance fixtures, funds and configuration changes were reconciled through guarded cleanup. Research and acceptance artifacts remain outside this package.

## Build and isolated verification

The final integrated source passed full world/zone builds and deployment for the actual acceptance cases. All 26 changed C++ translation units previously passed compilation against clean pinned overlay source, and offline CMake world/zone object graphs agreed with the integrated build. A separate full clean-source link was not run. Fresh pinned companion reconstruction, overlay check/apply, privacy checks and installed-source identity are checked separately for this package.

Isolated coverage also includes compact-profile bounds, sanitized ownership swaps, inventory/coin accounting, fee rollback/retry, content/migration structure and namespace guards. Builds and isolated checks are not substituted for gameplay proof.

## Remaining limits

Later branches/special entitlements, unknown progression thresholds, complete combat/rank/stacking matrices, remaining personal-bank/full-inventory boundaries, complex counter/rune client representation, additional corpse/reward boundaries and the wider disconnect/database/ACK-timeout matrix remain incomplete or unverified. See [known gaps](KNOWN-GAPS.md), [architecture](ARCHITECTURE.md) and [gameplay](GAMEPLAY.md).
