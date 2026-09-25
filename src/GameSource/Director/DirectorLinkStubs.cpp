// ============================================================================
// GameSource/Director/DirectorLinkStubs.cpp
//
// PC LINK-MOUNT STUBS for the DirectorModule mount: declaration-only callees of the
// mounted director spine whose owning TU is not in the link (the TU does not exist, or
// mounting it would drag an un-landed sub-system in behind it). Each gets a MARKED,
// QUIET stub here rather than a fabricated body inside a real header:
//
//   * every stub carries WHY it is a stub and a DELETE-WHEN note;
//   * no stub traps on a per-frame path -- a trap would make the exe unusable;
//   * a stub that must return a value returns the console's own "nothing happened" value;
//   * NO stub returns a reference to a fabricated object. Where a symbol returns a reference
//     the stub is omitted and the caller's TU is kept out of the link instead.
//
// When a real TU lands for any symbol below, DELETE its stub here (a duplicate definition
// is a link error, so the removal is enforced by the build).
//
// GROUP C -- sub-systems with no landed TU (the director DebugComponent).
// GROUP D -- RETIRED 2026-09-26 (the pointer-amount rw SLerp; see its note below).
// GROUP F -- RETIRED 2026-09-24 (the moment sub-system; see the foot of this file).
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"

#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h"
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"

#include "GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateDriveThru.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRankUp.h"
#include "GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h"

#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"           // the DWARF home (NOT the stale BrnBehaviourPassengerCam.h slice)

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"                       // group E
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h" // group E

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"
#include "SharedClasses/Trigger/BrnRegion.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"

// ----------------------------------------------------------------------------
// GROUP C -- sub-systems with no landed TU.
// ----------------------------------------------------------------------------
namespace BrnDirector
{
    // -- The three still-gated functions of the director's own CgsDev::DebugComponent page
    //    ("Camera"); the rest of the page lives in
    //    DirectorModule/BrnDirectorModuleDebugCompononent.cpp. These three index
    //    DirectorModule regions this reconstruction does not model yet.
    //    QUIET no-ops: the Camera page registers no variables and draws no overlay.
    void DebugComponent::UpdatePanoramaScreenshots(Camera::Camera* lpCamera)
    {
        (void)lpCamera;
    }

    void DebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        (void)lpRender;
    }

    void DebugComponent::OnActivate() {}

    // -- RETIRED 2026-09-25 (FX-DIRECTOR2): the two scene-query post-office stand-ins
    //    (sub_8221CC98, OutEventVolumeTestDeepest) are gone. They are PostOffice<T,N>::Clear (the
    //    fine office's specialisation) and PostOffice<T,N>::AddPostBox, typed in
    //    Utils/BrnPostOffice.h and called by name from Utils/BrnSceneQueryInterface.cpp.
}

// ----------------------------------------------------------------------------
// GROUP D -- RETIRED 2026-09-26 (crash parity FX-LASTFIX). The stub that stood here -- a pointer-amount
// `rw::math::vpu::SLerp(const Matrix44Affine&, const Matrix44Affine&, const float*)` that returned `lrTo` -- is GONE.
// No such overload exists on the console. Its banner named InertiaController::Update, but that site was reconciled to
// the four-argument form on 2026-07-29; the stub's last caller was BehaviourIceAnim::Update, whose `bl 0x82247354` goes
// to rw::math::vpu::SLerp @0x82216858 (rw/math/vpu/matrix44affine_operation.h) with the 0.2 splat of flt_82004744.
// Through the stub the ICE-anim heading space snapped to its look-at every frame; the console eases it 20% a frame.
// ----------------------------------------------------------------------------

// GROUP F -- RETIRED 2026-09-24 (FX-DIRECTOR, the moment tick + factory). The stub that stood here -- a
// MomentController::NewMoment that allocated nothing, so every MomentHandle stayed !IsAllocated() and
// every MomentSelector counted 0 valid moments -- is GONE. The real NewMoment (and the corrected
// MomentHandle::Prepare) live in MomentController/BrnMomentController.cpp, their DWARF home, which is
// mounted; MainDirector ticks the moments (UpdateMoments @0x82250268, called from Update 0x82274348).
// [FX-DIRECTOR2 2026-09-25] All twelve types allocate now: TakedownLookback (case 3) and PlayerJumping
// (case 7) were gated INSIDE NewMoment until their closure was in the link (see NewMoment's banner).

