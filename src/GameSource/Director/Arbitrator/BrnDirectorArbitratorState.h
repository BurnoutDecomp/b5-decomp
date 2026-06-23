#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_STATE_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_STATE_H

#include "types.hpp"

// ----------------------------------------------------------------------------
// BrnDirector::ArbitratorState -- the polymorphic base of every director arbitrator
// state (Roaming, Crashing, Takedown, CrashMode, PostEvent, RaceIntro,
// OnlineRaceIntro, DriveThru, CarSelect, RankUp, OnlineCarSelect).
//
// Layout / vtable order recovered from the DecFIGS DWARF
// (GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h) and the X360
// pseudocode/asm. The container's ConstructAll/UpdateAll/ReleaseAll
// (BrnDirectorArbitratorStateContainer.cpp) dispatch through this base:
//   * ConstructAll calls vtable slot 0  -> Construct          ( (***p)(p) )
//   * UpdateAll    calls vtable slot 2  -> Update             ( (*(*p+8))(p, info) ),
//                  then clears mbCycleCameraThisFrame (byte +0x171).
//   * ReleaseAll   calls vtable slot 3  -> Release            ( (*(*p+0xC))(p, info) )
//
// LAYOUT NOTE: mCamera is embedded BY VALUE in the source build (the byte
// mbCycleCameraThisFrame lands at +0x171 there). Our rebuilt Camera size differs, and
// this TU never touches mCamera by name, so the camera region is preserved as an
// opaque span and parity is BY NAME (the same convention as BrnICEMoviePlayer.h /
// BrnDirectorICEWrapper.h). The +0x171 quote is provenance only.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
    // ArbStateSharedInfo is the per-update shared context (camera container, ICE wrapper,
    // game state, vehicle data, timesteps, ...). It is only ever passed by reference to
    // Update/Release, so it is opaque here -- its real layout lives with the arbitrator
    // update TUs.
    struct ArbStateSharedInfo;

    class ArbitratorState
    {
    public:
        ArbitratorState();

        // Declared in X360 vtable order -- DO NOT REORDER. Construct MUST be vtable
        // slot 0 (the container calls (***p)(p)); there is NO virtual C++ destructor
        // ahead of it -- Destruct() is an explicit virtual (slot 4) instead, matching
        // the DWARF.
        virtual void        Construct();
        virtual bool        Prepare(ArbStateSharedInfo& lrSharedInfo);
        virtual void        Update(ArbStateSharedInfo& lrSharedInfo);
        virtual bool        Release(ArbStateSharedInfo& lrSharedInfo);
        virtual void        Destruct();
        virtual bool        CanRun(ArbStateSharedInfo& lrSharedInfo) const;
        virtual const char* GetName() const;

        // UpdateAll clears this each frame after Update (X360 byte store at +0x171).
        void ClearCycleCameraThisFrame() { mbCycleCameraThisFrame = false; }

    protected:
        bool ShouldCycleCameraThisFrame() const { return mbCycleCameraThisFrame; }
        bool IsDebugDisplayActive() const       { return mbDebugDisplayActive; }

    private:
        // mCamera region (embedded by value in the source build); opaque here.
        u8   mPadCamera[0x16C];   // +0x004 .. +0x170
        bool mbDebugDisplayActive;   // +0x170
        bool mbCycleCameraThisFrame; // +0x171
    };
}

#endif
