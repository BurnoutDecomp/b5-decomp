// CgsGui::SaveLoadSystem - the Xenon (X360) save/load front-end, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Wave-B partfile 02 (spec group G3 -- the small state setters):
//   SetSilentMode          @ 0x82473110
//   SetSignedInUserIndex   @ 0x8284C210
//   SetAutosaveDone        @ 0x8284CC00
//
// These are plain state setters over the +0x180/+0x181/+0x183 flag bytes and the +0x210 user
// index, forwarding to two RealmcIface::MemcardInterface vtable slots (UserSignedIn @+0x10,
// SetSilent @+0x18). SetAutosaveDone is a Realmc RESULT callback (Hex-Rays shows the +4 interface
// sub-object, so its raw displacement 0x17C is the real +0x180 member) -- written as a plain
// member over the NAMED member per the keystone spec (A1 the ±4 rule).

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"   // CgsGui::SaveLoadSystem; pulls RealmcIface::MemcardInterface

namespace CgsGui
{
    // X360 0x82473110. Toggle silent mode. The asm reloads mbField181 between the two edge tests,
    // so they are two INDEPENDENT ifs (not if/else): a falling edge (was silent, now not) forwards
    // SetSilent(0, -1); a rising edge (was not, now silent) forwards SetSilent(1, -1); then the new
    // state is cached. The -1 second argument's role is unrecovered (const li r29,-1 in the asm).
    void SaveLoadSystem::SetSilentMode(bool lbSilentMode)
    {
        if (mbField181 && !lbSilentMode)                     // lbz 0x181; !a2 -> SetSilent(0,-1)
            mpMemcardInterface->SetSilent(0, -1);            // slot 6 +0x18
        if (!mbField181 && lbSilentMode)                     // reload lbz 0x181; a2 -> SetSilent(1,-1)
            mpMemcardInterface->SetSilent(1, -1);            // slot 6 +0x18
        mbField181 = lbSilentMode;                           // stb r30,0x181
    }

    // X360 0x8284C210. Cache the signed-in user index. A valid index (>= 0) tail-forwards
    // UserSignedIn() to the memory-card interface; a negative index clears the bootup-check flag.
    void SaveLoadSystem::SetSignedInUserIndex(s32 liUserIndex)
    {
        miField210 = liUserIndex;                            // stw r4,0x210
        if (liUserIndex >= 0)                                // bge -> tail call
            mpMemcardInterface->UserSignedIn();              // slot 4 +0x10
        else
            mbField183 = false;                              // stb 0,0x183
    }

    // X360 0x8284CC00. Realmc set-autosave result callback (the +4 interface sub-object; raw
    // displacement 0x17C == real member +0x180). Autosave is considered armed only when the SDK
    // reported success (liResult == 0) AND the enable flag was set (liEnabled == 1). liArg2 (r5) is
    // never read.
    void SaveLoadSystem::SetAutosaveDone(s32 liResult, s32 liArg2, s32 liEnabled)
    {
        mbField180 = (liResult == 0 && liEnabled == 1);      // stb r11,0x17C (real +0x180)
    }
}
