#pragma once

// Canonical (DWARF) home for the BrnWorld::WorldEntityIO world-entity IO buffers
// (BrnWorldEntityModuleIO.h). This is a MINIMAL-COMPLETE slice: it currently homes only
// BrnWorld::WorldEntityIO::OutputBuffer_Prepare, scoped to the single X360-emitted
// accessor this group owns:
//   GetResourceRequestInterface()  @ 0x822BA180  write-lock (bit 3) -> this + 4  (DWARF :73)
//
// The const overload, the SceneInputInterface accessors, and the sibling prepare/scene/
// physics buffers (DWARF BrnWorldEntityModuleIO.h:61+) are NOT reconstructed here yet;
// they have their own owning homes / slices.
//
// LAYOUT (DWARF :61 member order + the X360 getter return-offset, authoritative):
//   base  CgsModule::IOBuffer                                  (1-byte status; +1..+3 pad)
//   +4    ResourceRequestInterface mResourceRequestInterface   (RequestInterface<4096>) :80
//   +...  SceneInputInterface      mSceneInputInterface        (InSceneUpdateInterface)  :81
//
// FLAG (foreign type): mResourceRequestInterface (a RequestInterface<4096>) and
// mSceneInputInterface have their own owning homes elsewhere and are NOT reconstructed
// here. The +4 member is modelled as correctly-positioned opaque storage so the single
// X360-pinned return offset (this + 4) is exact; the trailing scene-interface bytes are
// not separately recoverable from this slice and are omitted (this buffer is only
// constructed/locked elsewhere, never sized by this TU).
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityRequestInterface.h" // RequestInterface

namespace BrnWorld
{
namespace WorldEntityIO
{
    // DWARF BrnWorldEntityModuleIO.h:61 — the world-entity "prepare" phase output buffer.
    struct OutputBuffer_Prepare : public CgsModule::IOBuffer
    {
        // Foreign type (RequestInterface<4096>): own home elsewhere. Modelled as opaque
        // storage so the X360 +4 return offset is exact. First byte at this+4.
        struct ResourceRequestInterfaceStorage { unsigned char maBytes[1]; };

        // 0x822BA180 — write-lock tripwire ("Not locked for writing"); return this + 4.
        ResourceRequestInterfaceStorage* GetResourceRequestInterface();

        // 0x827A2728 — read-lock tripwire ("Not locked for reading"); return this + 4.
        // DWARF BrnWorldEntityModuleIO.h:72 — the const overload of the accessor above.
        const ResourceRequestInterfaceStorage* GetResourceRequestInterface() const;

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 build places
        // mResourceRequestInterface at this + 4, so pad bytes +1..+3 explicitly (the
        // 1-byte storage would otherwise pack at +1). Matches the sibling
        // PropEntityIO::OutputBuffer_PreScene layout.
        u8                              maStatusPad[3];              // +1..+3 (force +4)
        ResourceRequestInterfaceStorage mResourceRequestInterface;   // :80, at offset +4
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::OutputBuffer_PostPhysics (DWARF BrnWorldEntityModuleIO.h:234).
    // ADDITIVE GROW + RECONCILE: this slice homes the three X360-emitted accessors of the
    // post-physics output buffer. RECONCILED: the previously-committed Get()/mOutputPayload
    // accessor is really the write-lock GetStatusInterface() (DWARF :249) returning
    // mStatusInterface (:259, this + 822896):
    //   GetResourceRequestInterface() @ 0x822BA810  write-lock (bit 3) -> &mResourceRequestInterface (this + 4)      (DWARF :246)
    //   GetStatusInterface() const    @ 0x827A2E78  read-lock  (bit 4) -> &mStatusInterface (this + 822896)          (DWARF :248)
    //   GetStatusInterface()          @ 0x822BA8B8  write-lock (bit 3) -> &mStatusInterface (this + 822896)          (DWARF :249)
    //
    // The read getter tests bit 4 (`extrwi r11,r11,1,27`), the writers test bit 3
    // (`extrwi r11,r11,1,28`). The +822896 (0xC8E70) member address is computed by the asm as
    // `addis r3,this,0xD; addi r3,r3,-0x7190`.
    //
    // LAYOUT (X360 getter return-offsets, authoritative):
    //   base    CgsModule::IOBuffer                 (1-byte status; +1..+3 pad)
    //   +4      ResourceRequestInterface mResourceRequestInterface  (RequestInterface<4096>) :257
    //   +...    (mSceneInputInterface :258 + rest; opaque interior)
    //   +822896 StatusInterface          mStatusInterface           :259
    //
    // FLAG (foreign type): mResourceRequestInterface and mStatusInterface have their own owning
    // homes elsewhere and are NOT reconstructed here; both are modelled as correctly-positioned
    // opaque storage so the X360-pinned return offsets (this + 4, this + 822896) are exact. The
    // region between them (mSceneInputInterface + the rest) is honest opaque padding.
    struct OutputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        // Opaque foreign-type storage (see FLAG above).
        struct ResourceRequestInterfaceStorage { unsigned char maBytes[1]; };
        struct StatusInterfaceStorage          { unsigned char maBytes[1]; };

