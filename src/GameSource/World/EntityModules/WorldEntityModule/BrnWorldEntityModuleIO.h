#pragma once

// Canonical (DWARF) home for the BrnWorld::WorldEntityIO world-entity IO buffers
// (BrnWorldEntityModuleIO.h). GROWN 2026-07-24 by the WorldEntityModule TU: the
// buffers' opaque foreign-type storage is now the real typed members (their homes
// have all landed), and the module-facing accessors are typed. X360 32-bit byte
// offsets are kept as COMMENTS (several members now hold host pointers, so the
// x64 layout departs; semantic parity is by NAME per the x64 gate).
//
// X360-emitted accessors homed here (addresses per body):
//   OutputBuffer_Prepare::GetResourceRequestInterface       @ 0x822BA180 (w) / 0x827A2728 (r)
//   OutputBuffer_PostPhysics::GetResourceRequestInterface   @ 0x822BA810 (w)
//   OutputBuffer_PostPhysics::GetSceneInputInterface        @ 0x822BA960 (w)
//   OutputBuffer_PostPhysics::GetStatusInterface            @ 0x827A2E78 (r) / 0x822BA8B8 (w)
//   InputBuffer_GenerateDispatchLists::Get/SetDispatchFrame @ 0x822BAA08 / 0x827A2FC8
//   InputBuffer_GenerateDispatchLists::Get/SetShadowMap     @ 0x822BAAB0 / 0x827A3070
//   InputBuffer_PreScene::GetActiveRaceCarInterface         @ 0x822BA228 (r)
//   InputBuffer_PreScene::GetRequestInterface               @ 0x822BA2D0 (r)
//   InputBuffer_PreScene::AppendRequestInterface            @ 0x827A2888 (w)
//   InputBuffer_PreScene::SetActiveRaceCarInterface         @ 0x827A27D0 (w)
//   InputBuffer_PostPhysics::GetGameActionQueue             @ 0x822BA768 (r) / 0x827A2D28 (w)
//   OutputBuffer_PreScene::GetSceneInputInterface           @ 0x827A2938 (r) / 0x822BA378 (w)
//   OutputBuffer_PreScene::GetGameEventQueue                @ 0x822BA420 (w)
//   OutputBuffer_PreScene::GetPropGraphicsLoadedQueue       @ 0x822BA4C8 (w)
//   OutputBuffer_PreScene::GetPropGraphicsUnloadedQueue     @ 0x822BA618 (w)
//   OutputBuffer_PreScene::GetPropInstancesNeededForZoneQueue (queue trio :146)
//   OutputBuffer_PreScene::GetSoundWorldLoadInterface       @ 0x822BA6C0 (w)
//   OutputBuffer_PreScene::SetPlayerZoneNumber              (miPlayerZoneNumber :152,
//     the X360 UpdateStream store @ buffer+821764)

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsEventQueue.h"          // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<N,A>
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // InSceneUpdateInterface
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h" // RequestInterface<4096>
#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityRequestInterface.h" // RequestInterface
#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityStatusInterface.h"  // StatusInterface
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropGraphicsAndZoneEvents.h"    // Prop* events
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface
#include "GameSource/Sound/Module/SharedIO/BrnSoundRootSharedIO.h" // BrnSound::Module::Io::SoundWorldLoadEvent

namespace CgsGraphics
{
    class DispatchFrame;    // pointer-only members below; class matches CgsDispatcher.h:211 (mangling)
}
namespace BrnWorld
{
    struct ShadowMap;       // pointer-only member below; struct matches BrnShadowMap.h:64 (mangling)
}

namespace BrnWorld
{
namespace WorldEntityIO
{
    // The world-entity resource-request pipe (DWARF :80 etc.).
    typedef BrnResource::GameDataIO::RequestInterface<4096> ResourceRequestInterface;
    // The scene-manager input interface the world entities are added through.
    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;

