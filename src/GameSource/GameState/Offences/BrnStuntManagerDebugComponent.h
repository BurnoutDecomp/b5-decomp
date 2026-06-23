// b5-decomp/src/GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h
//
// BrnGameState::StuntManagerDebugComponent - the in-game stunt/offence debug menu. Derives from the
// real CgsDev::DebugComponent. Base/member types + order are authoritative from the DecFIGS DWARF
// (GameSource/GameState/Offences/BrnStuntManagerDebugComponent.h:50/86/87/88) and reproduce the X360
// object layout exactly: DebugComponent base sub-object = +0x00..+0x0F (vtable@0, mbActive@4,
// mpDebugLinkedListNext@8), mpStuntManager = +0x0C, maStrStreams[0] = +0x10 (== the r26+0x10 the
// X360 ctor strides from), each CgsDev::SimpleStrStream = 264B / 0x108 (matching the r31+=0x108
// stride). Only the constructor pass is reconstructed here; the Construct/Destruct/RenderHUD/
// RenderWorld/OnActivate/GetName/GetPath/CompleteAll* methods are declared-only, owned by their
// own passes.
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Development/CgsStrStream.h"        // CgsDev::SimpleStrStream (committed home)
#include "BrnCommonTypes.h"                                         // Vector3
#include "SharedClasses/World/BrnWorldRegion.h"                     // BrnWorld::WorldRegion (GetTriggerWorldRegion return)

namespace CgsDev { struct Debug2DImmediateRender; struct Debug3DImmediateRender; }

namespace BrnGameState
{
    struct StuntManager;          // pointer member only; full def in BrnStuntManager.h
    enum  StuntElementType;       // used only by declared-only CompleteAllStuntTypeButOne; home TBD

    class StuntManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        StuntManagerDebugComponent();                              // @ 0x827DB6B0 (this pass)

        void Construct(StuntManager* lpStuntManager);
        void Destruct();

        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) override;
        void RenderWorld(CgsDev::Debug3DImmediateRender* lpRender) override;
        void OnActivate() override;

    protected:
        const char* GetName() const override;
        const char* GetPath() const override;

    private:
        void        CompleteAllJumps(void* lpData);
        void        CompleteAllSmashes(void* lpData);
        void        CompleteAllStunt(void* lpData);
        void        CompleteAllStuntTypeButOne(StuntElementType leType);
        void        CompleteAllJumpsButOneCallback(void* lpData);
        void        CompleteAllSmashesButOneCallback(void* lpData);
        void        CompleteAllBillboardsButOneCallback(void* lpData);

        // @ 0x8236F070. Sample the StuntManager's 2D district map at a trigger's
        // world position and return the resulting world region. The X360 reads
        // the position as three floats (a3[0..2]); a w lane of 0 is filled before
        // the WorldMap2D::GetValue(Vector3) sample. Returns a WorldRegion clamped
        // to (E_DISTRICT_INVALID -> DistrictToCounty(E_DISTRICT_INVALID)) when the
        // sample is off-map (KU_INVALID_WORLD_MAP_VALUE == 255); otherwise the
        // region Construct()ed from the sampled district byte.
        BrnWorld::WorldRegion GetTriggerWorldRegion(const Vector3& lrTriggerPosition);

        // DWARF member layout (BrnStuntManagerDebugComponent.h:86-88), declared order:
        StuntManager*           mpStuntManager;     // +0x0C (set by Construct, NOT the ctor)
        CgsDev::SimpleStrStream maStrStreams[3];     // +0x10 .. (3 x 264B)
        Vector3                 maLastPositions[3];  // after the streams
    };
}