        // X360 0x822BA810: write-lock tripwire ("Not locked for writing"); return this + 4.
        ResourceRequestInterfaceStorage* GetResourceRequestInterface();
        // X360 0x827A2E78: read-lock tripwire ("Not locked for reading"); return this + 822896.
        const StatusInterfaceStorage* GetStatusInterface() const;
        // X360 0x822BA8B8: write-lock tripwire ("Not locked for writing"); return this + 822896.
        StatusInterfaceStorage* GetStatusInterface();

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 build places
        // mResourceRequestInterface at this + 4, so pad bytes +1..+3 explicitly.
        u8                              maStatusPad[3];                  // +1..+3 (force +4)
        ResourceRequestInterfaceStorage mResourceRequestInterface;       // +4      :257
        // Intervening payload (mSceneInputInterface :258 + rest; opaque). Spans +5 up to the
        // +822896 start of mStatusInterface.
        unsigned char                   maInterior[822896 - 4 - 1];      // +5..+822895
        StatusInterfaceStorage          mStatusInterface;                // +822896 :259
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::InputBuffer_GenerateDispatchLists
    // (DWARF BrnWorldEntityModuleIO.h:~278). ADDITIVE GROW: homes the four scalar
    // accessors the X360 emitted out-of-line for the generate-dispatch-lists input buffer.
    //   GetDispatchFrame() const @ 0x822BAA08  read-lock  (bit 4) -> u32 *(this+4)      (asm-line 278)
    //   SetDispatchFrame(u32)    @ 0x827A2FC8  write-lock (bit 3) ->     *(this+4) = v   (asm-line 279)
    //   GetShadowMap() const     @ 0x822BAAB0  read-lock  (bit 4) -> u32 *(this+0x8018)  (asm-line 281)
    //   SetShadowMap(u32)        @ 0x827A3070  write-lock (bit 3) ->     *(this+0x8018)=v (asm-line 282)
    //
    // The getters test the read-lock bit (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4 ==
    // IsBufferLockedForReading()) and fire "Not locked for reading\n"; the setters test the
    // write-lock bit (`extrwi r11,r11,1,28` == bit 3 == IsBufferLockedForWriting()) and fire
    // "Not locked for writing\n". Both accessed fields are 32-bit words (`lwz`/`stw` /
    // `lwzx`/`stwx`): the dispatch-frame index at this+4 and the shadow-map handle at
    // this+0x8018.
    //
    // LAYOUT (X360 accessor offsets, authoritative):
    //   base    CgsModule::IOBuffer            (1-byte status; +1..+3 pad)
    //   +4      u32 muDispatchFrame            dispatch-frame index
    //   +...    (intervening payload region; not recovered by this slice)
    //   +0x8018 u32 muShadowMap               shadow-map handle
    //
    // FLAG (opaque interior): the region between muDispatchFrame (+4) and muShadowMap (+0x8018)
    // is the buffer's other dispatch-list payload (its own members are not recovered by this
    // slice) and is modelled as correctly-sized opaque storage so the two X360-pinned accessor
    // offsets (+4, +0x8018) are exact.
    struct InputBuffer_GenerateDispatchLists : public CgsModule::IOBuffer
    {
        // X360 0x822BAA08: read-lock; return the dispatch-frame index (this+4).
        u32  GetDispatchFrame() const;
        // X360 0x827A2FC8: write-lock; set the dispatch-frame index (this+4).
        void SetDispatchFrame(u32 luDispatchFrame);
        // X360 0x822BAAB0: read-lock; return the shadow-map handle (this+0x8018).
        u32  GetShadowMap() const;
        // X360 0x827A3070: write-lock; set the shadow-map handle (this+0x8018).
        void SetShadowMap(u32 luShadowMap);

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 places muDispatchFrame at this+4,
        // so pad bytes +1..+3 explicitly.
        u8            maStatusPad[3];                     // +1..+3 (force +4)
        u32           muDispatchFrame;                    // +4
        // Intervening dispatch-list payload (opaque; see FLAG). Spans from +8 up to the
        // +0x8018 start of muShadowMap.
        unsigned char maPayloadAndPad[0x8018 - 8];        // +8..+0x8017
        u32           muShadowMap;                        // +0x8018
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::InputBuffer_PreScene (DWARF BrnWorldEntityModuleIO.h:~95).
    // ADDITIVE GROW: homes the three accessors the X360 emitted out-of-line for the
    // world-entity module's pre-scene input buffer (the buffer the race-car module's
    // BridgeRaceCarModuleToWorldModule_PreScene + the GUI->world BridgeInputToEntityModules
    // fill, and BrnWorld::WorldEntityModule::PreSceneUpdate drains):
    //   GetRequestInterface() const  @ 0x822BA2D0  read-lock  (bit 4) -> &member(this+10496) (asm-line 100)
    //   AppendRequestInterface(...)  @ 0x827A2888  write-lock (bit 3) -> mRequestInterface.Append(rOther) (asm-line 99)
    //   SetActiveRaceCarInterface(.) @ 0x827A27D0  write-lock (bit 3) -> XMemCpy(this+16, src, 10480) (asm-line 97)
    //
    // The const getter tests the read-lock bit (`lbz r11,0(this); extrwi r11,r11,1,27` == bit 4 ==
    // IsBufferLockedForReading()) and fires "Not locked for reading\n"; the two mutators test the
    // write-lock bit (`extrwi r11,r11,1,28` == bit 3 == IsBufferLockedForWriting()) and fire
    // "Not locked for writing\n". After the tripwire:
    //   - GetRequestInterface returns `this + 10496` (0x2900) -- the address of mRequestInterface.
    //   - AppendRequestInterface tail-calls RequestInterface::Append on `this + 10496` with the
    //     caller's RequestInterface (the OR-merge of the two collision-world request flags).
    //   - SetActiveRaceCarInterface block-copies 10480 (0x28F0) bytes from the source into the
    //     member at `this + 16` (`li r5,0x28F0; addi r3,this,0x10; bl XMemCpy`).
    //
    // LAYOUT (X360 accessor offsets, authoritative):
    //   base    CgsModule::IOBuffer            (1-byte status; +1..+15 pad to the 16-byte member)
    //   +16     ActiveRaceCarInterface mActiveRaceCarInterface   (10480-byte foreign payload) :~96
    //   +10496  RequestInterface       mRequestInterface         (collision-world request flags) :~98
    //
    // FLAG (foreign type): mActiveRaceCarInterface is a foreign per-race-car input payload whose
    // own home lands elsewhere; it is modelled as correctly-sized opaque storage (the X360 only
    // block-copies it as 10480 bytes, never naming its interior) so the +16 member start and the
    // derived +10496 RequestInterface offset are exact. mRequestInterface is the committed 2-byte
    // BrnWorld::WorldEntityIO::RequestInterface (home BrnWorldEntityRequestInterface.h); its
    // Append is reused, not redefined.
    struct InputBuffer_PreScene : public CgsModule::IOBuffer
    {
        // Foreign per-race-car input payload (see FLAG): the 10480-byte block the setter copies.
        // First byte at this+16; alignas(16) keeps the IOBuffer status byte padded out to the
        // 16-byte boundary the X360 `addi this,0x10` member start implies.
        struct alignas(16) ActiveRaceCarInterfaceStorage { unsigned char maBytes[10480]; };

