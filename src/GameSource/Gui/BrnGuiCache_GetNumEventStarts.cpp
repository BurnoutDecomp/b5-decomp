// GameSource/Gui/BrnGuiCache_GetNumEventStarts.cpp
//
// ⭐ EMPTY ON PURPOSE since 2026-08-29 (wave G3). This TU existed only to hold
// BrnGui::GuiCache::GetNumEventStarts away from BrnGuiCache.cpp, because the body it
// carried needed the REAL BrnGameStateSharedIO.h types (it forwarded to
// SetUpAllEventStartsInterface::GetNumEventStarts through a raw `this + 0x5690` cast) and
// those clash with the compile-only network/game-state slices BrnGuiOptionsDataProfile.h
// pulls into BrnGuiCache.cpp.
//
// THE FORWARDER WAS WRONG, and the split it forced is no longer needed. It was written
// against a 0x824F8830 attribution that BrnGuiCache.h has since retired; the real export
// (BrnGui::GuiCache::GetNumEventStarts @0x8241E4C8) calls nothing -- it reads the count
// member at cache+0x7760 (== the embedded interface's +0x20D0) behind the CgsArray "used
// before Construct/Clear" guard and returns it. That body needs no BrnGameStateSharedIO.h
// type, so it now lives BY NAME in the mounted partfile
//     GameSource/Gui/BrnGuiCache_wJ_01.cpp
// where it closes the LNK2019 this unmounted TU never could. Do not restore the forwarder:
// the two definitions cannot coexist (LNK2005), and the forwarder re-applies an X360 byte
// offset on the 64-bit host, which is the one thing this tree's members-by-name rule exists
// to prevent.
//
// The file is kept (rather than deleted) so this note stays attached to the name anyone
// searching the old symbol will find. It is NOT in build_game_exe.bat / build.rsp, and
// contributes no symbols.
