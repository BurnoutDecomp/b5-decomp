#ifndef GAMESOURCE_EFFECTS_PARTICLEEFFECTHELPER_H
#define GAMESOURCE_EFFECTS_PARTICLEEFFECTHELPER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Matrix44Affine
#include "GameSource/Effects/Particles/ParticleModule.h"    // BrnParticle::ParticleModule, LionEffect

// ============================================================================
// GameSource/Effects/ParticleEffectHelper.h
//
// BrnEffects::ParticleEffectHelper and BrnEffects::RaceCarParticleEffectHelper --
// the thin facades the effects state machines (boost / jump / wheel) use to start,
// stop and tweak playing LION particle effects without touching the particle
// module directly. The base wraps the ParticleModule; the race-car derivation adds
// the per-car context (race-car state, debug component, world index, car colour,
// game mode, vehicle locators) the boost/jump machines consult.
//
// SOURCE-OF-TRUTH: member SET / order / types / accessor signatures from the
// DecFIGS DWARF (GameSource/Effects/ParticleEffectHelper.h), gated on the ARTIST
// ledger. The member offsets used by THIS TU were verified against the
// BoostStateMachine asm (mpRaceCarState / mpDebugComponent / meCurrentGameMode /
// mpVehicleLocators reads in OnDetermineNextState 0x82288230 and OnTick
// 0x82298BE0). Layout is modelled with x64-host widths and accessed BY NAME (not
// console byte offsets) -- the standard recon rule. The SetEffect*/StartEffect/
// StopEffect bodies live in ParticleEffectHelper's own TU; declared-only here.
// ============================================================================

namespace BrnEffects
{
    // Forward declarations -- only used by pointer/reference in the layout below
    // (full types not needed to compile this TU's consumers; bodies own their TUs).
    class ActiveRaceCarData;
    class EffectsModule;
    class EffectsDebugComponent;
}
namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }
namespace BrnPhysics { namespace Deformation { struct VehicleLocatorOutput; } }

namespace BrnEffects
{
    // ------------------------------------------------------------------------
    // ParticleEffectHelper (base) -- wraps the ParticleModule.
    // ------------------------------------------------------------------------
    class ParticleEffectHelper
    {
    public:
        // ParticleModule() (DWARF :46/:186). The wrapped module. A non-const overload
        // is provided so the boost-effect helpers can resolve + tweak playing slots
        // (the X360 stores a single module reference and pokes through it).
        BrnParticle::ParticleModule&       ParticleModule()       { return *mpParticleModule; }
        const BrnParticle::ParticleModule& ParticleModule() const { return *mpParticleModule; }

        // The mutate/query surface (DWARF :56..:120). Bodies live in
        // ParticleEffectHelper's own TU; declared-only here for the shape.
        void StartEffect(const char* lpcEffectName, u32& lruHandle, u32 luWorldIndex);
        void StopEffect(u32& lruHandle);
        void SetEffectEnabled(u32 luHandle, bool lbEnabled);
        void SetEffectWorldIndex(u32 luHandle, u32 luWorldIndex);
        void SetEffectTransform(u32& lruHandle, const Matrix44Affine& lTransform);
        void SetEffectStateBlend(u32 luHandle, f32 lfBlend);
        void SetEffectStateBlend(const u32* lpuHandles, u32 luCount, f32 lfBlend);

    protected:
        // DWARF :136. The wrapped module (stored as a reference in the X360 build).
        BrnParticle::ParticleModule* mpParticleModule;   // +0x00
    };

    // ------------------------------------------------------------------------
    // RaceCarParticleEffectHelper -- adds the per-race-car context.
    // ------------------------------------------------------------------------
    class RaceCarParticleEffectHelper : public ParticleEffectHelper
    {
    public:
        // Accessors (DWARF :166..:210). Declared-only (bodies own their TU); the
        // ones this TU needs are spelled out for the call sites.
        const EffectsDebugComponent*                     DebugComponent() const { return mpDebugComponent; }
        const BrnPhysics::Vehicle::RaceCarState*         RaceCarState() const   { return mpRaceCarState; }
        ActiveRaceCarData*                               ActiveRaceCar() const  { return mpActiveRaceCar; }
        EffectsModule*                                   GetEffectsModule() const { return mpEffectsModule; }
        u32                                              WorldIndex() const     { return muWorldIndex; }
        u32                                              CurrentGameMode() const{ return meCurrentGameMode; }
        const BrnPhysics::Deformation::VehicleLocatorOutput* VehicleLocators() const { return mpVehicleLocators; }

    private:
        // ---- data members (DWARF :235..:242) -------------------------------
        ActiveRaceCarData*                          mpActiveRaceCar;     // +  mActiveRaceCar (ref)
        EffectsModule*                              mpEffectsModule;     //    mEffectsModule (ref)
        const BrnPhysics::Vehicle::RaceCarState*    mpRaceCarState;      //    mpRaceCarState
        const EffectsDebugComponent*                mpDebugComponent;    //    mpDebugComponent
        u32                                         muWorldIndex;        //    muWorldIndex
        f32                                         maCarColour[4];      //    mCarColour (RwRGBAReal, 4 floats)
        u32                                         meCurrentGameMode;   //    meCurrentGameMode (EGameModeType)
        const BrnPhysics::Deformation::VehicleLocatorOutput* mpVehicleLocators; // mpVehicleLocators
    };
}

#endif // GAMESOURCE_EFFECTS_PARTICLEEFFECTHELPER_H
