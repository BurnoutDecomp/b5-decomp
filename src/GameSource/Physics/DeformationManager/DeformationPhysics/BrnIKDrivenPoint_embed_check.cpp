// Embed/translation-unit compile check for the Deformation group's owned homes:
// IKDrivenPoint + its minimal IKDrivenPointSpec slice + the DetachedPartRenderEvent payload
// and its EventQueue instantiation. Verifies the headers are self-contained and the owned
// types are embeddable by value (correct sizes/alignment) without pulling the .cpp bodies.

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnIKDrivenPoint.h"
#include "SharedClasses/Physics/Deformation/BrnIKDrivenPointSpec.h"
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationEvents.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"

namespace
{
    using namespace BrnPhysics::Deformation;

    // The render event must be exactly 80 bytes (stride the X360 AddEvent copies: 64-byte
    // Matrix44Affine + EntityId + s32 + bool, padded to 16).
    static_assert(sizeof(DetachedPartRenderEvent) == 80, "DetachedPartRenderEvent stride must be 80 (0x50)");
    static_assert(alignof(DetachedPartRenderEvent) == 16, "DetachedPartRenderEvent must be 16-aligned");

    // Embed-by-value smoke checks.
    struct EmbedCheck
    {
        IKDrivenPoint                                            mPoint;
        IKDrivenPointSpec                                        mSpec;
        CgsModule::EventQueue<DetachedPartRenderEvent, 50>       mRenderQueue;
    };

    static_assert(sizeof(EmbedCheck) > 0, "EmbedCheck must be a complete type");
}
