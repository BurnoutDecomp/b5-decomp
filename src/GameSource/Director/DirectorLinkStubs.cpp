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
// GROUP C -- sub-systems with no landed TU (the director DebugComponent, the
//   scene-query post-office free functions).
// GROUP D -- the vendor rw SLerp leaf.
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

    // -- The two scene-query post-office free functions BrnSceneQueryInterface.h declares.
    //    OutEventVolumeTestDeepest mints the 16-bit query id for a staged volume test; 0 is
    //    the console's "no id" value and SceneQueryInterface treats it as a failed post.
    //    sub_8221CC98 resets slot 1's post office and returns a pointer into it.
    //    DELETE-WHEN: the two post-office TUs land.
    u32* sub_8221CC98(void* lpSlot)
    {
        return static_cast<u32*>(lpSlot);
    }
}

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    u32 OutEventVolumeTestDeepest(void* lpPostOffice, void* lpQueryParams)
    {
        (void)lpPostOffice;
        (void)lpQueryParams;
        return 0u;
    }
}
}

// ----------------------------------------------------------------------------
// GROUP D -- the vendor leaf with no reconstructed body.
// ----------------------------------------------------------------------------
namespace rw
{
namespace math
{
namespace vpu
{
    // The vendor affine-matrix SLerp (declared in rw/math/vpu/matrix44affine_operation.h,
    // body owned by the SDK and not reconstructed). ONE caller reaches it:
    // InertiaController::Update, and only on the branch where the camera has
    // requested LAG (`1 - CameraEffects::mfCameraLag < 1`, i.e. lag > 0). With no lag the
    // console returns before the call, which is the state on every frame of this build.
    // The stub therefore returns lrTo unchanged -- exactly the t == 1 endpoint, i.e. "adopt
    // the freshly-finalised transform", which is what the no-lag path already does. It is the
    // inert answer, not an approximation of the interpolation.
    // DELETE-WHEN: the vendor op is reconstructed (then camera lag starts working).
    Matrix44Affine SLerp(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo,
                         const float* lpafBlend)
    {
        (void)lrFrom;
        (void)lpafBlend;
        return lrTo;
    }
}
}
}

// GROUP F -- RETIRED 2026-09-24 (FX-DIRECTOR, the moment tick + factory). The stub that stood here -- a
// MomentController::NewMoment that allocated nothing, so every MomentHandle stayed !IsAllocated() and
// every MomentSelector counted 0 valid moments -- is GONE. The real NewMoment (and the corrected
// MomentHandle::Prepare) live in MomentController/BrnMomentController.cpp, their DWARF home, which is
// mounted; MainDirector ticks the moments (UpdateMoments @0x82250268, called from Update 0x82274348).
// Two of the twelve types -- TakedownLookback and PlayerJumping -- stay gated INSIDE NewMoment until
// their closure (BehaviourRig + CameraRig::Construct, the BehaviourCollection<> bodies,
// MomentPlayerJumping::UpdateCamera, the look-back's reach shims) is in the link.

