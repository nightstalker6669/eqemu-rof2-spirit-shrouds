# RoF2 Spirit Shroud protocol

Mappings target the May 10, 2013 client. Modern Live packet numbers must not be reused as RoF2 numbers. Source registration is in the integration patch; [wire.h](../overlay/common/shroud/wire.h) owns the shared scalar/text builders.

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

Monster AA definitions, ownership, purchases, activation and timers use the existing AA packet families. New local adapter IDs are distinct from captured Live group/rank IDs. Native RoF2 AA enumeration requires local IDs below 49999; the service remaps definitions into that range rather than modifying ordinary AA balances.

See [structures](STRUCTS.md) and [evidence limits](EVIDENCE.md). A mapped opcode alone does not mean its gameplay is implemented.
