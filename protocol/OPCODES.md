# RoF2 Spirit Shroud protocol

These mappings target the May 10, 2013 client; current Live numeric opcodes must not be substituted.

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

The empty completion acknowledgement is now mapped and handled. Its payload has no session/revision token. It is accepted only while the server is waiting for completion of the already committed destination revision; it never commits database state. A timeout or revision mismatch disconnects behind the write fence for authoritative reconnect recovery. Ordinary AA definitions, ownership, purchases, casts and timers use the existing AA packet families.

Local Monster adapter IDs must stay below the matching client's 49999 enumeration ceiling. Captured Live group/rank/string IDs and local adapters remain distinct. See [structures](STRUCTS.md), [evidence limits](EVIDENCE.md) and [status](../docs/STATUS.md).

Related existing message: OP_Buff0x659c S→C recreates saved simple native affect slots with operation3 after compact-profile installation. Existing ordinary operation2 updates and operation1 fades remain unchanged. This is an existing shared opcode, not a seventeenth newly recovered shroud message. Per-effect counter/rune slot_data remains unsupported for native replay and selects reload fallback. Long-buff288 native replay passed without reload; the separate short-buff6886 checkpoint correction awaits native/fallback acceptance.