    // DWARF BrnWorldEntityModuleIO.h:61 — the world-entity "prepare" phase output buffer.
    struct OutputBuffer_Prepare : public CgsModule::IOBuffer
    {
        // 0x822BA180 — write-lock tripwire ("Not locked for writing"); X360 this+4.
        ResourceRequestInterface* GetResourceRequestInterface();
        // 0x827A2728 — read-lock tripwire ("Not locked for reading").
        const ResourceRequestInterface* GetResourceRequestInterface() const;

        // ATTESTED 2026-07-24 by WorldModule::Prepare @0x827D53B0 (stage-8 fail path
        // appends this buffer's staged scene requests into the live scene input).
        SceneInputInterface* GetSceneInputInterface();

        static void _AssertLayout();

    private:
        // X360 layout: 1-byte IOBuffer status, +1..+3 pad, member at +4.
        u8                       maStatusPad[3];
        ResourceRequestInterface mResourceRequestInterface;   // :80 (X360 +4)
        SceneInputInterface      mSceneInputInterface;        // :81 (attested by
                                                              // WorldModule::Prepare)
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics (DWARF BrnWorldEntityModuleIO.h:234).
    struct OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        // X360 0x822BA810: write-lock; X360 this+4.
        ResourceRequestInterface* GetResourceRequestInterface();
        // X360 0x822BA960: write-lock; the scene input interface (X360 this+4116).
        SceneInputInterface* GetSceneInputInterface();
        // X360 0x827A2E78: read-lock; X360 this+822896.
        const StatusInterface* GetStatusInterface() const;
        // X360 0x822BA8B8: write-lock; X360 this+822896.
        StatusInterface* GetStatusInterface();

        static void _AssertLayout();

    private:
        u8                       maStatusPad[3];
        ResourceRequestInterface mResourceRequestInterface;   // :257 (X360 +4)
        SceneInputInterface      mSceneInputInterface;        // :258 (X360 +4116)
        StatusInterface          mStatusInterface;            // :259 (X360 +822896)
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists
    // (DWARF BrnWorldEntityModuleIO.h:~278). The two payload fields are POINTERS
    // (the X360 stored them as 32-bit addresses; the consumers dereference them:
    // DispatchFrame::GetList / ShadowMap::CalcLodDistanceModifier).
    struct InputBuffer_GenerateDispatchLists : public CgsModule::IOBuffer
    {
        // ADDITIVE (WorldModule::GenerateDispatchLists @0x827D1CE8 seeds the
        // frustum-result event here before the world dispatch feed runs --
        // X360 accessor sub_827BBCF8, spelled GetSceneResultQueue like the
        // sibling race-car/traffic/prop dispatch inputs).
        CgsModule::VariableEventQueue<32768, 16>* GetSceneResultQueue();

        // X360 0x822BAA08 (read-lock) / 0x827A2FC8 (write-lock); X360 this+4.
        CgsGraphics::DispatchFrame* GetDispatchFrame() const;
        void SetDispatchFrame( CgsGraphics::DispatchFrame* lpDispatchFrame );
        // X360 0x822BAAB0 (read-lock) / 0x827A3070 (write-lock); X360 this+0x8018.
        BrnWorld::ShadowMap* GetShadowMap() const;
        void SetShadowMap( BrnWorld::ShadowMap* lpShadowMap );

        static void _AssertLayout();

    private:
        CgsGraphics::DispatchFrame* mpDispatchFrame;             // X360 +4
        // Intervening dispatch-list payload (own members not yet attested).
        unsigned char               maPayload[0x8018 - 8];       // X360 +8..+0x8017
        BrnWorld::ShadowMap*        mpShadowMap;                 // X360 +0x8018
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::InputBuffer_PreScene (DWARF BrnWorldEntityModuleIO.h:~95).
    struct InputBuffer_PreScene : public CgsModule::IOBuffer
    {
        typedef RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface ActiveRaceCarInterface;

