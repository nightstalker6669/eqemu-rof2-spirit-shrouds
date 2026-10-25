# EQEmu RoF2 Spirit Shrouds

Spirit Shroud source extension for the **May 10, 2013 Rain of Fear 2 client**, using observed modern Live content adapted to that client. Published October 24, 2026. This is a partial implementation with a verified content and restoration baseline; full Live parity is not claimed.

The current catalog enables **112 combinations: eight forms at levels 5, 10, 15, …, 70**. It includes native selection and previews, temporary profiles, original-profile escrow and restoration, earned-item retrieval, Monster Point allocation, 311 paid AA ranks and deferred positive original-character XP. Entering and removing a shroud currently reloads the zone.

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

The other 26 standard branches have disabled preview records. Runed Gargoyle, complete family unlock progression, three AA groups, death/resurrection settlement and native transformation without zoning remain unfinished. See [known gaps](docs/KNOWN-GAPS.md) before deployment. Ordinary enabled forms have no GM-only gate.

## Opcodes and structures

The package documents **15 mapped Spirit Shroud messages** for the May 10, 2013 RoF2 client:

| Message | Opcode | Direction | Current use |
| --- | --- | --- | --- |
| Selection | 0x2ec3 | S→C | Keeper and delimited selection tree |
| Request | 0x2b37 | C→S | Selected template details request |
| Details | 0x6c04 | S→C | Template header and caret-delimited preview |
| Select | 0x1cd0 | C→S | Confirm selected template |
| Cancel | 0x79da | C→S | Close/reset keeper menu |
| ProgressAppend | 0x3823 | S→C | Native progression message family; full threshold behavior unfinished |
| ProgressReplace | 0x541d | S→C | Progress display refresh; percentage remains provisional |
| AbilityRequest | 0x3acc | C→S | Ability-preview cache request |
| AbilityReady | 0x006e | S→C | Ability-preview ready signal |
| EquipmentRequest | 0x4b25 | C→S | Equipment cache request |
| Equipment | 0x2507 | S→C | Native item preview |
| Transform | 0x6562 | S→C | Recovered mapping; native in-place flow not implemented |
| Bank | 0x7b71 | Both | Bank action envelope |
| BankItems | 0x1115 | S→C | Available earned items |
| BankEmpty | 0x0728 | S→C | Empty-bank notification |

- [Opcode inventory, directions and implementation status](protocol/OPCODES.md)
- [Packet structures, field layouts and native transform envelope](protocol/STRUCTS.md)
- [Machine-readable opcode mappings](protocol/opcodes.json)
- [Recovery evidence and compatibility limits](protocol/EVIDENCE.md)
- Production serializers: [wire.h](overlay/common/shroud/wire.h), [native_stats.h](overlay/common/shroud/native_stats.h) and [aa_preview.h](overlay/common/shroud/aa_preview.h)

The `0x6562` transformation mapping is recovered, but its native in-place flow remains unfinished. The empty `0x7786` completion acknowledgement is documented as a candidate and is not mapped or handled by this release. Monster AA operations reuse the existing AA packet families. A mapping alone does not establish complete gameplay support.

## Exact source dependencies

Apply in order:

1. EQEmu upstream `8175ae61874cbf20d7b05102821f6f8a4f4d1c39`.
2. [Housing](https://github.com/nightstalker6669/eqemu-rof2-housing) `9aad4cc0857bdd14677dd45ab24f3eafe20d489b`.
3. [Fellowship](https://github.com/nightstalker6669/eqemu-rof2-fellowship) `9382453264f4f84ac7e83a9ee5c9c9a1391c6017`.
4. [Mercenaries](https://github.com/nightstalker6669/eqemu-rof2-mercenaries) `00b07c381a738a21aff08867bf186e6c2bfaffb0`.
5. This extension.

These dependencies describe the integrated source baseline and reused inventory ownership safeguards. Housing ownership and housing use are not shroud prerequisites. The companion code is supplied by those repositories rather than copied into this package.

## Installation

Read [INSTALL.md](docs/INSTALL.md), then run against the exact combined baseline:

```sh
python3 tools/apply_overlay.py /path/to/EQEmu --check
python3 tools/apply_overlay.py /path/to/EQEmu --apply
```

The guarded installer changes source only. SQL, builds, opcode configuration and service deployment are separate operator steps. The package excludes the provisional three-form starter catalog; retained runtime compatibility still permits existing legacy sessions to recover.

Native inspection/restoration checks cover 33 distinct enabled combinations, including all six classes. One actual Monster Point purchase survived a full client relog and restored the original AA profile. These checks do not establish combat/death acceptance or all-form parity. See [status](docs/STATUS.md).

| Path | Contents |
| --- | --- |
| `overlay/` | New production sources and additive SQL migrations |
| `patches/` | Integration patch and exact prerequisite source hashes |
| `protocol/` | Opcode directions, layouts and recovery limits |
| `docs/` | Install, architecture, gameplay, validation and gaps |
| `tools/apply_overlay.py` | Guarded source installer |
| `source-manifest.json` | Package inventory and SHA-256 hashes |

Production source, SQL, installation tooling and documentation only. No test suites, fixtures, research scripts, captures, logs, database dumps, credentials, client assets, binaries or personal runtime configuration are distributed. GPLv3: see [LICENSE](LICENSE) and [NOTICE](NOTICE).
