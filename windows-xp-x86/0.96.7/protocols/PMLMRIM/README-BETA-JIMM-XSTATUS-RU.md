# PMLMRIM BETA — Jimm X-status experiment (Miranda NG 0.96.8)

**Separate experimental branch**: based on `PMLMRIM-0.96.8-XStatus-client-detection-repair-TEST.zip`. Stable 0.96.7/0.96.8 archives were not modified.

## What was actually extracted
The attached `Jimm_classic_zastavka (1).jar` contains `mrim-xstatus.txt` (29 URI-to-Russian-name pairs) and `mrim-xstatus.png` (464 × 16 pixels: exactly 29 icons of 16 × 16). This appears to be a 2010-era Jimm build, **not verified to be Jimm Multi**. The JAR also contains references to `mrim2jimm` / `jimm2mrim` translation; it cannot establish which modern MRIM servers accept every X-status ID.

## Changes
- Added **29 Jimm statuses** after the existing 16 in a separate menu group. No ID renumbering.
- Extracted each of the 29 Jimm sprites into a dedicated 16 × 16 ICO resource (`35000 + status id`) instead of guessing Agent ID associations.
- Existing original 16 Agent X-statuses and their resources untouched.
- Incoming X-statuses are recognized only when their URI is in one of the two explicit catalogs; unknown IDs remain hidden.
- No extra-icon fallback: `INVALID_HANDLE_VALUE` is still used to remove stale icons.
- Kept CP1251 outgoing text, login/reconnect persistence, and Tipper fields from the baseline.

## Known limitations
- **Experimental server support**: some Jimm statuses may not be accepted by the actual MRIM server. The code logs packet transmission but cannot claim server acceptance.
- Original Jimm source was bytecode and this audit does not fully reverse engineer its network stack. We used only verified embedded resources/catalog metadata.
- Visual Studio compile and real network interoperability are not verified.
- Do not install over stable PMLMRIM until tested on a copy of your Miranda profile.
