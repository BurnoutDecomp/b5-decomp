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
        static_assert(offsetof(OutputBuffer_PreScene, mPropGraphicsLoadedQueue) == 76,
                      "mPropGraphicsLoadedQueue @76");
        static_assert(offsetof(OutputBuffer_PreScene, mPropGraphicsUnloadedQueue) == 140,
                      "mPropGraphicsUnloadedQueue @140");
        static_assert(offsetof(OutputBuffer_PreScene, mSceneInputInterface) == 208,
                      "mSceneInputInterface @208");
        static_assert(offsetof(OutputBuffer_PreScene, mGameEventQueue) == 818976,
                      "mGameEventQueue @818976");
        static_assert(offsetof(OutputBuffer_PreScene, mSoundWorldLoadInterface) == 820528,
                      "mSoundWorldLoadInterface @820528");
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

    // X360 0x822BA4C8: write-lock; return &mPropGraphicsLoadedQueue (this + 76). DWARF
    // BrnWorldEntityModuleIO.h:127 (non-const). Called by
    // BrnWorld::WorldEntityModule::OnWorldGraphicsLoadComplete.
    OutputBuffer_PreScene::PropGraphicsLoadedQueueStorage*
    OutputBuffer_PreScene::GetPropGraphicsLoadedQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropGraphicsLoadedQueue;
    }

    // X360 0x822BA618: write-lock; return &mPropGraphicsUnloadedQueue (this + 140). DWARF
    // BrnWorldEntityModuleIO.h:133 (non-const). Called by
    // BrnWorld::WorldEntityModule::OnWorldGraphicsUnloadBegin.
    OutputBuffer_PreScene::PropGraphicsUnloadedQueueStorage*
    OutputBuffer_PreScene::GetPropGraphicsUnloadedQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropGraphicsUnloadedQueue;
    }

    // X360 0x822BA6C0: write-lock; return &mSoundWorldLoadInterface (this + 820528). DWARF
    // BrnWorldEntityModuleIO.h:136 (non-const). The +820528 (0xC8570) member address is
    // computed by the asm as `addis r3,this,0xD; addi r3,r3,-0x7AD0`. Called by
    // BrnWorld::WorldEntityModule::OnWorldGraphicsLoadComplete and OnWorldGraphicsUnloadBegin.
    OutputBuffer_PreScene::SoundWorldLoadInterfaceStorage*
    OutputBuffer_PreScene::GetSoundWorldLoadInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSoundWorldLoadInterface;
    }
}
}
