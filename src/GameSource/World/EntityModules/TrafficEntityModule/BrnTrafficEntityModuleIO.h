#pragma once

// BrnTraffic::BrnTrafficIO IO buffers (TrafficEntityModule shared IO). Reconstructed from the
// DecFIGS DWARF (GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h)
// with member OFFSETS pinned by the X360 retail XEX.
//
// This header currently homes OutputBuffer_PostScene (the post-scene producer buffer the
// traffic module fills with AI-visible traffic + the traffic->race-car interface). The other
// BrnTrafficIO buffers in the DWARF (OutputBuffer_Prepare/PrePhysics/..., InputBuffer_*) are
// reconstructed by their own TUs; only the pieces this TU needs are declared here.

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                                   // CgsModule::IOBuffer
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h" // TrafficAIInterface

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // ============================================================================
    // OutputBuffer_PostScene  (DWARF :291; X360 Construct @ 0x82761830)
    // ============================================================================
    // X360 member offsets (from Construct @ 0x82761830 store displacements):
    //   +0      IOBuffer status flag (*a1 = 1)
    //   +4      mSceneCoarseQueryQueue  (VariableEventQueue<16384,16>::Construct(a1+4))
    //   +16416  mTrafficAIInterface     (GetTrafficAIInterface returns a1+16416; count zeroed)
    //   +61488  mTrafficToRaceCarInterface_PostScene (RivalInTrafficUpdateEvent,34 not here --
    //           it lives inside mTrafficAIInterface @ +45072; this trailing interface is the
    //           post-scene traffic->race-car interface)
    //
    // mSceneCoarseQueryQueue is the SceneManager coarse-query input queue
    // (InputBuffer_Query::InSmCoarseQueryQueue == InCoarseQueryQueue<16384>, a
    // VariableEventQueue<16384,16> subclass that adds NO data members). The X360 places it at
    // offset 4 (4-aligned, right after the 1-byte IOBuffer status), so it is modelled as a
    // 4-ALIGNED 16400-byte sized blob -- NOT the alignas(16) SceneCoarseQueryQueue slice, which
    // would force it to offset 16. mTrafficAIInterface (alignas 16) then lands at 16416 (16400
    // queue ends at 16404, padded up to the next 16-boundary), matching the X360.
    struct OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
        // 4-aligned sized blob for the coarse-query queue (sizeof(VariableEventQueue<16384,16>)
        // == 1 + 16384 + 12 -> round to 4 == 16400). The full queue layout/methods belong to
        // the SceneCoarseQueryQueue TU; this buffer only takes &mSceneCoarseQueryQueue.
        struct SceneCoarseQueryQueue { unsigned char maReserved[16400]; };

        // DWARF :187 -- the trailing traffic->race-car post-scene interface. The DWARF spells
        // it as a 1-byte placeholder (muDUMMY); the X360 zeroes it in Construct. Modelled as the
        // DWARF 1-byte struct (this TU only takes its address, never its interior).
        struct TrafficToRaceCarInterface_PostScene { u8 muDUMMY; };

        void Construct();                                                                  // :249
        const SceneCoarseQueryQueue* GetSceneCoarseQueryQueue() const;                     // :252
        SceneCoarseQueryQueue*       GetSceneCoarseQueryQueue();                            // :253
        const TrafficAIInterface*    GetTrafficAIInterface() const;                        // :255
        TrafficAIInterface*          GetTrafficAIInterface();                              // :256 W (0x827111C0)
        const TrafficToRaceCarInterface_PostScene* GetTrafficToRaceCarInterface_PostScene() const; // :258
        TrafficToRaceCarInterface_PostScene*       GetTrafficToRaceCarInterface_PostScene();        // :259

    private:
        SceneCoarseQueryQueue               mSceneCoarseQueryQueue;                // :263 (offset 4)
        TrafficAIInterface                  mTrafficAIInterface;                   // :264 (offset 16416)
        TrafficToRaceCarInterface_PostScene mTrafficToRaceCarInterface_PostScene;  // :265
    };
}
}
