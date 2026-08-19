#pragma once

// CgsSceneManager::OverlapCullingIO — the input/output payload event types for the
// overlap-culling (broadphase narrow-phase request) module. The InputBuffer carries three
// fixed-capacity EventQueues; two of them (the internal-collision-volume add/remove request
// queues) are bounded at guMAX_NUM_INTERNAL_VOLUME_REQUESTS (256).
//
// Event field sets recovered from the DecFIGS DWARF (CgsOverlapCullingModuleIO.h:46-57); the
// per-instantiation EventQueue<...,256>::Construct bodies are reconstructed from the X360 asm
// (0x828C4E20 / 0x828C4E90) in the sibling explicit-instantiation .cpp files.
//
// Each event derives from the empty CgsModule::Event base, so the queue stores them by their
// byte image. AddInternalCollisionVolume is 12 bytes (3 u32), RemoveInternalCollisionVolume is
// 4 bytes (1 u32); both are only 4-aligned, so EventQueue<T,256>'s inline maEvents buffer lands
// right after the 12-byte BaseEventQueue header (no 16-byte padding) -- the Construct stores its
// buffer pointer at this+0xC, exactly as the asm does (addi r30,this,0xC; stw r30,0(this)).

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CGS_ASSERT (lock tripwires)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                       // CgsModule::EventQueue, CgsModule::Event
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                         // CgsModule::IOBuffer (both buffers' base)
#include "GameShared/GameClasses/SceneManager/CgsOverlappingPair.h"            // CgsSceneManager::OverlappingPair (in-queue element)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerContact.h"        // CgsSceneManager::Contact (out-queue element)

namespace CgsSceneManager
{
namespace OverlapCullingIO
{
    // CgsOverlapCullingModuleIO.h:37/38 (DWARF).
    const u32 guMAX_NUM_BODIES                     = 16384;
    const u32 guMAX_NUM_INTERNAL_VOLUME_REQUESTS   = 256;

    // Empty per-module event base (CgsModule event-queue convention; the queue stores events
    // by byte image). Matches the namespace-local Event base the other SceneManager IO event
    // payloads use (e.g. CgsSceneManager::SceneManagerIO::Event in CgsSceneManagerIO_EventAddForCollision.h).
    struct Event {};

    // CgsOverlapCullingModuleIO.h:46 (DWARF). Request to add an internal collision volume
    // (a volume nested inside another) to the broadphase, carrying the three volume-instance
    // indices that wire the nested/escape relationship.
    struct AddInternalCollisionVolume : public Event
    {
        u32 muVolumeInstanceIndex;          // +0x00 (after the empty Event base)
        u32 muInternalVolumeInstanceIndex;  // +0x04
        u32 muEscapeVolumeInstanceIndex;    // +0x08
    };

    // CgsOverlapCullingModuleIO.h:55 (DWARF). Request to remove a previously-added internal
    // collision volume, identified by its volume-instance index.
    struct RemoveInternalCollisionVolume : public Event
    {
        u32 muVolumeInstanceIndex;  // +0x00 (after the empty Event base)
    };

    // The two bounded request queues (DWARF CgsOverlapCullingModuleIO.h:63/64).
    typedef CgsModule::EventQueue<AddInternalCollisionVolume, 256>    InAddInternalVolumeQueue;
    typedef CgsModule::EventQueue<RemoveInternalCollisionVolume, 256> InRemoveInternalVolumeQueue;

