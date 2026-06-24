// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO_InputBuffer_PreScene.cpp
//
// Out-of-line bodies for the BrnWorld::WorldEntityIO::InputBuffer_PreScene accessors the X360
// ARTIST build emitted out-of-line. Each guarded accessor asserts the buffer's lock bit then
// touches a member:
//   read-lock  (status>>4 &1) => IsBufferLockedForReading()  (const getter)
//   write-lock (status>>3 &1) => IsBufferLockedForWriting()  (mutators)
// Member offsets are layout-derived (member-by-name / &member), not hardcoded. CGS_ASSERT stamps
// __FILE__/__LINE__, so the X360-baked d:\p4 path/lines (97/99/100) are not reproduced.
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof
#include <cstring>   // memcpy (the X360 XMemCpy block copy)

namespace BrnWorld
{
namespace WorldEntityIO
{
    void InputBuffer_PreScene::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PreScene, mActiveRaceCarInterface) == 16,
                      "mActiveRaceCarInterface @16");
        static_assert(offsetof(InputBuffer_PreScene, mRequestInterface) == 10496,
                      "mRequestInterface @10496 (16 + 10480)");
        static_assert(sizeof(InputBuffer_PreScene::ActiveRaceCarInterfaceStorage) == 10480,
                      "ActiveRaceCarInterface payload is 10480 bytes");
    }

    // X360 0x822BA2D0 (R, :100) -- read-lock; return &mRequestInterface (this + 10496).
    const RequestInterface* InputBuffer_PreScene::GetRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mRequestInterface;
    }

    // X360 0x827A2888 (W, :99) -- write-lock; merge lrOther into mRequestInterface
    // (RequestInterface::Append OR-merges the two collision-world request flags).
    void InputBuffer_PreScene::AppendRequestInterface(const RequestInterface& lrOther)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        mRequestInterface.Append(lrOther);
    }

    // X360 0x827A27D0 (W, :97) -- write-lock; block-copy the 10480-byte race-car input payload
    // into mActiveRaceCarInterface (this + 16). The X360 XMemCpy(this+16, src, 0x28F0) is a plain
    // 10480-byte block copy.
    void InputBuffer_PreScene::SetActiveRaceCarInterface(const ActiveRaceCarInterfaceStorage& lrInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        std::memcpy(&mActiveRaceCarInterface, &lrInterface, sizeof(ActiveRaceCarInterfaceStorage));
    }
}
}
