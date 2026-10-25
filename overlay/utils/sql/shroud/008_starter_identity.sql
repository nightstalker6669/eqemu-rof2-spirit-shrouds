-- Item serial GUIDs are regenerated during inventory loading. NULL is retained
-- only for legacy issuance; new grants persist the same session-bound token in
-- this column and item custom_data. Apply while world/zone processes are stopped.
ALTER TABLE spirit_shroud_starter_provenance
 ADD COLUMN IF NOT EXISTS issued_token VARCHAR(63) NULL;
