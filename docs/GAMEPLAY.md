# Gameplay

The current catalog lets an eligible RoF2 character choose the first branch in
each of eight shroud families at levels 5 through 70, in five-level steps. The
selected level cannot exceed the original character's level. Once the server
operator enables the feature, these forms are available without a GM-only gate.
See [Status](STATUS.md) for the enabled branches.

**Death and resurrection are incomplete. In particular, accepting a resurrection
through the hover-respawn window while shrouded can consume a corpse's recoverable
XP without credit. The released implementation does not include the proposed
repair.** Read [Known gaps](KNOWN-GAPS.md) before adventuring.

## Enter a form

1. Leave combat, dismiss your active pet and suspend active mercenaries.
2. Close trading and other inventory transactions, and empty the item and coin
   cursor. Retrieve anything left in your shroud bank from an earlier session.
3. Approach and target a shroudkeeper, then hail to open selection.
4. Choose an enabled family, branch and level. Review its stats, powers and starter
   equipment, then select the form.
5. Expect a same-zone loading screen while the server exchanges profiles.

Your ordinary profile and possessions are held separately during the session.
The shroud has its own equipment, attributes and Monster Point balance. Starter
weapons and a backpack depend on the form; the current catalog does not issue the
full Live consumable supplies.

## Use Monster AAs

Open Alternate Advancement to inspect the shroud's powers. Turn off the
**Can Purchase** filter when checking the complete available list: free or fully
trained powers may otherwise be hidden.

Free powers are granted by the selected form. Spend its Monster Points on the
supported paid ranks; prerequisites, level restrictions and remaining points are
checked by the server. These points do not spend the original character's AA
balance. Make hotkeys for activated powers.

The native Save and Load controls can retain a setup. Loading remains subject to
the selected form's available ranks and point budget. Paid allocations are stored
for the active session; a purchase canary survived a relog. Persistence after
ordinary travel to another zone still needs verification. Do not assume every
rank or ability has been tested in combat.

Attack Speed, Enduring Voice and Improved Plane Shift are currently withheld.
Warlord's Tenacity retains the existing RoF2 spell behavior, including its older
HP decay, rather than matching the modern Live payload exactly.

## Experience and possessions

The selected shroud level stays fixed. Positive rewards can advance separate
family progress and accumulate normal XP/AA rewards for the original character.
Those original rewards are settled when the shroud is removed. Exact Live
progression rates and later-branch thresholds are not established, so the progress
display is provisional and later branches remain disabled.

Earned items are kept separately from temporary starter equipment. When you leave
the form, identified starter items are removed and earned items move to shroud
storage. Earned items placed inside a starter backpack are preserved. Earned
carried coin returns with the original wallet.

Mercenary controls and several banking, trading, crafting and ownership-changing
operations are restricted while shrouded. These restrictions are implementation
limits, not a complete statement of Live rules.

## Return to the original character

Return to the shroudkeeper, target it and say `remove`. Spirit Sending is supplied
as a return power and has a server return hook, but its full native displacement
acceptance remains open. Removing the form uses another same-zone reload.

Ask the keeper for `bank` to inspect and retrieve earned items after restoration.
Retrieval currently reloads the zone to synchronize inventory. Empty this storage
before entering another form. This implementation does not delete stored items
using an assumed Live expiry timer.

The original profile, ordinary AA state and held possessions return through the
transactional restoration path. Full death, resurrection, cooldown and crash
recovery parity remains outside the verified gameplay scope.
