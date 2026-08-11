// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h
//
// Canonical (DWARF) home for the BrnWorld::TriggerEntityModuleIO IO buffers
// (DWARF home BrnTriggerEntityModuleIO.h). It homes OutputBuffer_PreScene and InputBuffer_PreScene
// (the two whose out-of-line accessors the ARTIST build emitted) and -- ADDITIVELY GROWN for the
// TriggerEntityModule Process* TU -- minimal slices of the four remaining buffers
// (InputBuffer_PostScene / OutputBuffer_PostScene / InputBuffer_PrePhysics /
// OutputBuffer_PrePhysics). The Process* functions only ever take the embedded queue of these
// four buffers (by named getter), so each is modelled as its IOBuffer base + the single embedded
// queue its X360 accessor returns. Byte-exact layout is not required (all access is by-name).
//
// LAYOUT (DWARF :81/:94 + X360 Construct @0x822EED90, authoritative -- UNCHANGED):
//   OutputBuffer_PreScene : base IOBuffer + mSceneInputInterface @ +16 (alignas(16) member).
//   InputBuffer_PreScene  : base IOBuffer + mInputInterface (mgmt-input opaque storage) @ +4.
//
// DWARF queue typedefs for the four added buffers (reproduced by name):
//   InputBuffer_PostScene  : TriggerQueryInputInterface  == VariableEventQueue<4096,16>
//   OutputBuffer_PostScene : InFineQueryQueue<2048>      == VariableEventQueue<2048,16>
//   InputBuffer_PrePhysics : SceneResultQueue            == VariableEventQueue<32768,16>
//   OutputBuffer_PrePhysics: TriggerEntityModuleOutputInterface == VariableEventQueue<1024,16>
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                    // CgsModule::IOBuffer
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"  // InSceneUpdateInterface
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // TriggerManagementInputInterface

namespace BrnWorld
{
namespace TriggerEntityModuleIO
{
    // BrnTrigger...IO.h:81 -- the pre-scene OUTPUT buffer: the trigger entity module's
    // scene-update commands for the scene manager.
    class OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        // BrnTriggerEntityModuleIO.h:40 typedef -- the scene input interface aggregate.
        typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;

        // BrnTriggerEntityModuleIO.h:84 -- this TU. @ X360 0x822EED90.
        void Construct();

        // BrnTriggerEntityModuleIO.h:87 / :90 / :91 -- declared-only (own TUs / sibling group).
        void Destruct();
        const SceneInputInterface* GetSceneInputInterface() const;
        SceneInputInterface*       GetSceneInputInterface();

        static void _AssertLayout();

    private:
        SceneInputInterface mSceneInputInterface;   // :94  (+16, alignas(16))
    };

    // ========================================================================
    // BrnWorld::TriggerEntityModuleIO::InputBuffer_PreScene (DWARF BrnTriggerEntityModuleIO.h:~116).
    // Homes the single accessor the X360 emitted out-of-line for the trigger entity module's
    // pre-scene INPUT buffer (the buffer the GUI->world BridgeInputToEntityModules fills with the
    // frame's trigger-management input):
    //   GetInputInterface() @ 0x827A3270  write-lock (bit 3) -> &member(this+4)  (asm-line 119)
    //
    // The getter tests the write-lock bit (`lbz r11,0(this); extrwi r11,r11,1,28` == bit 3 ==
    // IsBufferLockedForWriting()) and on failure fires "Not locked for writing\n"; it then returns
    // `this + 4` (`addi r3,this,4`) -- the address of the embedded input-interface member.
    //
    // LAYOUT (X360 getter return-offset, authoritative):
    //   base  CgsModule::IOBuffer       (1-byte status; +1..+3 pad)
    //   +4    InputInterface mInputInterface   (trigger-management input aggregate) :~118
    //
    // mInputInterface IS the trigger-management input aggregate (its home,
    // BrnTriggerEntityModuleInputInterface.h, is now committed). InputBuffer_PreScene::Construct
    // @0x822EED48 proves it byte-for-byte:
    //     *this = 1;
    //     VariableEventQueue<131072,16>::Construct(this + 4);
    //     InRemoveTriggerEvent_256_::Construct(this + 4 + 131088);
    // i.e. the aggregate's own two embedded queues at its own two offsets, based at this+4.
    // (Was `struct InputInterfaceStorage { unsigned char maBytes[1]; }` -- 1 byte of storage
    // that every consumer reinterpret_cast to the 131 KB aggregate and appended into.)
    class InputBuffer_PreScene : public CgsModule::IOBuffer
    {
    public:
        typedef TriggerManagementInputInterface InputInterface;   // DWARF :~118

        // X360 0x822EED48 -- IOBuffer status + the aggregate's two embedded queues.
        void Construct();

