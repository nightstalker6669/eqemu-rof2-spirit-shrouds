# Status

This is a partial Spirit Shroud implementation for the RoF2 client. It combines
modern Live observations with the emulator's combat and skill calculations. It
does not claim complete Live parity or an exact reconstruction of the 2013 game.

**Death, resurrection and branch progression are incomplete. A known hover
resurrection defect can consume a corpse's recoverable XP without crediting it.
The proposed repair has not been applied to this release.** See
[Known gaps](KNOWN-GAPS.md) before using the feature.

## Content

The enabled catalog contains 112 form/level combinations: the first branch in
each of eight families, at levels 5 through 70 in five-level steps.

| Family | Enabled branch | Underlying class |
| --- | --- | --- |
| Aberrations | Imp Wizard | Wizard |
| Animals | Bear Beast | Warrior |
| Elementals | Earth Elemental Fighter | Warrior |
| Goblinoid | Goblin Rogue | Rogue |
| Humanoid | Kobold Cleric | Cleric |
| Nature Spirits | Fairy Trickster | Enchanter |
| Reptiles | Basilisk Beast | Berserker |
| Undead | Skeleton Wizard | Wizard |

Another 26 standard branches, representing 364 preview combinations, remain
disabled. Their preview data does not establish complete active loadouts,
equipment, budgets or unlock requirements. Special-form coverage is incomplete.

The paid Monster AA catalog contains 44 ability groups and 311 ranks. Free powers
use separate form-specific grants. Three paid groups are deliberately withheld:
Attack Speed, Enduring Voice and Improved Plane Shift. Similar class names do not
imply interchangeable form loadouts.

## Implemented behavior

- Keeper selection and previews, with original-level limits and server-side
  validation. Enabled content has no GM-only access gate.
- Captured HP, mana, endurance, seven attributes and model size for the enabled
  combinations. Resource calculations reconcile the captured baseline with the
  RoF2 client; remaining combat and skill behavior uses emulator policy.
- Separate Monster Point spending, paid rank chains, prerequisites, free grants,
  passive bonuses and active-ability routing. Ordinary AA state is escrowed.
- Transactional original-profile preservation and restoration, stable starter-item
  provenance, earned-item banking and bank retrieval.
- Durable positive original-character XP/AA rewards and separate family progress,
  with settlement during restoration. The selected form level remains fixed.
- Same-zone reloads to enter and leave a form and to synchronize bank retrieval.
  In-place transformation is not the normal operational path.

## Verification boundaries

Native entry, inspection and restoration checks cover 33 distinct form/level
combinations. The two batches comprise 35 case executions, including two repeated
combinations. Checks cover the captured resource/attribute baseline, available AA
presentation and free ownership, starter equipment, and restoration conservation.
They do not constitute combat acceptance for all six underlying classes.

A paid-purchase canary verified spending and ownership persistence across a relog.
Purchase persistence across ordinary travel to another zone has not been proved.
The entry/removal reload checks do not establish that separate property.

Offline validation checked content structure, rank topology, effects,
prerequisites, level-eligible prefixes, migration repetition, namespace collision
refusal and preservation of ordinary AA, original-profile escrow and starter
state. These checks do not prove every rank's gameplay effect.

Unverified areas include full combat balance, every active cast and timer,
ordinary zoning after purchases, observers, all failure/restart boundaries,
group/raid XP parity, death losses, resurrection and later-branch unlocks.

Read [Architecture](ARCHITECTURE.md) for the state model and
[Gameplay](GAMEPLAY.md) for the current player flow.
