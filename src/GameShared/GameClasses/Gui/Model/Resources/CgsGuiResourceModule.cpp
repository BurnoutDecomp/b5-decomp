#include "types.hpp"

#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::GuiResourceModuleIO::OutputBuffer + its GuiResourceRequestQueue

// CgsGui::GuiResourceModule member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU (class:CgsGui::GuiResourceModule) bodies the one X360-emitted GuiResourceModule
// function scheduled in the cgs-gui3 group:
//
//   HasPendingResourceRequests(OutputBuffer*) @ 0x8284EF48 (1 func, ledger reconciled)
//
// GuiResourceModule is the GUI model's resource subsystem module: per the DecFIGS DWARF
// (CgsGuiResourceModule.h) it is
//   struct CgsGui::GuiResourceModule : public CgsModule::ModuleSingleBuffered {
//       EPrepareStage mePrepareStage; EReleaseStage meReleaseStage;
//       EAcquireStage meAcquireStage / meAfterWaitStage;
//       int32 miAptPersistentBundleBank ... miGlobalTexturePoolId;
//       int32 mNumWaitingResourceAllocations;
//       int32 miNumBundlesToLoad; int32 miNumPendingResources;
//       GuiBundleToLoad maBundlesToLoad[64]; GuiBundleToLoad maBundlesPending[5];
//       bool mbHighDef;
//       EventReceiverQueue<8192,16> mReceiverQueue;
//   };
// The surrounding ModuleSingleBuffered base layout (the dominant ~0x18000-class size of the
// owning ModelModule sub-object) is not yet modelled -- the committed CgsModuleSingleBuffered.h
// is a small declarative slice whose sizeof does not reproduce GuiResourceModule's interior
// member offsets. So, exactly as the sibling subsystem-module body CgsModelModule.cpp does for
// this same un-modelled module, every interior location this function reads is addressed by its
// X360 byte offset through a char* view of `this`. Only the two locations this function reads
// are reproduced; no fabricated layout is asserted.

namespace CgsGui
{
    // Declaration-only skeleton: the body addresses its two counter fields by X360 byte
    // offset (the full ModuleSingleBuffered-derived layout is not yet modelled). The single
    // member bodied here is the X360 @0x8284EF48 HasPendingResourceRequests.
    struct GuiResourceModule
    {
        // X360 @0x8284EF48 (private, DWARF CgsGuiResourceModule.h:271). Clears the output
        // buffer's resource-request queue, then reports whether the module's two
        // request-progress counters differ.
        bool HasPendingResourceRequests(GuiResourceModuleIO::OutputBuffer* lpOutputBuffer);
    };

    // X360 byte offsets the function reads (authoritative, from the @0x8284EF48 asm):
    //   lwz r11, 0x1184(this)   ; *(this + 4484)
    //   lwz r10,  0x254(this)   ; *(this +  596)
    // Both are 32-bit loads (lwz). They are two of the module's int32 request-progress
    // counters; the X360 build separates them by 0xF30 bytes (one sits before the embedded
    // maBundlesToLoad[64]/maBundlesPending[5]/mReceiverQueue bulk, the other after it), so
    // they are addressed by offset rather than named because the intervening base + array
    // layout is not yet modelled.
    static const int KI_HASPENDING_COUNTER_A = 0x0254; // 596
    static const int KI_HASPENDING_COUNTER_B = 0x1184; // 4484

    // X360 0x8284EF48.
    //   mr  r31, r3            ; r31 = this
    //   mr  r3, r4             ; r3 = lpOutputBuffer
    //   bl  OutputBuffer::GetResourceRequestQueue()   ; r3 = &outBuf->mRequestQueue (this+4)
    //   bl  VariableEventQueue<2048,16>::Clear()      ; Clear() on that request queue
    //   lwz r11, 0x1184(r31)   ; counterB = *(this+4484)
    //   lwz r10,  0x254(r31)   ; counterA = *(this+ 596)
    //   subf/cntlzw/extrwi/xori : r3 = (counterA != counterB)   ; the !=-via-cntlzw idiom
    //   blr
    // GetResourceRequestQueue() (non-const, write side @0x8284DF50) returns OutputBuffer's
    // GuiResourceRequestQueue* (ResourceRequestQueue<2048>, is-a VariableEventQueue<2048,16>),
    // so the inherited Clear() binds exactly to VariableEventQueue<2048,16>::Clear().
    bool GuiResourceModule::HasPendingResourceRequests(GuiResourceModuleIO::OutputBuffer* lpOutputBuffer)
    {
        lpOutputBuffer->GetResourceRequestQueue()->Clear();

        const char* lpcThis = reinterpret_cast<const char*>(this);
        const s32 liCounterA = *reinterpret_cast<const s32*>(lpcThis + KI_HASPENDING_COUNTER_A);
        const s32 liCounterB = *reinterpret_cast<const s32*>(lpcThis + KI_HASPENDING_COUNTER_B);
        return liCounterA != liCounterB;
    }
}
