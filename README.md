# EQEmu RoF2 Spirit Shrouds

Production source extension for the **May 10, 2013 Rain of Fear 2 client**, adapting observed modern Live content. Full Live parity is not claimed. This update includes migrations **013–015**, corpse-domain XP accounting, system interaction safeguards and an optional native transformation path. Completed native checks and remaining physical acceptance limits are described in [STATUS.md](docs/STATUS.md).

The catalog enables **112 combinations: eight first-branch forms at levels 5, 10, 15, …, 70**, covering all six underlying class types. It contains native selection/previews, profile escrow/restoration, earned-item retrieval, Monster Point purchases, **47 paid AA groups / 329 ranks**, and durable original-character XP/AA accounting. Enabled forms have no GM-only gate.

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

The other 26 standard branches (364 level variants) remain disabled. Exact family completion and later-branch unlock thresholds are explicitly **unknown**, stored as NULL rather than invented totals. Runed Gargoyle entitlement/content remains incomplete.

## Opcodes and structures

The package maps **16 Spirit Shroud messages** for the matching RoF2 client:

| Message | Opcode | Direction | Use |
| --- | --- | --- | --- |
| Selection | 0x2ec3 | S→C | Keeper and delimited selection tree |
| Request | 0x2b37 | C→S | Selected template details |
| Details | 0x6c04 | S→C | Template header and preview |
| Select | 0x1cd0 | C→S | Confirm selected template |
| Cancel | 0x79da | C→S | Close/reset menu |
| ProgressAppend | 0x3823 | S→C | Native progression message family |
| ProgressReplace | 0x541d | S→C | Evidence-gated progress refresh |
| AbilityRequest | 0x3acc | C→S | Ability preview request |
| AbilityReady | 0x006e | S→C | Ability preview readiness |
| EquipmentRequest | 0x4b25 | C→S | Equipment preview request |
| Equipment | 0x2507 | S→C | Native serialized item preview |
| Transform | 0x6562 | S→C | Prepared in-place profile/spawn/inventory |
| Complete | 0x7786 | C→S | Empty native completion acknowledgement |
| Bank | 0x7b71 | Both | Bank action envelope |
| BankItems | 0x1115 | S→C | Earned-item list |
| BankEmpty | 0x0728 | S→C | Empty-bank notification |

The existing shared **OP_Buff (`0x659c`, S→C)** also participates in native restoration; it is not a new shroud opcode. Its operation DWORD is **3** to recreate an empty native affect slot, **2** for ordinary duration updates and **1** for fade. Native counter/rune per-effect slot data remain unrecovered, so those saved states take reload fallback. The corrected long-buff288 restoration passed natively without reload, with client maxHP3348. The separate short-buff6886 transactional checkpoint correction is included; plain long288/short6886 native restoration now passed without reload; the separate saved-pet reload-fallback plain long/short-buff case also passed.

- [Opcode inventory and directions](protocol/OPCODES.md)
- [Packet structures and native transformation envelope](protocol/STRUCTS.md)
- [Machine-readable opcode mappings](protocol/opcodes.json)
- [Evidence and compatibility limits](protocol/EVIDENCE.md)
- Production serializers: [wire.h](overlay/common/shroud/wire.h), [native_transform.h](overlay/common/shroud/native_transform.h) and [native_stats.h](overlay/common/shroud/native_stats.h)

`Character:NativeSpiritShroudTransitions` defaults to **false**. With the rule enabled, the service prepares complete destination packets/caches before commit, exchanges caches behind a write fence, sends `0x6562`, and waits for empty `0x7786`. Unsupported states use the retained zone-reload fallback. The actual RoF2/server native batch passed 40 form/level cases and 80 entry/removal directions, covering transition revisions 139–218 and 280 recorded checks. A subsequent two-client pass verified Imp5/Bear70 observer transforms in both directions, and bounded group/raid continuity checks passed. The four tested commit-boundary process crashes and plain long/short buff restoration also passed; wider failure timing and complex counter/rune states remain unverified.

## Exact source dependencies

Apply in order:

1. EQEmu upstream `8175ae61874cbf20d7b05102821f6f8a4f4d1c39`.
2. [Housing](https://github.com/nightstalker6669/eqemu-rof2-housing) `9aad4cc0857bdd14677dd45ab24f3eafe20d489b`.
3. [Fellowship](https://github.com/nightstalker6669/eqemu-rof2-fellowship) `9382453264f4f84ac7e83a9ee5c9c9a1391c6017`.
4. [Mercenaries](https://github.com/nightstalker6669/eqemu-rof2-mercenaries) `00b07c381a738a21aff08867bf186e6c2bfaffb0`.
5. This extension.

The companion repositories supply the integrated source baseline and shared inventory ownership safeguards. Housing ownership and housing use are not shroud prerequisites.

## Installation

Read [INSTALL.md](docs/INSTALL.md), then run against the exact combined baseline:

```sh
python3 tools/apply_overlay.py /path/to/EQEmu --check
python3 tools/apply_overlay.py /path/to/EQEmu --apply
```

The guarded installer changes source only. SQL, builds, opcode configuration and service deployment are separate operator steps. The provisional starter catalog is excluded; runtime compatibility preserves existing legacy-session recovery.

Actual client/server acceptance passed 40 native self-client cases / 80 directions, separate observer/group/raid continuity, 19 landed combat cases across eight forms and six classes, selected paid-AA purchases and endpoint timers, solo/group/quest/100%-AA settlement, death/resurrection/restart accounting, and mercenary deferred-fee/restart cases. Earned-item trade/return, an exact-price sale, item-click charge/effect persistence through relog, native recovery-bank retrieval and guarded conservation cleanup also passed. Original plain long/short buffs passed native and supported saved-pet reload paths; one original AA cooldown passed escrow/return. Four real process-crash boundaries around entry/restoration commit plus a repeated recovery check passed committed-state recovery and exactly-once reward settlement. These are bounded cases: later branches, unknown thresholds, complex counter/rune representation, full combat/AA/bank matrices and wider failure timings remain incomplete. The native rule stays default off. See [status](docs/STATUS.md) and [known gaps](docs/KNOWN-GAPS.md).

| Path | Contents |
| --- | --- |
| `overlay/` | New production sources and additive SQL migrations |
| `patches/` | Integration patch and exact prerequisite hashes |
| `protocol/` | Opcodes, layouts and recovery limits |
| `docs/` | Install, architecture, gameplay, validation and gaps |
| `tools/apply_overlay.py` | Guarded source installer |
| `source-manifest.json` | Package inventory and SHA-256 hashes |

Production source, SQL, installation tooling and documentation only. No tests, fixtures, research helpers, captures, logs, database dumps, credentials, client assets, binaries or private runtime configuration are distributed. GPLv3: [LICENSE](LICENSE), [NOTICE](NOTICE).
