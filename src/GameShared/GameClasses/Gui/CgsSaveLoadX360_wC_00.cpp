// CgsGui::SaveLoadSystem -- wave-C bootup/retry lane, reconstructed from BURNOUT_X360_ARTIST.XEX.
// Bodies for the three remaining bootup/save retry-prompt functions:
//
//   BootupCheckDone         @ 0x82859A38 -- Realmc bootup-check result callback: success latches
//                                           mbField183/mbField180 + re-arms the autosave
//                                           pre-flight and reports success; the 1..2 failure
//                                           lanes either arm the SAVELOAD_RETRY_BOOT prompt (a
//                                           signed-in user exists) or report cancelled; >= 3
//                                           fires "Should not get here".
//   BootupHandleRetryBootup @ 0x82855B80 -- the retry-bootup prompt handler: RETRY restarts the
//                                           boot-up load, otherwise the boot-up is abandoned
//                                           (mbField183 = true, mbField180 = false) and failure
//                                           is reported.
//   SaveHandleRetry         @ 0x82856188 -- the save-retry prompt handler: RETRY re-runs Save(),
//                                           otherwise mbField180 = false and failure is reported.
//
// The class layout + member names are frozen in CgsSaveLoadX360.h (X360-authoritative offset map);
// nothing here edits a header. `this` conventions (see the header's note):
//   * BootupCheckDone is a Realmc RESULT callback -- Hex-Rays shows it the +4 interface
//     sub-object, so every displacement in its pseudocode is (real member offset - 4)
//     (raw 0x12C -> mpActiveMessageDisplay +0x130, raw 0x130 -> mpMessageDisplay +0x134,
//     raw 0x17C -> mbField180 +0x180, raw 0x17F -> mbField183 +0x183, raw 0x194 -> the
//     mMessageDisplayOptionFunc pair +0x198, raw 0x20C -> miField210 +0x210). Its
//     `EnableAutosave(a1 - 4)` call is therefore the plain member call EnableAutosave(-1)
//     (r3 = the real `this`, r4 = -1).
//   * BootupHandleRetryBootup / SaveHandleRetry are HandleOption-dispatched option handlers and
//     receive the real `this`; their displacements are the real X360 offsets.
// All three are written as plain member functions over the NAMED members -- no this-arithmetic
// and no raw offset casts.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"   // CgsGui::SaveLoadSystem (FIRST -- see note)

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT

namespace
{
    // Pending message-display option DISPATCH THUNK. The X360 stores the handler as a member-
    // function pointer ({ func @+0x198, delta @+0x19C }) written by BootupCheckDone's
    // `std r9,0x194(r11)`, and HandleOption dispatches it as (adjustedThis, option); this
    // reconstruction stores a plain function pointer, so the thunk forwards to the real
    // CgsGui::SaveLoadSystem member of the same name (CgsSaveLoadPS3.cpp precedent).
    void SaveLoadSystem_BootupHandleRetryBootup(void* lpSystem, u32 luOption)
    {
        static_cast<CgsGui::SaveLoadSystem*>(lpSystem)->BootupHandleRetryBootup(luOption);
    }

    // Localisation string-table entries (X360 rodata pointers).
    const char* const KSTR_RETRY_BOOT             = "SAVELOAD_RETRY_BOOT";              // off_82F331A0
    const char* const KSTR_RETRY                  = "SAVELOAD_RETRY";                   // off_82F331AC
    const char* const KSTR_CONTINUE_WITHOUT_SAVING = "SAVELOAD_CONTINUE_WITHOUT_SAVING"; // off_82F331C0
}

namespace CgsGui
{
    // X360 0x82859A38. Realmc bootup-check result callback (+4 sub-object -- written as a plain
    // member). luResult 0 == the check succeeded, 1..2 are the failure lanes, >= 3 is impossible.
    void SaveLoadSystem::BootupCheckDone(u32 luResult)
    {
        if (luResult != 0)                                   // cmplwi r4,1; blt -> success lane
        {
            if (luResult >= 3)                               // cmplwi r4,3; blt -> prompt lane
            {
                CGS_ASSERT(false, "Should not get here");    // CgsSaveLoadX360.cpp:558
                return;
            }

            if (miField210 >= 0)                             // lwz r10,0x20C(r11); cmpwi/blt
            {
                // The retry prompt's pending option handler = &BootupHandleRetryBootup. The X360
                // builds the member-fn pointer { func @+0x198, delta = 0 @+0x19C } on the stack and
                // stores it with the 8-byte `std r9,0x194(r11)`. muFunc is full-width on x64 (see
                // the header note) -- no truncation.
                mMessageDisplayOptionFunc.muFunc =
                    reinterpret_cast<uintptr_t>(&SaveLoadSystem_BootupHandleRetryBootup);
                mMessageDisplayOptionFunc.muDelta = 0;

                // The X360 reuses the same stack slot for the pair and then the options array.
                const char* lapcOptions[2];
                lapcOptions[0] = KSTR_RETRY;
                lapcOptions[1] = KSTR_CONTINUE_WITHOUT_SAVING;

                // Direct virtual on the display passed to Construct (r3 = raw 0x130 == +0x134),
                // handler = this as the embedded OptionHandler (r4 = r11 - 4), count = 2.
                mpMessageDisplay->ShowMessage(static_cast<MessageDisplay::OptionHandler*>(this),
                                              KSTR_RETRY_BOOT, lapcOptions, 2);
                return;
            }

            // No signed-in user: nothing to retry with -- report cancelled through the bound
            // result handler (r3 = raw 0x12C == mpActiveMessageDisplay +0x130, r4 = 2).
            reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
                ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_CANCELLED);
            return;
        }

        mbField183 = true;      // stb r10,0x17F(r11) -- +0x183 first
        mbField180 = true;      // stb r10,0x17C(r11) -- +0x180
        EnableAutosave(-1);     // r3 = the real this (r11 - 4), r4 = -1
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_SUCCESS);
    }

    // X360 0x82855B80. The retry-bootup prompt handler armed by BootupCheckDone's failure lane
    // (real `this`). Option 1 == RETRY: restart the boot-up load (the X360 tail-calls BootupStart
    // with r4 = 0 -- its parameter is the never-read HandleOption dispatch argument). Otherwise
    // the boot-up is finished without a save and failure is reported.
    void SaveLoadSystem::BootupHandleRetryBootup(u32 luOption)
    {
        if (luOption == 1)
        {
            BootupStart(0);
            return;
        }

        mbField183 = true;      // stb r10,0x183(r3)
        mbField180 = false;     // stb r9,0x180(r3)
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_FAILURE);
    }

    // X360 0x82856188. The save-retry prompt handler (real `this`). Option 1 == RETRY: re-run the
    // write (the X360 tail-calls the zero-argument Save @0x82856040). Otherwise disarm the autosave
    // flag and report failure.
    void SaveLoadSystem::SaveHandleRetry(u32 luOption)
    {
        if (luOption == 1)
        {
            Save();
            return;
        }

        mbField180 = false;     // stb r10,0x180(r3)
        reinterpret_cast<SaveLoadTaskResultHandler*>(mpActiveMessageDisplay)
            ->HandleSaveLoadTaskResult(E_SAVELOADTASKRESULT_FAILURE);
    }
}
