#ifndef GAMESOURCE_WORLD_ENTITYMODULES_WORLDENTITYMODULE_PVSMODULE_BRNPVSMODULE_H
#define GAMESOURCE_WORLD_ENTITYMODULES_WORLDENTITYMODULE_PVSMODULE_BRNPVSMODULE_H

// ============================================================================
// GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.h
//
// BrnWorld::PVSModule -- the world's PVS (Potentially-Visible-Set) module. It is a
// single-buffered module: each frame it consumes GetZoneRequest events (which zone
// is the listener in) and produces GetZoneResponse events (the per-zone visibility
// replies), and it owns a game-data request interface used to stream the PVS data.
//
// It derives from CgsModule::ModuleSingleBufferedTemplate<InputBuffer, OutputBuffer>
// (the PVS IO buffers live in SharedIO/BrnPVSModuleEvents.h). The canonical home for
// this header is the path baked into the X360 asserts:
//   d:\p4\b5_main\...\worldentitymodule\PVSModule/BrnPVSModule.h
//
// This slice homes the four functions the X360 ARTIST build emitted out-of-line for
// the class (the dossier's TU):
//   PVSModule()              @ 0x827E4CC8  ctor
//   GetInputInterface()      @ 0x822BAF78  (BrnPVSModule.h:122 assert)
//   GetOutputInterface()     @ 0x822BAFF0  (BrnPVSModule.h:130 assert)
//   GetGameDataRequestInt()  @ 0x822BB068  (BrnPVSModule.h:138 assert)
//
// FLAG (deferred PVSModule-specific members): the X360 ctor zeroes one flag byte at
// this+0x1B58 (stb) and initialises a circular list-head sentinel at this+0x1D78
// (three zero words then three self-pointers then a zero word -- an empty intrusive
// list whose element type is NOT recovered by this slice). They sit AFTER the whole
// ModuleSingleBufferedTemplate sub-object (base + embedded InputBuffer + OutputBuffer);
// they are modelled here as honestly-named members initialised to the ctor-observed
// empty state. Their absolute offsets (0x1B58 / 0x1D78) depend on the exact embedded
// buffer sizes and are NOT asserted -- only the empty-state initialisation is pinned.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBufferedTemplate.h"  // ModuleSingleBufferedTemplate
#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h" // PVSIO::InputBuffer/OutputBuffer
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"           // RequestInterface<512>

namespace BrnWorld
{
    class PVSModule
        : public CgsModule::ModuleSingleBufferedTemplate<PVSIO::InputBuffer, PVSIO::OutputBuffer>
    {
    public:
        // X360 0x827E4CC8. Chains the base (which constructs the two RWMutexes), sets the
        // PVSModule vtable, clears the PVS flag byte and seeds the empty PVS list-head sentinel.
        PVSModule();

        // X360 0x822BAF78. Returns the module's input data structure (the GetZoneRequest queue
        // buffer). The base GetInputStructure() is the lpInputBuffer the asm reads; the
        // SafeGetInputStructure() tripwires ("lpBuffer != NULL" / "lpInputBuffer") fire on null.
        PVSIO::InputBuffer* GetInputInterface();

        // X360 0x822BAFF0. Returns the module's output data structure (the GetZoneResponse queue
        // buffer). Tripwires "lpBuffer != NULL" / "lpOutputBuffer" on null.
        PVSIO::OutputBuffer* GetOutputInterface();

        // X360 0x822BB068. Returns the game-data request interface embedded in the output buffer
        // (asm: GetOutputStructure() + 0x1718 == &OutputBuffer::mGameDataRequestInterface).
        // Tripwires "lpBuffer != NULL" / "lpOutputBuffer" on null.
        BrnResource::GameDataIO::RequestInterface<512>* GetGameDataRequestInt();

    private:
        // FLAG (see header banner): PVSModule-specific tail members the X360 ctor initialises.
        // A circular intrusive list-head sentinel; its element type is not recovered by this
        // slice. The X360 ctor stores: three zero words, then mpHead/mpTail/mpCursor pointing
        // back at the sentinel itself, then a trailing zero word -- the empty-list state.
        struct PvsListHead
        {
            void* mpField0;   // +0x00  cleared to 0
            void* mpField1;   // +0x04  cleared to 0
            void* mpField2;   // +0x08  cleared to 0
            void* mpHead;     // +0x0C  self-pointer (empty list)
            void* mpTail;     // +0x10  self-pointer (empty list)
            void* mpCursor;   // +0x14  self-pointer (empty list)
            u32   muField18;  // +0x18  cleared to 0

            PvsListHead()
                : mpField0(nullptr)
                , mpField1(nullptr)
                , mpField2(nullptr)
                , mpHead(this)
                , mpTail(this)
                , mpCursor(this)
                , muField18(0)
            {
            }
        };

        bool        mbPvsFlag;   // X360 ctor: stb 0 @ this+0x1B58 -- a cleared flag/state byte.
        PvsListHead mPvsList;    // X360 ctor: empty circular list-head sentinel @ this+0x1D78.
    };
}

#endif // GAMESOURCE_WORLD_ENTITYMODULES_WORLDENTITYMODULE_PVSMODULE_BRNPVSMODULE_H