        // X360 0x822BA228: read-lock; X360 this+16.
        const ActiveRaceCarInterface* GetActiveRaceCarInterface() const;
        // X360 0x822BA2D0: read-lock; X360 this+10496.
        const RequestInterface* GetRequestInterface() const;
        // X360 0x827A2888: write-lock; merges lrOther into mRequestInterface.
        void AppendRequestInterface( const RequestInterface& lrOther );
        // X360 0x827A27D0: write-lock; copies the race-car payload (X360 10480 bytes).
        void SetActiveRaceCarInterface( const ActiveRaceCarInterface& lrInterface );

        static void _AssertLayout();

    private:
        ActiveRaceCarInterface mActiveRaceCarInterface;   // :~96 (X360 +16, 10480B)
        RequestInterface       mRequestInterface;         // :~98 (X360 +10496)
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::InputBuffer_PostPhysics (DWARF BrnWorldEntityModuleIO.h:212).
    struct InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        // The GameAction event pipe (X360 VariableEventQueue<13312,16>).
        typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueue;

        // X360 0x822BA768: read-lock; X360 this+4.
        const GameActionQueue* GetGameActionQueue() const;
        // X360 0x827A2D28: write-lock; X360 this+4.
        GameActionQueue* GetGameActionQueue();

        static void _AssertLayout();

    private:
        u8              maStatusPad[3];
        GameActionQueue mGameActionQueue;   // :229 (X360 +4)
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::OutputBuffer_PreScene (DWARF BrnWorldEntityModuleIO.h:109).
    struct OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<BrnWorld::PropEntityIO::PropInstancesNeededForZoneEvent, 30> PropInstancesNeededForZoneQueue;
        typedef CgsModule::EventQueue<BrnWorld::PropEntityIO::PropGraphicsLoadedEvent, 25>         PropGraphicsLoadedQueue;
        typedef CgsModule::EventQueue<BrnWorld::PropEntityIO::PropGraphicsUnloadedEvent, 25>       PropGraphicsUnloadedQueue;
        typedef CgsModule::VariableEventQueue<1536, 16>                                            GameEventQueue;
        typedef CgsModule::EventQueue<BrnSound::Module::Io::SoundWorldLoadEvent, 25>               SoundWorldLoadInterface;

        // X360 0x827A2938 (read-lock) / 0x822BA378 (write-lock); X360 this+208.
        const SceneInputInterface* GetSceneInputInterface() const;
        SceneInputInterface* GetSceneInputInterface();
        // X360 0x822BA420: write-lock; X360 this+818976.
        GameEventQueue* GetGameEventQueue();
        // Queue trio (:146/:147/:148; X360 +16/+76/+140).
        PropInstancesNeededForZoneQueue* GetPropInstancesNeededForZoneQueue();
        // X360 0x822BA4C8: write-lock; X360 this+76.
        PropGraphicsLoadedQueue* GetPropGraphicsLoadedQueue();
        // X360 0x822BA618: write-lock; X360 this+140.
        PropGraphicsUnloadedQueue* GetPropGraphicsUnloadedQueue();
        // X360 0x822BA6C0: write-lock; X360 this+820528.
        SoundWorldLoadInterface* GetSoundWorldLoadInterface();
        // miPlayerZoneNumber :152 (the X360 UpdateStream store @ this+821764).
        void SetPlayerZoneNumber( s32 liPlayerZoneNumber );

        static void _AssertLayout();

    private:
        PropInstancesNeededForZoneQueue mPropInstancesNeededForZoneQueue; // :146 (X360 +16)
        PropGraphicsLoadedQueue         mPropGraphicsLoadedQueue;         // :147 (X360 +76)
        PropGraphicsUnloadedQueue       mPropGraphicsUnloadedQueue;       // :148 (X360 +140)
        SceneInputInterface             mSceneInputInterface;             // :149 (X360 +208)
        GameEventQueue                  mGameEventQueue;                  // :150 (X360 +818976)
        SoundWorldLoadInterface         mSoundWorldLoadInterface;         // :151 (X360 +820528)
        s32                             miPlayerZoneNumber;               // :152 (X360 +821764)
    };
}
}