    // =====================================================================
    // ⭐ THE REAL BUFFERS (wave Q5, 2026-08-18). These replace the two 4096-byte
    // `u8 maReserved[4096]` stand-ins that live in ContactGen/CgsContactGenerationIO.h
    // (that file is another owner's this wave -- the stand-ins must be DELETED there and
    // this header included in their place; see the sceneio owner report for the exact
    // lines). Nothing else about the placeholder was salvageable: the console InputBuffer
    // is 266,280 bytes and the OutputBuffer 1,048,608.
    //
    // DECLARATION SHAPE is the DecFIGS DWARF verbatim
    // (references/DecFIGS/dwarfdump/.../ContactGen/CgsOverlapCullingModuleIO.h:72-143):
    // member set, member order, typedef names, and both getter pairs.
    //
    // CONSOLE LAYOUT (offsets are comments -- on this LLP64 host the queues are wider and
    // every access is BY NAME):
    //   InputBuffer   sizeof 0x41028 = 266280   (IOBufferStack::Free literal @0x828C4F70)
    //     +0        IOBuffer status byte
    //     +4        mOverlappingPairQueue     EventQueue<OverlappingPair,16384>  262156 = 12 + 16384*16
    //     +262160   mAddInternalVolumeQueue   EventQueue<AddInternal...,256>       3084 = 12 + 256*12
    //     +265244   mRemoveInternalVolumeQueue EventQueue<RemoveInternal...,256>   1036 = 12 + 256*4
    //   OutputBuffer  sizeof 0x100020 = 1048608 (IOBufferStack::Alloc literal @0x828CC698)
    //     +0        IOBuffer status byte
    //     +16       mContactQueue             EventQueue<Contact,16384>         1048592 = 16 + 16384*64
    //
    // ✅ CONSOLE ALIGNMENT NOTE -- RESOLVED 2026-08-19 (wave Q5, cluster E3a). This block
    // used to record a defect: the overlapping-pair queue sits at buffer+4 and its
    // Construct @0x828C4D40 puts maEvents at queue+0xC, so on the X360
    // CgsSceneManager::OverlappingPair is 4-ALIGNED -- while CgsOverlappingPair.h modelled
    // it as two 8-aligned u64 VolumeInstanceIds, which pushed the host's maEvents to +0x10.
    // That header has now been corrected to the DWARF's real field set
    // {u32, u32, f32, bool} (CgsSceneManagerTypes.h:87-90), which is 4-aligned with the
    // same 16-byte stride, so the host and the console now agree exactly. Nothing in this
    // file changed as a result -- access was always by name.
    // =====================================================================

    // DWARF :75. The culler's per-frame input: the broad phase's overlapping pairs plus the
    // two internal-collision-volume request queues.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // DWARF :61 -- the in-queue typedef (the :63/:64 pair above are the same typedefs
        // the DWARF nests in this struct; they are hoisted to namespace scope in this tree
        // because the two explicit-instantiation TUs name them there).
        typedef CgsModule::EventQueue<CgsSceneManager::OverlappingPair, 16384> InOverlappingPairQueue;

        // X360 0x828CB640 / (Destruct inlined into DestroyIOBuffer @0x828C4F70). Bodies in
        // the sibling CgsOverlapCullingModuleIO.cpp -- the console's own file (DWARF
        // CgsOverlapCullingModuleIO.cpp:46 / :72).
        void Construct();   // DWARF :80
        void Destruct();    // DWARF :84

        // @ 0x828B0698 -- read-lock tripwire (bit 4, "Not locked for reading\n"), `this+4`,
        // baked CgsOverlapCullingModuleIO.h:87. Its ONE caller is
        // OverlapCullingModule::ProcessOverlapsQueue @0x828D0330, which walks the queue with
        // BaseEventQueue<OverlappingPair>::GetEvent @0x828AE0D0 (stride 0x10).
        const InOverlappingPairQueue* GetOverlappingPairQueue() const        // :87
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mOverlappingPairQueue;
        }

