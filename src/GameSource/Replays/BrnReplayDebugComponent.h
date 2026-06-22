#pragma once

// BrnReplays::DebugComponent -- the in-game debug overlay for the replay system
// (record/playback HUD, per-serialiser buffer usage graphs, stream-block views).
// DWARF home: GameSource/Replays/BrnReplayDebugComponent.h:93.
//
// MINIMAL SLICE. This wave homes the class skeleton (derived from the committed
// CgsDev::DebugComponent base) and bodies only the leaf identity override
// GetName() @0x827DD230, which the boot trace / debug-menu registration needs.
// The full member layout (mpReplayModule/mpAllocator/mpSerialisers, the render
// helpers, PreUpdateRecord/PostUpdateRecord, the *CB action callbacks, and the
// DebugSerialiserInfo/DebugGraph helper structs at BrnReplayDebugComponent.h:52/76)
// is left to the full DebugComponent TU -- GROW this home additively when it lands;
// do NOT fork it.

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace BrnReplays
{
    // DWARF: BrnReplayDebugComponent.h:93 -- struct DebugComponent : public CgsDev::DebugComponent.
    class DebugComponent : public CgsDev::DebugComponent
    {
    protected:
        // GetName @ 0x827DD230. Identity string shown in the debug menu / used when
        // the component registers itself. The X360 body is a single return of the
        // string literal "Replays".
        //   0x827DD230  lis r11, aReplays@ha ; addi r3, r11, aReplays@l ("Replays") ; blr
        virtual const char* GetName() const { return "Replays"; }
    };
}