        // X360 0x822BA228: read-lock handle, returns &mActiveRaceCarInterface (this + 16).
        const ActiveRaceCarInterfaceStorage* GetActiveRaceCarInterface() const;
        // X360 0x822BA2D0: read-lock handle, returns &mRequestInterface (this + 10496).
        const RequestInterface* GetRequestInterface() const;
        // X360 0x827A2888: write-lock handle, merges lrOther into mRequestInterface.
        void AppendRequestInterface(const RequestInterface& lrOther);
        // X360 0x827A27D0: write-lock handle, block-copies the 10480-byte race-car input payload
        // into mActiveRaceCarInterface (this + 16).
        void SetActiveRaceCarInterface(const ActiveRaceCarInterfaceStorage& lrInterface);

        static void _AssertLayout();

    private:
        ActiveRaceCarInterfaceStorage mActiveRaceCarInterface;   // +16     (10480 bytes)
        RequestInterface              mRequestInterface;         // +10496  (16+10480)
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::InputBuffer_PostPhysics (DWARF BrnWorldEntityModuleIO.h:212).
    // ADDITIVE GROW: homes the two X360-emitted GetGameActionQueue overloads of the
    // post-physics input buffer:
    //   GetGameActionQueue() const @ 0x822BA768  read-lock  (bit 4) -> &mGameActionQueue (this + 4)  (DWARF :223)
    //   GetGameActionQueue()       @ 0x827A2D28  write-lock (bit 3) -> &mGameActionQueue (this + 4)  (DWARF :224)
    //
    // The const getter tests the read-lock bit (`extrwi r11,r11,1,27` == bit 4); the non-const
    // getter tests the write-lock bit (`extrwi r11,r11,1,28` == bit 3). Both return the member
    // address (this + 4).
    //
    // LAYOUT (X360 getter return-offset, authoritative):
    //   base    CgsModule::IOBuffer               (1-byte status; +1..+3 pad)
    //   +4      GameActionQueue mGameActionQueue   (foreign EventQueue payload) :229
    //
    // FLAG (foreign type): mGameActionQueue is a foreign InputBuffer::GameActionQueue EventQueue
    // whose own home lands elsewhere; it is modelled as correctly-positioned opaque storage so
    // the X360 +4 return offset is exact. (A foreign EventQueue holds a host pointer and would
    // widen past this member on x64, so no offsets after it are pinned.)
    struct InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        // Opaque foreign-type storage (see FLAG above): first byte at this + 4.
        struct GameActionQueueStorage { unsigned char maBytes[1]; };