        // ⭐ @ 0x828B0740 (the export's unnamed `sub_828B0740`) -- ASM-ATTESTED
        // 2026-08-19 (wave Q5, cluster E3a; the earlier "lock bit inferred" FLAG here is
        // RETIRED). Read-lock tripwire (bit 4: `lbz 0(this) ; extrwi r11,r11,1,27`,
        // "Not locked for reading\n"), baked file CgsOverlapCullingModuleIO.h line 0x58
        // == 88, and it returns `this + 0x40010` == this+262160 -- exactly the
        // mAddInternalVolumeQueue seat documented above. Its one caller is
        // OverlapCullingModule::ProcessAddInternalVolumeQueue @0x828B5420 (0x828B546C),
        // which then walks the queue with
        // BaseEventQueue<AddInternalCollisionVolume>::GetEvent @0x828AE220 (stride 0x0C
        // == the 3-u32 element, `slwi r11,r30,1 ; add r11,r30,r11 ; slwi r11,r11,2`).
        const InAddInternalVolumeQueue* GetAddInternalVolumeQueue() const    // :88
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mAddInternalVolumeQueue;
        }
        const InRemoveInternalVolumeQueue* GetRemoveInternalVolumeQueue() const  // :89
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mRemoveInternalVolumeQueue;
        }

        // :91 / :92 / :93 -- the write-locked producer handles. The producer is
        // SceneManagerModule::BridgeOverlapGenerationToOverlapCulling @0x828BA538 (round 4).
        InOverlappingPairQueue* GetOverlappingPairQueue()                    // :91
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mOverlappingPairQueue;
        }
        InAddInternalVolumeQueue* GetAddInternalVolumeQueue()                // :92
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mAddInternalVolumeQueue;
        }
        InRemoveInternalVolumeQueue* GetRemoveInternalVolumeQueue()          // :93
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mRemoveInternalVolumeQueue;
        }

    private:
        // No status pad here (unlike SceneManagerIO's buffers): the console seats this queue
        // at +4 and, since the OverlappingPair correction of 2026-08-19, the host element is
        // 4-aligned too, so the compiler lands it at +4 without one. Access is by name
        // regardless; a pad here would only be a wrong comment.
        InOverlappingPairQueue      mOverlappingPairQueue;       // :98  console +4
        InAddInternalVolumeQueue    mAddInternalVolumeQueue;     // :99  console +262160
        InRemoveInternalVolumeQueue mRemoveInternalVolumeQueue;  // :100 console +265244
    };

    // DWARF :116. The culler's per-frame output: the narrow phase's resolved contacts.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // DWARF :104.
        typedef CgsModule::EventQueue<CgsSceneManager::Contact, 16384> OutContactQueue;

        // The console INLINES both into the IOBufferStack templates
        // (CreateIOBuffer @0x828CC698 / DestroyIOBuffer @0x828C5148); the DWARF still
        // declares them as members (CgsOverlapCullingModuleIO.cpp:98 / :119), which is where
        // the bodies live in this tree.
        void Construct();   // DWARF :121
        void Destruct();    // DWARF :125

        // :128. No emitted body in the ARTIST image (nothing reads the culler's contacts
        // through a const handle -- SceneManagerModule::BridgeOverlapCullerToOutputBuffer
        // is the only consumer and it is a stub today). FLAG: lock bit inferred from the
        // family pattern; existence/offset/constness are DWARF-attested.
        const OutContactQueue* GetContactQueue() const                       // :128
        {
            CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
            return &mContactQueue;
        }

        // ⭐ @ 0x828B0938 -- write-lock tripwire (bit 3, "Not locked for writing\n"),
        // `this+16`, baked CgsOverlapCullingModuleIO.h:129. Recovered by headless IDA in
        // wave Q5 (it is an export hole). Its two call sites are both inside
        // OverlapCullingModule::DoPairQuery @0x828C1A18 (0x828C1B2C and 0x828C1C88) -- the
        // prim x prim arm and the prim x aggregate arm -- each of which then pushes with
        // BaseEventQueue<Contact>::AddEventSafe @0x828B8D08.
        OutContactQueue* GetContactQueue()                                   // :129
        {
            CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
            return &mContactQueue;
        }

    private:
        u8              maStatusPad[15];   // +1..+15 (force +16, the console's Contact-queue seat)
        OutContactQueue mContactQueue;     // :133 console +16
    };
}
}
