# Installation

Read [known gaps](KNOWN-GAPS.md) before enabling this partial implementation. Newly added death/resurrection, interaction and native-transition paths require physical acceptance. No installer changes a database or running service.

## Source baseline

Start with EQEmu `8175ae61874cbf20d7b05102821f6f8a4f4d1c39`, then apply the housing, fellowship and mercenary overlays at the exact commits listed in the root README, using each package's source installer. Leave upstream HEAD unchanged; companion overlays are uncommitted source additions, as their installers expect. Apply only the documented prerequisite files, without unrelated local changes.

```sh
python3 tools/apply_overlay.py /path/to/EQEmu --check
python3 tools/apply_overlay.py /path/to/EQEmu --apply
```

The installer validates package checksums, upstream HEAD, every prerequisite changed/new source hash, absence of unrelated changes, patch applicability and destination collisions. It installs the source patch and new files only. Do not bypass a mismatch by editing the manifest: reconcile the dependency versions first.

The integration also corrects mercenary atomic-save declarations in `zone/zonedb.h` to match the implementations in the pinned mercenary baseline. It adds DB query-failure generation tracking so shroud checkpoint transactions detect a transient failure even if a query retry succeeds. These shared changes are included explicitly in the patch.

## Offline migration and deployment

1. Back up source, database, runtime binaries and opcode configuration. Stop world and every zone process. Drain active shroud sessions by normal restoration before a content upgrade; do not delete escrow or snapshot rows. Keep the old binaries and database backup as a matched rollback pair.
2. Install the companion overlays' required schema according to their documentation. The shroud implementation uses their authenticated inventory writer/lease infrastructure, but does not require a character to own a house.
3. Apply the following files from `utils/sql/shroud/` to the server database, in exactly this order, using a MariaDB-compatible client that **stops on errors**. Do not use `--force`:

   `001_storage.sql`, `003_parity_storage.sql`, `004_live_imp_wizard.sql`, `005_experience_settlement.sql`, `006_starter_inventory.sql`, `007_rof2_enumerable_aa.sql`, `008_starter_identity.sql`, `009_live_captured_catalog.sql`, `011_live_lower_level_forms.sql`, `012_live_paid_monster_rank_chains.sql`, `013_remaining_monster_aa.sql`, `014_death_resurrection_accounting.sql`, `015_system_interactions.sql`.

   Number 002 is an excluded provisional test catalog. Number 010 is not required or distributed. The old comments referring to inclusive numeric ranges describe migration history; the explicit list above is authoritative. Do not substitute research or scratch migrations.
4. These migrations require the corresponding EQEmu/PEQ character, inventory, AA, item and spell tables. Snapshot tables created with `LIKE` must remain schema-compatible with their original tables. AA adapters use IDs in the 48000–49017 range; later guarded migrations reject incompatible namespace contents. Verify all earlier reserved IDs are unused before initial installation. Content migrations retain exact captured per-level free grants and explicitly documented RoF2 spell compatibility differences; they do not replace the ordinary spell database.
5. Build and deploy matching `world` and `zone` binaries using the standard EQEmu build process. Deploy the updated `utils/patches/patch_RoF2.conf` to the runtime opcode-config path. Do not combine a new schema with old source/binaries or change wire mappings piecemeal.
6. In the active ruleset, set `Character:EnableSpiritShrouds=true`, `Character:AllowTestSpiritShrouds=false`, and initially `Character:NativeSpiritShroudTransitions=false` (its source default). Opt into native transitions only for a controlled acceptance pass, then verify both self and observer paths and completion/recovery. The reload fallback remains available for unsupported states. The latter hides only separately marked provisional definitions; it does not impose a GM gate on the enabled observed catalog. Ensure the selected rule set is actually loaded after restart.
7. Restart services, inspect startup errors and verify normal character entry, keeper selection, entry/removal, original AA/inventory restoration and shroud-bank retrieval before regular play. The supported keeper NPC type IDs are 202326, 202327 and 202329. Existing keeper hails and the `remove` keyword are intercepted by the service; no replacement quest script is required.

## Recovery and upgrades

Disabling new entry must never be used to remove recovery/removal access. Preserve the exact active template version and its session JSON until restoration completes. `012` refuses active target sessions; use a stopped, drained server rather than forcing an upgrade. Keep snapshot manifests, session records, allocations and pending XP events together. Never clear an uncertain transaction by guessing which profile is authoritative.

Migration 014 adds versioned corpse loss/refund provenance and explicit unknown progression thresholds. Migration 015 versions the inventory domain: new sessions retain the live personal bank; legacy sessions keep their exact escrow domain. Mercenary fee liabilities survive restoration/reconnect, and cannot block original-profile recovery. Do not rewrite active legacy session snapshots into the new policy.

Shroud bank contents have no implemented expiry deletion policy. Preserve existing bank rows. Restore the predeployment database and binaries together for rollback, with players offline; source-only rollback cannot undo profile or schema changes.

The source-only installer intentionally does not launch clients, migrate SQL, build binaries, change account data or restart services.

The source patch also updates shared inventory loading so a genuine empty character can use the authenticated writer path with housing gameplay disabled and mercenaries enabled. Query failures and lost writer reservations still reject admission. Validate an empty-character login after deploying this build; isolated source checks are not that runtime acceptance.
