#pragma once

// Minimal concrete homes for the eleven director-moment subclasses that
// BrnDirector::MomentController::NewMoment (@0x82255850) allocates through
// AbstractPool<70,20,Vector4>::AllocateVoid<MomentXxx>(). The twelfth subclass,
// MomentBystanderSeesAction, is already homed (concretely) in BrnMoment.h and is
// NOT redefined here.
//
// SCOPE / FLAG: these are deliberately MINIMAL, layout-stubbed slices -- each derives
// from BrnDirector::Moment and overrides the pure-virtual interface just enough to be a
// concrete type (so AllocateVoid<T> can placement-construct one and static_cast the slot
// to it). Each real subclass carries a large member set in its own DWARF home
// (Moments/BrnMomentXxx.h: BehaviourHandles, ShotSelectionInfo, CrashAnalysis, the
// per-type Parameters, etc.); those full layouts land with each moment's own ledger TU.
// The dossier action for the NewMoment TU explicitly permits stubbing these layouts.
// FLAG: subclass DATA MEMBERS are not modelled here (sizeof is the bare Moment base);
// NewMoment never reads any subclass field -- it only allocates, type-tags via the vtable,
// and forwards SetParameters -- so semantic parity for NewMoment holds.
//
// The two nested Parameters records that MomentParameterBank holds by value
// (MomentHardStop::Parameters, MomentTumbling::Parameters) ARE modelled faithfully from
// the DecFIGS DWARF (Moments/BrnMomentHardStop.h, Moments/BrnMomentTumbling.h) so the
// parameter-bank layout is real.

#include "types.hpp"
#include "GameSource/Director/MomentController/BrnMoment.h"   // BrnDirector::Moment

namespace BrnDirector
{
    namespace Camera { class BehaviourManager; }

    // Shared minimal concrete-override block. Every stub subclass needs the same set of
    // overrides to satisfy Moment's pure-virtual interface; expressed once via a macro so
    // the eleven stubs stay one-liners and obviously identical in shape.
    #define BRN_MOMENT_STUB_OVERRIDES(MOMENT_ENUM)                                  \
        bool Prepare(void* /*lrBehaviourController*/) override { return true; }      \
        void Update(f32, void*, const void*) override {}                            \
        bool Release() override { return true; }                                    \
        const char* GetName() const override { return #MOMENT_ENUM; }               \
        EType GetInstanceType() override { return MOMENT_ENUM; }

    // case 0
    class MomentHardStop : public Moment
    {
    public:
        // DWARF: Moments/BrnMomentHardStop.h:111. The hard-stop "ultra slow-mo" tuning the
        // parameter bank stores by value.
        struct Parameters : public Moment::Parameters
        {
            f32 mfDuration;             // BrnMomentHardStop.h:117
            f32 mfSpeedDiffThreshold;   // BrnMomentHardStop.h:118
        };
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_HARD_STOP)
    };

    // case 1
    class MomentHitTraffic : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_HIT_TRAFFIC)
    };

    // case 2
    class MomentTumbling : public Moment
    {
    public:
        // DWARF: Moments/BrnMomentTumbling.h:111.
        struct Parameters : public Moment::Parameters
        {
            // DWARF: Moments/BrnMomentTumbling.h:119.
            enum ESubType
            {
                E_SUBTYPE_TRUCKING_SIDE  = 0,
                E_SUBTYPE_TRUCKING_FRONT = 1,
                E_SUBTYPE_FOLLOW         = 2,
                E_SUBTYPE_LEAD           = 3,
                E_SUBTYPE_SIDE           = 4
            };
            ESubType meSubType;        // BrnMomentTumbling.h:128
            bool     mbCrashMoment;    // BrnMomentTumbling.h:130
            bool     mbTakedownMoment; // BrnMomentTumbling.h:131
        };
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_TUMBLING)
    };

    // case 3
    class MomentTakedownLookback : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_TAKEDOWN_LOOKBACK)
    };

    // case 4
    class MomentPassengerSeesAction : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_PASSENGER_SEES_ACTION)
    };

    // case 6  (case 5 == MomentBystanderSeesAction, homed in BrnMoment.h)
    class MomentFailSafe : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_FAILSAFE)
    };

    // case 7
    class MomentPlayerJumping : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_PLAYER_JUMPING)
    };

    // case 8
    class MomentPlayerStunt : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_PLAYER_STUNT)
    };

    // case 9
    class MomentStaticCamImpact : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_STATIC_CAM_IMPACT)
    };

    // case 10
    class MomentNewCarJoined : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_NEW_CAR_JOINED)
    };

    // case 11
    class MomentStationaryCrash : public Moment
    {
    public:
        BRN_MOMENT_STUB_OVERRIDES(E_MOMENT_STATIONARY_CRASH)
    };

    #undef BRN_MOMENT_STUB_OVERRIDES

} // namespace BrnDirector