        // X360 0x827A3270: write-lock handle, returns &mInputInterface (this + 4). Body committed
        // in BrnTriggerEntityModuleIO_Accessors.cpp.
        InputInterface* GetInputInterface();
        // X360 0x822BCFE0: read-lock const overload, returns &mInputInterface (this + 4). Body in
        // BrnTriggerEntityModuleIO_QueueAccessors.cpp.
        const InputInterface* GetInputInterface() const;

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 places mInputInterface at this+4.
        // NOTE (2026-08-11): that +4 is a 32-BIT-POINTER offset. mInputInterface's leading
        // VariableEventQueue<131072,16> starts with an event pointer, so on this x64 host
        // alignof(InputInterface) == 8 and the member lands at +8 whatever this pad says --
        // the pad is kept only to mirror the console's explicit +1..+3 gap. Access is by name;
        // see the corrected _AssertLayout in BrnTriggerEntityModuleIO_Accessors.cpp.
        u8             maStatusPad[3];      // +1..+3 (console: forces +4)
        InputInterface mInputInterface;     // +4
    };

    // ========================================================================
    // ADDITIVE GROW (TriggerEntityModule Process* TU) -- the four remaining IO buffers as minimal
    // slices. Each is its IOBuffer base + the single embedded queue its X360 accessor returns.
    // FLAG: layout is by-named-getter only (not byte-exact); full layout belongs to each buffer's
    // own TU. None of these four has a committed .cpp, so adding them here is purely additive.

    // The trigger-query input queue (post-scene). == WorldModuleIO's TriggerQueryInputInterface.
    typedef CgsModule::VariableEventQueue<4096, 16>   TriggerQueryQueue;
    // The scene-manager fine-line-test request queue (post-scene OUTPUT). == InFineQueryQueue<2048>.
    typedef CgsModule::VariableEventQueue<2048, 16>   SceneFineQueryQueue;
    // The shared scene-result queue (pre-physics INPUT). == OutSmSceneQueryResultsQueue.
    typedef CgsModule::VariableEventQueue<32768, 16>  SceneResultQueue;
    // The trigger-overlap output queue (pre-physics OUTPUT). == TriggerEntityModuleOutputInterface.
    typedef CgsModule::VariableEventQueue<1024, 16>   TriggerEntityModuleOutputInterface;

    // Post-scene INPUT: carries the frame's trigger-query events (car line queries).
    class InputBuffer_PostScene : public CgsModule::IOBuffer
    {
    public:
        // Both overloads emitted out-of-line by the X360 build (const 0x822BCE90 read-lock,
        // non-const 0x827A3120 write-lock); bodies in BrnTriggerEntityModuleIO_QueueAccessors.cpp.
        const TriggerQueryQueue* GetQueryInputInterface() const;
        TriggerQueryQueue*       GetQueryInputInterface();
        // X360 0x822DA168: *this = 1 then VariableEventQueue<4096,16>::Construct(this+4).
        void Construct();
    private:
        TriggerQueryQueue mQueryInputInterface;
    };

    // Post-scene OUTPUT: the scene-manager fine-line-test request queue.
    class OutputBuffer_PostScene : public CgsModule::IOBuffer
    {
    public:
        const SceneFineQueryQueue* GetSceneFineQueryQueue() const { return &mSceneFineQueryQueue; }
        SceneFineQueryQueue*       GetSceneFineQueryQueue()       { return &mSceneFineQueryQueue; }
        // X360 0x822DA180: *this = 1 then VariableEventQueue<2048,16>::Construct(this+4).
        void Construct();
    private:
        SceneFineQueryQueue mSceneFineQueryQueue;
    };

    // Pre-physics INPUT: the shared scene-result queue (fine line-test results).
    class InputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
    public:
        // The const overload is emitted out-of-line by the X360 build (0x822BD130, read-lock);
        // body in BrnTriggerEntityModuleIO_QueueAccessors.cpp. The non-const stays inline.
        const SceneResultQueue* GetSceneResultQueue() const;
        SceneResultQueue*       GetSceneResultQueue()       { return &mSceneResultQueue; }
        // X360 0x822DA198: *this = 1 then VariableEventQueue<32768,16>::Construct(this+4).
        void Construct();
    private:
        SceneResultQueue mSceneResultQueue;
    };

    // Pre-physics OUTPUT: the trigger-overlap output queue (published to the trigger-query mgr).
    class OutputBuffer_PrePhysics : public CgsModule::IOBuffer
    {
    public:
        // Both overloads emitted out-of-line by the X360 build (const 0x827A3468 read-lock,
        // non-const 0x822BD1D8 write-lock); bodies in BrnTriggerEntityModuleIO_QueueAccessors.cpp.
        const TriggerEntityModuleOutputInterface* GetOutputInterface() const;
        TriggerEntityModuleOutputInterface*       GetOutputInterface();
        // X360 0x822DA1B0: *this = 1, VariableEventQueue<1024,16>::Construct(this+4) then
        // ::Clear on the same queue.
        void Construct();
    private:
        TriggerEntityModuleOutputInterface mOutputInterface;
    };
}
}
