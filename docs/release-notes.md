# v0.1.0 — experimental prerelease

Independent minimal Ion timing transplant for the existing R2Vanilla ABI1 loader.

- Vanilla 5ms minimum command duration, render-independent command scheduling.
- Extra mouse sampling, preserved key state, movement accumulation and full command time restoration.
- Client-time precision remainder correction.
- Fail-closed module SHA/PE/unique signature validation; no guessed addresses.
- Optional timestamp CSV diagnostics and analysis tool.

**NOT verified in a running game or on multiplayer servers.** Offline timing tests
and isolated real-module trampoline validation passed. This is a test build, not
a proven fix. It does not set FPS, modify existing CFG or graphics settings, or
install itself into the game. No injector or anti-cheat bypass is included.

The release ZIP contains source, third-party notices, TF2HighFPSFix.dll and
TF2HighFPSFix.ini. Copy only the DLL and INI to your existing R2Vanilla/plugins
directory while the game is closed. See README.md and docs/testing.md before use.

Only the retail module fingerprints in build_profile.json are supported.
Different versions or conflicting hooks must disable the patch.

DLL SHA-256: F56A7F7BC295B472E28C3FCF54C1A2FD983B1BB3DE8ABF4A13CAF0E2690F4AC0
