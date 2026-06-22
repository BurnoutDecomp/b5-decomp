// Tiny embed/usage check for this group's three homes:
//   - BrnPhysics::Deformation::WheelPhysicalStates::operator=
//   - CgsSound::Utils::IntClamp  (+ CgsNumeric::Max / Min inline)
//   - CgsSound::Playback::Handle<Factory>::operator*
// Exercises each bodied function so the gate sees them instantiated together.

#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"
#include "GameShared/GameClasses/Numeric/CgsBranchlessOperations.h"
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"

namespace CgsSound
{
namespace Utils
{
    s32 IntClamp(s32 liValue, s32 liLow, s32 liHigh);
}
}

namespace
{

// Concrete-enough Factory stand-in so Handle<Factory>::operator* instantiates.
struct FactoryStub
{
    int miValue;
};

// Force instantiation of the inline dereference (both overloads) without needing
// the declared-only ctor: taking the member-function pointer instantiates the body.
typedef CgsSound::Playback::Handle<FactoryStub> StubHandle;
FactoryStub&       (StubHandle::*gpfDeref)()       = &StubHandle::operator*;
const FactoryStub& (StubHandle::*gpfDerefConst)() const = &StubHandle::operator*;

void EmbedCheck()
{
    // WheelPhysicalStates copy-assignment.
    BrnPhysics::Deformation::WheelPhysicalStates lA;
    BrnPhysics::Deformation::WheelPhysicalStates lB;
    lB = lA;
    (void)lB;

    // Branchless Max/Min via IntClamp, and the helpers directly.
    volatile s32 liClamped = CgsSound::Utils::IntClamp(5, 0, 10);
    volatile s32 liMax     = CgsNumeric::Max(3, 7);
    volatile s32 liMin     = CgsNumeric::Min(3, 7);
    volatile u32 luMin     = CgsNumeric::Min(static_cast<u32>(3), static_cast<u32>(7));
    (void)liClamped; (void)liMax; (void)liMin; (void)luMin;

    (void)gpfDeref;
    (void)gpfDerefConst;
}

} // namespace
