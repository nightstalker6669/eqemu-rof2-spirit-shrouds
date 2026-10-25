-- Apply offline after 014. Legacy sessions retain their exact escrow domain.
-- New sessions retain the ordinary personal bank; shared bank remains escrowed.
ALTER TABLE spirit_shroud_sessions
 ADD COLUMN IF NOT EXISTS inventory_policy TINYINT UNSIGNED NOT NULL DEFAULT 0;

-- Only contracts first hired while shrouded receive a second, original-level
-- hire charge. Insufficient funds never block restoration: the contract stays
-- suspended until this fee is paid on a later normal-profile unsuspend.
-- That insufficient-funds policy is conservative compatibility behavior; Live's
-- exact insufficient-funds response has not been established.
CREATE TABLE IF NOT EXISTS spirit_shroud_mercenary_fees (
 merc_id INT UNSIGNED NOT NULL PRIMARY KEY,
 character_id INT UNSIGNED NOT NULL,
 session_id BIGINT UNSIGNED NOT NULL,
 template_id INT UNSIGNED NOT NULL,
 original_level INT UNSIGNED NOT NULL,
 fee_copper BIGINT UNSIGNED NOT NULL,
 paid_at BIGINT UNSIGNED NULL,
 KEY character_session (character_id,session_id)
) ENGINE=InnoDB;
