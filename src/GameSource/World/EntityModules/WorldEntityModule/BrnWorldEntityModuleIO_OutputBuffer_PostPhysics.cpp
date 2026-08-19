#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX (DWARF BrnWorldEntityModuleIO.h:234 struct). RECONCILED: the
// previously-committed Get()/mOutputPayload accessor is really the write-lock
// GetStatusInterface() (DWARF :249) returning mStatusInterface (:259, this + 822896).
// This TU now bodies all three X360-emitted accessors:
//
//   GetResourceRequestInterface() @ 0x822BA810  write-lock (bit 3) -> &mResourceRequestInterface (this + 4)      [DWARF :246]
//   GetStatusInterface() const    @ 0x827A2E78  read-lock  (bit 4) -> &mStatusInterface (this + 822896)          [DWARF :248]
//   GetStatusInterface()          @ 0x822BA8B8  write-lock (bit 3) -> &mStatusInterface (this + 822896)          [DWARF :249]
//   GetSceneInputInterface()      @ 0x822BA960  write-lock (bit 3) -> &mSceneInputInterface (this + 0x1020 == 4128)
//   GetSceneInputInterface() const@ 0x827A2F20  read-lock  (bit 4) -> &mSceneInputInterface (this + 0x1020 == 4128)  [added 2026-08-19]
//
// The read getter tests bit 4 (`extrwi r11,r11,1,27`), the writers test bit 3
// (`extrwi r11,r11,1,28`). The +822896 (0xC8E70) member address is computed by the asm as
// `addis r3,this,0xD; addi r3,r3,-0x7190`. The streamed "\n" is dropped from the stringized
// condition, as in the sibling OutputBuffer_Prepare accessor.

namespace BrnWorld
{
namespace WorldEntityIO
{
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        // X360 32-bit byte offsets retired 2026-07-24: the buffer now holds the real
        // typed members (several contain host pointers), so per the x64 gate the
        // layout is semantic-by-NAME; the X360 offsets live as comments in the header.
    }

    // X360 0x822BA810: write-lock; return this + 4.
    ResourceRequestInterface*
    OutputBuffer_PostPhysics::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mResourceRequestInterface;
    }

    // Read-lock const twin (WorldModule::BridgeEntityModulesToOutput_PostPhysics
    // @0x827AEEB0 drains the streamer's staged requests through it).
    const ResourceRequestInterface*
    OutputBuffer_PostPhysics::GetResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mResourceRequestInterface;
    }

    // X360 0x827A2E78: read-lock; return this + 822896.
    const StatusInterface*
    OutputBuffer_PostPhysics::GetStatusInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mStatusInterface;
    }

    // X360 0x822BA8B8: write-lock; return this + 822896.
    StatusInterface*
    OutputBuffer_PostPhysics::GetStatusInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mStatusInterface;
    }

    // X360 0x822BA960: write-lock; return &mSceneInputInterface (X360 this + 0x1020 == 4128).
    // Called by BrnWorld::WorldEntityModule::UpdateCollisionValidation.
    SceneInputInterface*
    OutputBuffer_PostPhysics::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }

    // ⭐ X360 0x827A2F20: READ-lock; return &mSceneInputInterface (X360 this + 0x1020 == 4128).
    // BODIED 2026-08-19 (wave Q5 cluster F3) -- this overload had never been modelled, and its
    // absence is what parked the post-physics world-entity leg of
    // WorldModule::BridgeEntityModulesToScene_PostPhysics @0x827AB608. That leg's FLAG said the
    // getter "guards on the WRITE lock while the bridge runs read-locked (the X360 orders its
    // locks differently across the post-physics spine)". THAT WAS A HYPOTHESIS AND IT IS FALSE:
    // the console emits BOTH overloads for this member, the bridge calls the read one, and the
    // lock order is the ordinary LockBuffersForIO(dest-for-write, sources-for-read).
    // Everything below is off the raw asm (per-address JSON 0x827A2F20, re-dumped this cluster):
    //   * `lbz r11,0(r28); extrwi r11,r11,1,27` -- bit 4 (eStatusLockedForRead) =>
    //     IsBufferLockedForReading(), i.e. the CONST overload (its writer twin 0x822BA960
    //     tests bit 3, `extrwi r11,r11,1,28`);
    //   * the baked assert cites BrnWorldEntityModuleIO.h line 0xFB == 251, three lines BELOW
    //     the read GetStatusInterface (0x827A2E78, baked line 0xF8 == 248) on the same ladder,
    //     which is what the recovered ladder 245 -> +4 / 248 -> +822896 / 251 -> +4128 shows,
    //     and consistent with the member roll-call :257/:258/:259 below;
    //   * epilogue `addi r3, r28, 0x1020` == this + 4128 -- the same displacement the writer
    //     returns and the one the header records for mSceneInputInterface. Returned BY NAME.
    // Caller: WorldModule::BridgeEntityModulesToScene_PostPhysics (`bl sub_827A2F20` @0x827AB718,
    // the fourth and last Append leg).
    const SceneInputInterface*
    OutputBuffer_PostPhysics::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneInputInterface;
    }
}
}