        // X360 0x822BA768: read-lock tripwire ("Not locked for reading"); return this + 4.
        const GameActionQueueStorage* GetGameActionQueue() const;
        // X360 0x827A2D28: write-lock tripwire ("Not locked for writing"); return this + 4.
        GameActionQueueStorage* GetGameActionQueue();

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte; the X360 build places mGameActionQueue at
        // this + 4, so pad bytes +1..+3 explicitly.
        u8                     maStatusPad[3];       // +1..+3 (force +4)
        GameActionQueueStorage mGameActionQueue;     // +4      :229
    };

    // ========================================================================
    // BrnWorld::WorldEntityIO::OutputBuffer_PreScene (DWARF BrnWorldEntityModuleIO.h:109).
    // ADDITIVE GROW: homes the three X360-emitted accessors of the world-entity pre-scene
    // output buffer:
    //   GetSceneInputInterface() const @ 0x827A2938  read-lock  (bit 4) -> &mSceneInputInterface (this + 208)     (DWARF :120)
    //   GetSceneInputInterface()       @ 0x822BA378  write-lock (bit 3) -> &mSceneInputInterface (this + 208)     (DWARF :121)
    //   GetGameEventQueue()            @ 0x822BA420  write-lock (bit 3) -> &mGameEventQueue (this + 818976)       (DWARF :124/:150)
    //
    // The const getter tests the read-lock bit (`extrwi r11,r11,1,27` == bit 4); the mutators
    // test the write-lock bit (`extrwi r11,r11,1,28` == bit 3). mSceneInputInterface sits at
    // this + 0xD0 (208), after the three prop-queue members (mPropInstancesNeededForZoneQueue
    // :146 / mPropGraphicsLoadedQueue :147 / mPropGraphicsUnloadedQueue :148). mGameEventQueue
    // sits at this + 0xC7F20 (818976, asm: `addis r3,this,0xC; addi r3,r3,0x7F20`).
    //
    // LAYOUT (X360 getter return-offsets, authoritative):
    //   base    CgsModule::IOBuffer                       (1-byte status)
    //   +1      (three prop-queue members :146/:147/:148; opaque)
    //   +208    SceneInputInterface mSceneInputInterface   :149
    //   +...    (mSoundWorldLoadInterface :151 etc.; opaque interior)
    //   +818976 GameEventQueue      mGameEventQueue        :150
    //
    // FLAG (foreign types): the prop-queue members, mSceneInputInterface and mGameEventQueue have
    // their own owning homes elsewhere and are NOT reconstructed here; each is modelled as
    // correctly-positioned opaque storage so the X360-pinned return offsets (+208, +818976) are
    // exact. The trailing member miPlayerZoneNumber :152 is beyond this slice and omitted.
    struct OutputBuffer_PreScene : public CgsModule::IOBuffer
    {
        // Opaque foreign-type storage (see FLAG above).
        struct SceneInputInterfaceStorage     { unsigned char maBytes[1]; };
        struct GameEventQueueStorage          { unsigned char maBytes[1]; };
        struct PropGraphicsLoadedQueueStorage   { unsigned char maBytes[1]; };  // +76   :147
        struct PropGraphicsUnloadedQueueStorage { unsigned char maBytes[1]; };  // +140  :148
        struct SoundWorldLoadInterfaceStorage   { unsigned char maBytes[1]; };  // +820528 :151

