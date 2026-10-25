# Architecture

Spirit Shrouds exchange a character's active profile while keeping the original
profile in transactional storage. The system targets RoF2 and uses modern Live
content observations where available. Several gameplay policies remain partial;
[Known gaps](KNOWN-GAPS.md) is part of the implementation contract.

## Catalog and retained definitions

The catalog separates progression family, branch, selected level and versioned
form definition. Enabled definitions provide the form identity, captured resource
and attribute targets, equipment, Monster Point budget and available AA ranks.
Disabled preview records remain outside the playable catalog.

A session retains its exact form definition. Catalog upgrades must not reinterpret
an active legacy session as a newer form. Disabling new entry must leave removal
and recovery available. Selection previews and the active profile use the same
catalog data, subject to the documented emulator calculation policy.

The production catalog currently enables the eight first branches across fourteen
levels each. More preview records exist, but preview availability alone does not
prove an active loadout or a progression threshold.

## Transactional profile exchange

The transition service validates the keeper, character level, inventory cursor,
combat state, pets, suspended mercenaries and relevant ownership state before an
exchange. Profile, inventory, currency, spell, skill, AA, buff, pet and timer rows
are preserved in original-profile storage with a snapshot manifest.

State includes a session identity and monotonically advancing transition revision.
Database locks and ownership checks fence competing or stale writers. Once a
profile exchange commits, the old client instance is write-fenced while a
same-zone reload reconstructs server and client caches from committed storage.
An ambiguous commit outcome requires recovery from stored state.

The implementation uses the existing shared inventory-ownership safeguards where
configured. Owning a house or plot is not a shroud prerequisite. Existing housing
and mercenary infrastructure must still match the integration baseline.

## Stats and Monster AAs

Captured resource maxima are translated into the contributions needed by the
server and matching client calculations. Independently captured attributes remain
separate from equipment, purchased passive bonuses and spell effects. This is a
resource baseline, not evidence that displayed AC or attack equals an internal
combat value. Unmeasured combat fields and skills use emulator calculations.

Monster AAs use dedicated local definitions rather than borrowing ordinary class
AA identities. Some free powers use a single grant-only adapter to present an
observed higher native rank without inventing predecessor purchases. Paid chains
retain their actual captured rank costs, prerequisites, level gates and cumulative
effect vectors. Only the highest owned rank contributes its cumulative effects;
predecessor effects are not added again.

Purchases validate the selected session's availability, ownership, prerequisites,
rank order and Monster Point balance. Ownership and spending are stored together.
The ordinary character's AA balances, allocation settings and definitions remain
separate. Native saved-set loading remains subject to server purchase validation.

Three groups remain withheld because their haste or AA cooldown-target semantics
are unresolved. Warlord's Tenacity deliberately retains the existing RoF2 spell
behavior as a documented compatibility exception; it is not an exact modern Live
spell match.

## Inventory conservation

Starter equipment has issuance provenance and persistent session-bound tokens;
loaded item serial numbers alone are insufficient identity. Restoration removes
identified temporary starter items while preserving earned items. Earned children
inside a temporary starter container are promoted to independent bank roots.
Ambiguous provenance or insufficient capacity aborts restoration before commit.

Earned items move to shroud-bank storage and earned carried currency is combined
with the original wallet during restoration. Retrieval checks the destination and
moves ownership transactionally. Current retrieval uses a reload. Bank expiry is
not enabled; storage lifetime does not yet match the reported Live timer policy.

## Experience and recovery limits

Positive rewards update a durable original-character XP/AA ledger and a separate
family-progress record. Entry captures the original allocation, XP curve and caps.
Restoration settles the ledger and original profile in the same transaction as the
session's restored marker, preventing repeated removal from settling it twice.
The temporary form does not gain character levels.

The current positive ledger does not implement death debits or corpse-domain
refunds. Temporary possessions remain with the shrouded player on death, and
shrouded corpses advertise no refundable XP. Resurrection is provisionally refused
while shrouded, with a known hover-path defect described in
[Known gaps](KNOWN-GAPS.md). The corpse provenance schema is not a completed
loss/refund implementation.

Original buff and cooldown rows are retained, but their full Live suspension and
resumption semantics are not established. Direct settlement also bypasses some
ordinary XP/level event paths. Neither successful profile exchange nor migration
validation establishes those behaviors.
