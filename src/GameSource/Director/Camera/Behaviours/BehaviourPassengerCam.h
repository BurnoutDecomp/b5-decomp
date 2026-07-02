#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                 // EActiveRaceCarIndex
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"          // BrnDirector::Camera::Behaviour base slice + shared-info fwd types

// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h
//
// BrnDirector::Camera::BehaviourPassengerCam -- the "passenger cam" camera
// behaviour. Class shape / method set / member names verbatim from the DecFIGS
// DWARF (BehaviourPassengerCam.h:55/:98/:104); gated on the X360 ledger. This
// TU bodies Construct/Update/Release/GetName; the rest of the surface
// (SetParameters/Prepare/SetupTweaker/StartLookingAtRaceCar and
// Parameters::Construct) is declared-only (their own ledger functions / not
// X360-exported).
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

struct BehaviourPassengerCam : public Behaviour
{
    // DWARF BehaviourPassengerCam.h:104 -- the authored parameter block.
    struct Parameters : public Behaviour::Parameters
    {
        // BehaviourPassengerCam.h:108 (cpp:34 body calls Behaviour::Parameters::
        // Construct) -- not X360-exported (folded); declared-only.
        void Construct();

        // BehaviourPassengerCam.h:115 -- Utils::CameraImpactEffect::Parameters
        // BY VALUE. FLAG: the impact-effect parameter block's own home
        // (class:BrnDirector::Camera::Utils::CameraImpactEffect) has not landed;
        // modelled as opaque NOMINAL storage (accessed by no function in this TU).
        u8 maImpactParams[0x40];   // NOMINAL -- grown by CameraImpactEffect's own TU
    };

    // DWARF :122 -- declared-only (X360 header-inline; no exported body).
    void SetParameters(const Parameters* lpParameters);

    // The virtual surface (DWARF cpp:50/:65/:84/:99/:116/:129).
    virtual void Construct();                                                  // @0x821F9E48 (this TU)
    virtual bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo);     // own ledger fn (declared-only)
    virtual bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo);  // @0x821F9E70 (this TU)
    virtual void Release(const BehaviourSharedPrepareReleaseInfo& lrInfo);     // @0x821F9EC0 (this TU)
    virtual void SetupTweaker(Utils::Tweaker& lrTweaker);                      // own ledger fn (declared-only)
    virtual const char* GetName() const;                                       // @0x821F9ED8 (this TU)

    // DWARF :145 -- declared-only (own ledger function).
    void StartLookingAtRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex);

private:
    // DWARF :98 -- X360 this+0x14 (right after the base's +0x10 debug-name).
    const Parameters* mpParameters;
};

}
}
