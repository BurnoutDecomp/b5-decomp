#pragma once

// CgsInput::InputModule -- the single-buffered input module: owns the physical controller
// pads (InputPads), the two published bind/unbind result queues, and drives the boot-time
// Construct/Prepare/Release/Destruct state machines plus the per-frame PreWorld/PostWorld passes.
//
// COHERENCE NOTE (verifier): this header REPLACES the stale pre-DWARF stub previously at
// GameShared/GameClasses/Input/CgsInputModule.h (a "SubModule"/RWMutex sketch with a fabricated
// InputModule() constructor in the sibling .cpp). The registered class home in
// progress/class_homes.json is GameShared/GameClasses/Input/CgsInputModule.cpp, so the corrected
// DWARF-shaped class lives here (Input/, NOT System/Input/) to avoid a duplicate CgsInput::InputModule
// definition. The bogus InputModule() ctor body in Input/CgsInputModule.cpp is deleted and the
// four method bodies (Prepare/Release/Destruct/ProcessMappingQueue) put in its place.
//
// Recovered from BURNOUT_X360_ARTIST.XEX with member names/types + the class shape (base class,
// virtual set, EPrepareStage/EReleaseStage enums) from the DecFIGS DWARF (CgsInputModule.h). The
// X360 byte offsets used by the reconstructed bodies:
//   mePrepareStage            @ this+0x228 (552)
//   meReleaseStage            @ this+0x22C (556)
//   mControllers (InputPads)  @ this+0x230 (560)          -- InputPads::Destruct/Prepare take this+0x230
//   mOutputBindResultQueue    @ this+0x130C (4876), miLength @ +0x1314 (4884)
//   mOutputUnbindResultQueue  @ this+0x1378 (4984), miLength @ +0x1380 (4992)
//   mpAllocator               @ this+0x13E4 (5092)
// The queue members sit after the CgsModule::ModuleSingleBuffered base (which widens on the 64-bit
// host: RWMutexes + pointers) and after InputPads (which embeds pointer-width DeviceX360Pad records),
// so their X360 offsets are documented but NOT offsetof-pinned -- the load-bearing contract is the
// typed member each body touches, not a byte-exact offset the pointer-width difference makes
// unattainable. mePrepareStage/meReleaseStage precede the pointer-widening tail, hence are the
// first members after the base.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/System/Input/CgsInputPads.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"   // OutputBuffer::BindResultQueue / UnBindResultQueue, PostWorldInputBuffer
#include "GameShared/GameClasses/System/Input/CgsInputTypes.h"      // CgsInput::InputIO::PadMapping, KU_NUMBER_OF_PADS
#include "rw/rwcore_structs.h"                                      // rw::IResourceAllocator (Prepare param)

namespace rw { namespace core { struct GeneralResourceAllocator; } }

namespace CgsInput
{
    // Forward declaration for the debug game-pad accessor (declared in the DWARF; not in this batch).
    class DebugManagerPad;

    class InputModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // Boot Construct/Prepare/Release/Destruct state stages (DWARF CgsInputModule.h:51/59).
        enum EPrepareStage : s32
        {
            E_INPUTPREPARESTAGE_START     = 0,
            E_INPUTPREPARESTAGE_MANAGER   = 1,
            E_INPUTPREPARESTAGE_INPUTPADS = 2,
            E_INPUTPREPARESTAGE_DONE      = 3,
        };
        enum EReleaseStage : s32
        {
            E_INPUTRELEASESTAGE_START     = 0,
            E_INPUTRELEASESTAGE_INPUTPADS = 1,
            E_INPUTRELEASESTAGE_MANAGER   = 2,
            E_INPUTRELEASESTAGE_DONE      = 3,
        };

        // X360 0x828EEFD8. Resumable boot prepare: run the manager (base ModuleSingleBuffered::Prepare,
        // the no-arg base virtual -- a DISTINCT vtable slot from this Prepare(rw::IResourceAllocator*))
        // then the controller pads, tracking mePrepareStage so a partial prepare resumes. Clears the two
        // published result queues on the first stage. Returns true only when fully DONE.
        virtual bool Prepare(rw::IResourceAllocator* lpAllocator);

        // X360 0x828EF100. Resumable boot release: tear down the pads then the manager (base
        // ModuleSingleBuffered::Release), tracking meReleaseStage. Resets mePrepareStage + the two result
        // queues once released. Returns true only when fully DONE.
        virtual bool Release();

        // X360 0x828F8438. Destruct the controller pads, clear the two published result queues, then
        // chain to the base ModuleSingleBuffered::Destruct.
        virtual void Destruct();

    private:
        // X360 0x828E7098. Drain the post-world pad-mapping request queue: for each PadMapping event,
        // copy its 112-byte action-mapping payload into the addressed pad's mapping storage (port == -1
        // broadcasts to all KU_NUMBER_OF_PADS pads).
        void ProcessMappingQueue(const InputIO::PostWorldInputBuffer* lpPostWorldBuffer);

        // ---- members (see the header banner for the X360 byte offsets) ----
        EPrepareStage mePrepareStage;   // this+0x228 (552)
        EReleaseStage meReleaseStage;   // this+0x22C (556)
        InputPads     mControllers;     // this+0x230 (560)

        // The two result queues the module publishes to the game each frame. Prepare/Release/Destruct
        // reset them to empty by zeroing miLength (BaseEventQueue::Clear).
        InputIO::OutputBuffer::BindResultQueue   mOutputBindResultQueue;   // this+0x130C (4876)
        InputIO::OutputBuffer::UnBindResultQueue mOutputUnbindResultQueue; // this+0x1378 (4984)

        rw::core::GeneralResourceAllocator*      mpAllocator;              // this+0x13E4 (5092)
    };
}