        // X360 0x827A2938: read-lock tripwire ("Not locked for reading"); return this + 208.
        const SceneInputInterfaceStorage* GetSceneInputInterface() const;
        // X360 0x822BA378: write-lock tripwire ("Not locked for writing"); return this + 208.
        SceneInputInterfaceStorage* GetSceneInputInterface();
        // X360 0x822BA420: write-lock tripwire ("Not locked for writing"); return this + 818976.
        GameEventQueueStorage* GetGameEventQueue();
        // X360 0x822BA4C8: write-lock; return &mPropGraphicsLoadedQueue (this + 76).   :127
        PropGraphicsLoadedQueueStorage* GetPropGraphicsLoadedQueue();
        // X360 0x822BA618: write-lock; return &mPropGraphicsUnloadedQueue (this + 140). :133
        PropGraphicsUnloadedQueueStorage* GetPropGraphicsUnloadedQueue();
        // X360 0x822BA6C0: write-lock; return &mSoundWorldLoadInterface (this + 820528). :136
        SoundWorldLoadInterfaceStorage* GetSoundWorldLoadInterface();

        static void _AssertLayout();

    private:
        // The IOBuffer base is a single status byte. The prop-queue members land at their
        // X360-attested offsets (+76 loaded, +140 unloaded), mSceneInputInterface at +208,
        // mGameEventQueue at +818976, mSoundWorldLoadInterface at +820528. Explicit gaps carve
        // each to its exact offset (all opaque foreign-type storage, no host pointers -> the
        // byte offsets are safe to pin).
        unsigned char                    maPreLoaded[76 - 1];                 // +1..+75
        PropGraphicsLoadedQueueStorage   mPropGraphicsLoadedQueue;            // +76      :147
        unsigned char                    maLoadedToUnloaded[140 - 76 - 1];    // +77..+139
        PropGraphicsUnloadedQueueStorage mPropGraphicsUnloadedQueue;          // +140     :148
        unsigned char                    maUnloadedToScene[208 - 140 - 1];    // +141..+207
        SceneInputInterfaceStorage       mSceneInputInterface;               // +208     :149
        unsigned char                    maSceneToGameEvent[818976 - 208 - 1]; // +209..+818975
        GameEventQueueStorage            mGameEventQueue;                     // +818976  :150
        unsigned char                    maGameEventToSound[820528 - 818976 - 1]; // +818977..+820527
        SoundWorldLoadInterfaceStorage   mSoundWorldLoadInterface;            // +820528  :151
    };
}
}
