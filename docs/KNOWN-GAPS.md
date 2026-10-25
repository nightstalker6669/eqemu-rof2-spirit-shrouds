# Known gaps

**This release is incomplete. Death and resurrection are not implemented as Live
shroud gameplay, and a known hover resurrection defect can destroy a corpse's
recoverable XP claim without applying the credit.** A proposed repair has not been incorporated into the released implementation.

## Death and resurrection

The temporary profile currently ignores XP decreases. Death therefore applies no
shroud XP debit, and a shrouded corpse records zero refundable XP. Temporary
possessions remain with the player rather than entering corpse custody. These
safeguards prevent an unapplied loss from becoming an XP refund after restoration;
they do not implement the reported Live loss and resurrection rules.

The ordinary resurrection packet handler and central resurrection function refuse
claims while shrouded. The same-zone hover-respawn caller nevertheless changes
health/location, clears hover, and completes the corpse after the central refusal.
Completing the corpse clears its refundable XP. An offer involving an original
character's pre-entry corpse can therefore be consumed without restoring that XP.

A complete solution needs durable corpse/session provenance, an actual recorded
debit, a bounded refund claimed exactly once, and atomic corpse completion with
ledger settlement. It must distinguish pre-entry corpses from current and previous
shroud-session corpses. Current Live loss amounts, their level/curve basis and
cross-profile resurrection rules have not been established. Do not enable those
paths by deleting the existing refusal guards alone.

## Incomplete forms and progression

Only the first branch in each of eight families is enabled, across levels 5–70.
The other 26 standard branches and their 364 preview combinations remain disabled.
Special forms and their entitlement paths are incomplete. Preview data and shared
classes are insufficient evidence for assigning active equipment or free ranks.

Family progress is stored separately, but exact later-branch unlock thresholds
and Live progression units are unknown. The current progress percentage uses a
provisional fixed conversion; it must not be interpreted as verified Live
completion. Legacy starter progress has not been converted into completed Live
families.

## Withheld paid groups

| Group | Reason |
| --- | --- |
| Attack Speed | The Live/client haste cache and effective combat/stacking interpretation remain unresolved. A displayed haste change does not prove the advertised combat effect. |
| Enduring Voice | Its AA cooldown-reduction effect uses an unresolved target namespace and interpretation. |
| Improved Plane Shift | Its AA cooldown-reduction targets cannot safely be inferred from the captured identifiers. |

These groups are unavailable even though their definitions were observed.
Other captured paid chains are present, but every rank and interaction has not
received native combat acceptance.

## Spell compatibility exceptions

Warlord's Tenacity uses the existing RoF2 spell definitions. They retain the older
negative HP-per-tick decay and self-range value that differ from the observed
modern Live payload. The client and ordinary spell database are not rewritten.
This deliberate compatibility choice is not a claim of exact modern spell parity;
actual cast behavior still needs acceptance.

Several previously captured free spells also retain existing RoF2 effect
interpretations where modern Live payloads differ, including Hamstring, Swarm of
Pain and Earth Bind. Matching spell IDs and costs alone do not establish identical
effects across client generations.

## Experience and level-up integration

Positive original XP/AA rewards settle transactionally when the shroud is removed.
The temporary form stays at its selected level. However, current Live conversion,
quest reward scaling, death adjustments, group/raid edge cases and cap behavior
have not been fully measured.

The shroud reward path bypasses some ordinary XP processing, including fellowship
and evolving-item integration and quest/event hooks. Direct settlement changes
stored levels without running every ordinary level-up event path. Original buffs
and cooldown rows are preserved, but full suspension/resumption semantics remain
unverified; absolute cooldown timestamps continue to age.

## Restricted systems and inventory

Current restrictions are broader than documented Live behavior. Active pets and
mercenaries must be removed or suspended before entry. Mercenary controls are
blocked while shrouded; automatic suspension, allowed shrouded mercenary use,
scaling and contract repricing have not been completed.

Bank transfers, shared-bank access, trading, merchant selling, parcel sending,
crafting, augments, tribute and other ownership-changing operations are restricted.
Normal characters retain their existing paths, but those restrictions should not
be described as complete Live interaction parity.

Starter consumable stacks are omitted until stack-safe provenance exists. Stable
starter tokens and earned children inside starter bags are handled, while the
full inventory/currency overflow and failure matrix remains incomplete. Shroud
bank retrieval uses a reload, and Live-style expiry is not implemented. Existing
bank contents are not automatically deleted by an assumed timer.

## Transformation and verification

Entry, removal and bank retrieval currently rely on reloads. The recovered native
in-place transformation flow, observer updates and complete packet/cache ordering
are unfinished.

Native checks cover 33 distinct form/level combinations, not all 112 enabled
combinations. One purchase canary demonstrated relog persistence; ordinary zoning
after purchases has not been verified. The complete combat, active-ability,
group/raid, pet, mercenary, crash/restart and database-failure suites remain open.
Passing builds, SQL validation or native inspection alone does not close them.
