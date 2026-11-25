# Supernova Engineering Rules (Permanent & Non-Negotiable)

Every session, compaction, and `/teamwork` subagent working in `C:\Users\kmric\projects\Supernova` MUST strictly obey these 5 rules:

1. **PERSISTENT ZERO-BS RULE (`CMakeLists.txt` Litmus Test):**
   Never replace a real Petari/Garigari subsystem with a hand-written stand-in or "inspired-by" mini-class. Ported subsystems MUST compile the actual `.cpp` translation units in `native/smg2/CMakeLists.txt` (from `native/petari/` adapted to `SB4E01`, or reconstructed in `native/garigari/` directly against `C:\Users\kmric\projects\garigari-fork\build\SB4E01\asm\` and `native/garigari/config/SB4E01/symbols.txt`).

2. **NO BRIDGE SMUGGLING:**
   `Smg2PetariBridge.cpp`, `Smg2Render.cpp`, and `Smg2Stubs.cpp` are strictly for host/asset/ABI glue and isolated `TODO(Garigari)` linker stubs—NEVER put custom gameplay physics, custom camera math, or custom menu state machines in bridge files.

3. **REAL VERIFICATION GATE (NO TITLE-SCREEN GREENWASHING):**
   Never claim a gameplay, collision, gravity, or menu fix is "verified" just because `Nebula.exe --seconds 2` idled on the Title screen. Every commit must explicitly list:
   - (a) which real `.cpp` translation units were added to `native/smg2/CMakeLists.txt`,
   - (b) which exact code path was actually executed during verification, and
   - (c) any remaining `TODO(Garigari)` blockers honestly.

4. **HONEST README PROGRESS:**
   Never bump percentages in `README.md` for temporary stubs or unverified paths—only count real compiled engine translation units verified in action.

5. **NO SILENT FALLBACKS:**
   If a real subsystem hits a hard dependency wall, STOP and report the exact unresolved symbols/TUs instead of quietly substituting ad-hoc code.
