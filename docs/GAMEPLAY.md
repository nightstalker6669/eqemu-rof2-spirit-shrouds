# Gameplay

Enable the observed catalog without a GM gate. Select an enabled first branch at a five-level step from 5 through 70, no higher than the original character. Later branches with missing unlock data remain disabled. See [status](STATUS.md) and [limits](KNOWN-GAPS.md).

Leave combat/casting, release charmed pets, close trades and empty item/coin cursors. Hail a supported keeper, inspect the form, and select it. An active mercenary automatically suspends. A regular saved pet is preserved in original-profile escrow. With the default configuration, profile changes reload the zone; the optional native rule attempts an in-place exchange with a reload fallback for unsupported state.

The form has its own equipment, attributes and Monster Point budget. Open Alternate Advancement and clear **Can Purchase** to see free/fully trained abilities. Purchases are checked against the actual selected form, level, prerequisites and budget, not merely its ordinary class. Original AA balances/settings are separate. Full rank combat/timer acceptance is still pending.

The form's level stays fixed. Original XP/AA rewards, and supported corpse loss/refund adjustments, settle through the original-profile ledger. Progression families advance separately; unknown completion/unlock thresholds are not estimated. Specific Imp70 death/hover resurrection/restoration and older-session corpse refund cases passed; the complete death/rez suite, including other failure boundaries, remains pending; tested restart persistence passed.

Ordinary personal banking, usable acquired consumables/item clicks and earned-only trade/merchant sales are supported by the new source policy. Shared bank, tradeskills and several other outgoing ownership operations remain restricted. Starter item/container provenance prevents temporary equipment or any container carrying it from being traded, sold or banked. Earned gear may be carried but is refused as active worn equipment. Stat food/drink is evaluated separately. Mercenary controls are available under the normal lifecycle, active-level scaling and upkeep rules.

A mercenary first hired while shrouded owes the full original-level hire fee again. This release defers that fee until normal-profile unsuspend; insufficient funds keep the contract suspended and never prevent returning to the original character. This is a disclosed compatibility policy, not a claim about Live's exact insufficient-funds behavior.

Return to a keeper and say `remove`. Original possessions and AA state return; identified temporary starters are removed. Earned carried items move to shroud storage and carried earnings add to the original wallet. Live personal-bank changes remain; they are not overwritten by the original snapshot. Ask for `bank` to inspect/retrieve stored earnings. Retrieval still reloads for synchronization; contents are not deleted using an assumed expiry timer.
