#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::WorldEntityIO::OutputBuffer_PreScene accessors, reconstructed from
// BURNOUT_X360_ARTIST.XEX (DWARF BrnWorldEntityModuleIO.h:109 struct). This TU bodies the
// three X360-emitted accessors of the world-entity pre-scene output buffer:
//
//   GetSceneInputInterface() const @ 0x827A2938 -> &mSceneInputInterface (this + 0xD0 == 208),
//                                                   asserts read-lock  (bit 4) [DWARF :120]
//   GetSceneInputInterface()       @ 0x822BA378 -> &mSceneInputInterface (this + 0xD0 == 208),
//                                                   asserts write-lock (bit 3) [DWARF :121]
//   GetGameEventQueue()            @ 0x822BA420 -> &mGameEventQueue (this + 0xC7F20 == 818976),
//                                                   asserts write-lock (bit 3) [DWARF :124/:150]
//
// The const getter tests the read-lock bit (`extrwi r11,r11,1,27` == bit 4); the mutators
// test the write-lock bit (`extrwi r11,r11,1,28` == bit 3). mSceneInputInterface sits at
// this + 208 (after the three prop-queue members); mGameEventQueue at this + 818976
// (asm: `addis r3,this,0xC; addi r3,r3,0x7F20`). The streamed "\n" is dropped from the
// stringized condition, as in the sibling OutputBuffer_Prepare accessor.

namespace BrnWorld
{
namespace WorldEntityIO
{
    void OutputBuffer_PreScene::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PreScene, mSceneInputInterface) == 208,
                      "mSceneInputInterface @208");
        static_assert(offsetof(OutputBuffer_PreScene, mGameEventQueue) == 818976,
                      "mGameEventQueue @818976");
    }

    // X360 0x827A2938: read-lock; return this + 208.
    const OutputBuffer_PreScene::SceneInputInterfaceStorage*
    OutputBuffer_PreScene::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mSceneInputInterface;
    }

    // X360 0x822BA378: write-lock; return this + 208.
    OutputBuffer_PreScene::SceneInputInterfaceStorage*
    OutputBuffer_PreScene::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }

    // X360 0x822BA420: write-lock; return &mGameEventQueue (this + 818976). Called by
    // BrnWorld::WorldEntityModule::UpdateStream.
    OutputBuffer_PreScene::GameEventQueueStorage* OutputBuffer_PreScene::GetGameEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mGameEventQueue;
    }
}
}
