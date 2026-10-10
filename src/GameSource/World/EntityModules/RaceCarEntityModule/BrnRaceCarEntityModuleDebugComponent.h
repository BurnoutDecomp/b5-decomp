#ifndef BRN_RACE_CAR_ENTITY_MODULE_DEBUG_COMPONENT_H
#define BRN_RACE_CAR_ENTITY_MODULE_DEBUG_COMPONENT_H

// BrnWorld::RaceCarEntityModuleDebugComponent -- the race-car entity module's debug menu page and
// overlays: the per-car damage / streaming HUD, the player's average speed and engine state, the
// traffic-direction readout, the tailgating cones, the last place-on-track result and the player's
// reset ring. The console embeds it BY VALUE in RaceCarEntityModule (+0x17CF0, between
// mBoostManager and mNearMissManager; BrnPlaceOnTrackManager writes the line-test pair at
// module +0x17CF0 + 0x70 / + 0x80 and ModeArming reads its +0x16 byte at module +0x17D06).
//
// SHAPE: the recovered type information for BrnRaceCarEntityModuleDebugComponent.h (a struct
// derived from CgsDev::DebugComponent), gated on the ledger: its Destruct, OnRegister, GetPlayerCar,
// ResetAverageSpeed and the three Push* recorders have no out-of-line console body (inlined), so
// they are not declared. AirRamAddNew and ResetAverageSpeedCallback are registered as debug-menu
// callbacks with the component as the user data, so they are static and take that pointer.
//
// LAYOUT (console offsets from the asm; the host widens the base and the module pointer):
//   +0x0C mpRaceCarEntityModule        (Construct stores the argument there)
//   +0x10..+0x15 the six recovered bools (OnActivate registers +0x10..+0x14; AirRamAddNew sets +0x15)
//   +0x16 mbForceOnlineFreeburnSpawnPosition -- console-only, absent from the recovered type: OnActivate
//         registers it as "Force Online Freeburn Spawn Position"; Construct zeroes it.
//   +0x18 / +0x1C / +0x20 the three last-distance ints, +0x24 mMag ("Magnitude")
//   +0x30 mLastResetTransform, +0x70 / +0x80 the line-test pair,
//   +0x90 mLastResetLineTestIntersections (count word at +0x130)
//   +0x140 mbRenderTailgateCones, +0x141 mbDrawDamageHUD, +0x142 mbShowEngineState,
//   +0x143 mbDrawResetOnWaterHeight, +0x144 mbShowAverageSpeed, +0x148 mfAverageSpeed,
//   +0x14C miAverageSpeedFrameCount, +0x150 mbTrafficRelatedData

#include "types.hpp"
#include "BrnCommonTypes.h"                                                     // Vector3 / Matrix44Affine / VecFloat
#include "rw/rwcore_structs.h"                                                  // rw::RGBA
#include "GameShared/GameClasses/Containers/CgsArray.h"                         // Array<T,N>
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"

namespace CgsDev
{
    struct Debug2DImmediateRender;
    struct Debug3DImmediateRender;
    struct StrStream;
}

namespace BrnWorld
{
    class RaceCarEntityModule;

    struct RaceCarEntityModuleDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(RaceCarEntityModule* lpRaceCarEntityModule);

        void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender) override;
        void RenderTailgateCones(CgsDev::Debug3DImmediateRender* lpRender);
        void RenderSingleDebugCone(CgsDev::Debug3DImmediateRender* lpRender, Matrix44Affine lTransform,
                                   VecFloat lvfHalfAngle, rw::RGBA lColour, s32 liNumSegments) const;
        void Update() override;
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;
        void RenderEngineState(CgsDev::Debug2DImmediateRender* lpRender);
        void DrawTextWithOffsets(CgsDev::Debug2DImmediateRender* lpRender, CgsDev::StrStream* lpStream,
                                 f32 lfX, f32 lfY);

    protected:
        const char* GetName() const override;
        void OnActivate() override;

    private:
        static void AirRamAddNew(void* lpData);
        static void ResetAverageSpeedCallback(void* lpData);
        void RenderTrafficRelated(CgsDev::Debug2DImmediateRender* lpRender);
        void DrawResetOnWaterHeight(CgsDev::Debug3DImmediateRender* lpRender);

        RaceCarEntityModule*  mpRaceCarEntityModule;          // +0x0C
        bool                  mbRenderCarDamageState;         // +0x10
        bool                  mbRenderCarSreamingState;       // +0x11 (recovered spelling)
        bool                  mbRenderLastResetInfo;          // +0x12
        bool                  mbRenderPlayerResetPositions;   // +0x13
        bool                  mbRenderCurrentDistrict;        // +0x14
        bool                  mbAirRam;                       // +0x15
        bool                  mbForceOnlineFreeburnSpawnPosition; // +0x16
        s32                   miLastDriftDistance;            // +0x18
        s32                   miLastInAirDistance;            // +0x1C
        s32                   miLastOncomingDistance;         // +0x20
        f32                   mMag;                           // +0x24
        Matrix44Affine        mLastResetTransform;            // +0x30
        Vector3               mLastResetLineTestStart;        // +0x70
        Vector3               mLastResetLineTestEnd;          // +0x80
        Array<Vector3, 10>    mLastResetLineTestIntersections;// +0x90
        bool                  mbRenderTailgateCones;          // +0x140
        bool                  mbDrawDamageHUD;                // +0x141
        bool                  mbShowEngineState;              // +0x142
        bool                  mbDrawResetOnWaterHeight;       // +0x143
        bool                  mbShowAverageSpeed;             // +0x144
        f32                   mfAverageSpeed;                 // +0x148
        s32                   miAverageSpeedFrameCount;       // +0x14C
        bool                  mbTrafficRelatedData;           // +0x150
    };
}

#endif // BRN_RACE_CAR_ENTITY_MODULE_DEBUG_COMPONENT_H
