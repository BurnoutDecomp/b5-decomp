#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags

// BrnPhysics::Deformation::DeformableObject -- the IK-skinning / locator slice.
// ⭐ TU CREATED 2026-08-14 (deformation-mount wave), exactly as the walls-wave census prescribed
// ("the _Update.cpp UpdateLocators leg via an IKSkinning slice"): the deformation-manager mount
// brings BrnDeformableObject_Update.cpp with it, whose UpdateLocators loops call the per-locator
// UpdateLocator @0x825E0EC8 (204 instr; PS3 twin 0x7A8498) -- a body that is STILL NOT
// RECONSTRUCTED (it re-derives each generic/light/camera locator transform from its tag points /
// parent part pose). This slice holds its honest seam until that body lands here.
//
// ⚠️ DEAD AT RUNTIME this wave: DeformableObject::Update (UpdateLocators' only caller) is only
// reached from DeformationManager::Update @0x82649B40, which is still a conductor gate. The gate
// below exists for LINK closure and to be LOUD if the update path goes live before the locator
// body lands. Locators simply keep their PrepareLocators rest transforms.

namespace BrnPhysics
{
namespace Deformation
{
    // UpdateLocator @0x825E0EC8 (204; PS3 0x7A8498) -- log-once gate, NOT reconstructed. The
    // locator transform/type slots are left untouched (their PrepareLocators rest pose persists);
    // nothing is fabricated.
    void DeformableObject::UpdateLocator(Matrix44Affine& /*lrTransform*/, ETagPointType& /*lreType*/,
                                         const LocatorPointSpec* /*lpSpec*/,
                                         const rw::math::vpu::Matrix44Affine& /*lrParent*/,
                                         DetachedPartManager* /*lpPartMgr*/)
    {
        static bool s_bLoggedUpdateLocatorGate = false;
        if (!s_bLoggedUpdateLocatorGate)
        {
            s_bLoggedUpdateLocatorGate = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "conductor gate: DeformableObject::UpdateLocator @0x825E0EC8 reached but "
                       "not reconstructed -- locator transforms keep their rest pose [FLAG PC "
                       "boot gate]. Reported once, not per frame\n";
        }
    }
}
}
