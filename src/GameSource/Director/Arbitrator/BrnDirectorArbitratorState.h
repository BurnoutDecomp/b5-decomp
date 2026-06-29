#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_STATE_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_STATE_H

#include "types.hpp"
#include "GameSource/Director/Camera/Camera.h"   // BrnDirector::Camera::Camera (mCamera by value)

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
    // ---- pointee types the shared-context holds by pointer (declaration-only) -------------
    // ArbStateSharedInfo carries these by pointer only; forward declarations suffice (the
    // states reach them by name and the real layouts live in their own TUs). Where this TU
    // dereferences one (GameState, AllVehicleData, the state container, the behaviour
    // manager), the consumer #includes the real header itself.
    struct SharedCameraContainer;
    class  DebugPrinter;
    class  DebugLog;
    class  ICEWrapper;
    struct DirectorOutputInterface;
    class  ArbitratorStateContainer;
    namespace Camera { class BehaviourManager; }
    struct NamedParameters;
    class  MomentController;
    struct GameState;
    struct Random;
    class  DirectorResourceManager;
    struct EffectInterface;
    struct PlayerCrashInfo;
    class  AllVehicleData;
    class  VehicleTracker;
    struct ControllerInfo;
    struct VehicleInfo;
    class  Camera2DRotationController;
    class  CameraSphericalRotationController;

    // ArbStateSharedInfo -- the per-update shared context passed by reference to every
    // arbitrator state's Prepare/Update/Release. Layout recovered VERBATIM from the DecFIGS
    // DWARF (BrnDirectorArbitratorState.h:54), X360-attested: the ARTIST asm dereferences
    // exactly these slots (mpStateContainer @+0x14, mpBehaviourManager @+0x18, mpGameState
    // @+0x24, mpEffectInterface @+0x30, mpAllVehicleData @+0x38, mpPlayerTracker @+0x3C,
    // mfTimestep @+0x5C, mfSimTimestep @+0x60 on the 4-byte-pointer console). The X360
    // offsets quoted are the console layout; on the x64 host the pointers widen and the two
    // trailing timesteps shift -- access is BY NAME (the x64-gate rule).
    struct ArbStateSharedInfo
    {
        SharedCameraContainer*                  mpSharedCameraContainer;      // +0x00
        DebugPrinter*                           mpDebugPrinter;               // +0x04
        DebugLog*                               mpDebugLog;                   // +0x08
        ICEWrapper*                             mpICEWrapper;                 // +0x0C
        DirectorOutputInterface*                mpOutputInterface;            // +0x10
        ArbitratorStateContainer*               mpStateContainer;             // +0x14
        Camera::BehaviourManager*               mpBehaviourManager;           // +0x18
        const NamedParameters*                  mpNamedParameters;            // +0x1C
        MomentController*                       mpMomentController;           // +0x20
        GameState*                              mpGameState;                  // +0x24
        Random*                                 mpRandom;                     // +0x28
        const DirectorResourceManager*          mpDirectorResourceManager;    // +0x2C
        const EffectInterface*                  mpEffectInterface;            // +0x30
        const PlayerCrashInfo*                  mpPlayerCrashInfo;            // +0x34
        const AllVehicleData*                   mpAllVehicleData;             // +0x38
        const VehicleTracker*                   mpPlayerTracker;              // +0x3C
        const ControllerInfo*                   mpControllerInfo;             // +0x40
        const VehicleInfo*                      mpRaceCars;                   // +0x44
        const VehicleInfo*                      mpPlayerCar;                  // +0x48
        const void*                             mpPlayerCarTransform;         // +0x4C  (Matrix44Affine*)
        s32                                     mePlayerActiveRaceCarIndex;   // +0x50  (EActiveRaceCarIndex)
        const Camera2DRotationController*       mpRotationController;         // +0x54
        const CameraSphericalRotationController* mpSphericalRotationController; // +0x58
        f32                                     mfTimestep;                   // +0x5C
        f32                                     mfSimTimestep;                // +0x60
    };

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

        // The director camera this state drives. DWARF (BrnDirectorArbitratorState.h:135/157)
        // pins mCamera as a protected by-value member with a public const GetCamera(); the
        // non-const accessor is what the roaming state's effect-trigger calls need (the X360
        // passes &mCamera == this+0x10 to EnsureEffectIsPlaying / StopCurrentEffect).
        const Camera::Camera& GetCamera() const { return mCamera; }

    protected:
        bool ShouldCycleCameraThisFrame() const { return mbCycleCameraThisFrame; }
        bool IsDebugDisplayActive() const       { return mbDebugDisplayActive; }

        // Clear the two base camera flags. The derived states' virtual Construct() re-zero
        // them at init time (X360 byte stores at +0x170 / +0x171); exposed protected so the
        // derived Construct can reset them by name without poking the base privates by offset.
        void ResetBaseCameraFlags() { mbDebugDisplayActive = false; mbCycleCameraThisFrame = false; }

        // Mutable camera access for the derived states' per-frame effect / transition pokes.
        Camera::Camera& GetNonConstCamera() { return mCamera; }

        // mCamera is embedded by value at +0x10 (X360 Camera::Construct(this+0x10)); the
        // states reach it by name (parity rule). The two trailing flag bytes follow it at the
        // source build's +0x170/+0x171.
        Camera::Camera mCamera;          // +0x10
    private:
        bool mbDebugDisplayActive;   // +0x170 (console)
        bool mbCycleCameraThisFrame; // +0x171 (console)
    };
}

#endif
