#pragma once

// ===========================================================================
// CgsSceneManager::OverlapGenerationIO -- the OverlapGenerationModule's per-frame IO
// buffers.
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.{h,cpp}
//
// THIS FILE NAME IS CONSOLE-ATTESTED, not inferred: every accessor below bakes its own
// __FILE__/__LINE__ into the lock tripwire, and the X360 .rdata literal is
//     "d:\p4\b5_main\burnout\main\code\gameshared\gameclasses\scenemanager\contactgen\
//      CgsOverlapGenerationModuleIO.h"
// (seen in 0x828AFEE8/0x828AFF90/0x828B0038/0x828B00E0/0x828B0188/0x828B0230/0x828B02D8/
// 0x828B0380/0x828B0428/0x828B04D0). The DWARF line numbers those tripwires quote are
// reproduced against each declaration.
//
// The INPUT buffer carries the four per-frame body-mutation queues the SceneManagerModule
// fills while draining InSceneUpdateInterface; the OverlapGenerationModule's Process*Queue
// passes replay them into its SceneSweeper. The OUTPUT buffer carries the candidate
// overlapping volume-instance pairs the sweeper produced, which
// SceneManagerModule::BridgeOverlapGenerationToOverlapCulling then forwards to the
// narrow phase.
//
// ---- CLASS SHAPE: DecFIGS DWARF (references/DecFIGS/dwarfdump/GameShared/GameClasses/
//      SceneManager/ContactGen/CgsOverlapGenerationModuleIO.{h,cpp}) ----------------------
//   :38  const uint32_t guMAX_NUM_BODIES = 16384
//   :54  struct InAddBody         : Event { AABBox mAabb; InEventAddForCollision::CullingGroup
//                                           mCullGroup; uint32_t muIndex;
//                                           rw::physics::BodyState meBodyState;
//                                           VolumeInstanceId mVolumeInstanceID; }
//   :111 struct InForceNoPadding  : Event { uint32_t muVolInstIndex; }
//   :144 struct InUpdateBody      : Event { AABBox mAabb; Vector3 mPadding; uint32_t muIndex;
//                                           VolumeInstanceId mVolumeInstanceID; }
//   :163 struct InRemoveBody      : Event { VolumeInstanceId mVolumeInstanceID; uint32_t muIndex; }
//   :180 struct InputBuffer  : IOBuffer  (four queues; Construct/Destruct; the four producers;
//                                         eight accessors)
//   :253 struct OutputBuffer : IOBuffer  (EventQueue<OverlappingPair,16384>; Construct/Destruct;
//                                         two accessors)
//
// ---- LAYOUT + SIZES: X360 ARTIST asm (authoritative) --------------------------------
// InputBuffer::Construct @0x828CB918 seats the four queues, and
// IOBufferStack::DestroyIOBuffer<InputBuffer> @0x828C53F0 frees the exact total:
//     status byte    @      0   (CgsModule::IOBuffer base; `stb r11(=1), 0(r31)`)
//     mAddBodyQueue        @         16   (`addi r3,r31,0x10`)      EventQueue<InAddBody,16384>
//     mUpdateBodyQueue     @  1,048,608   (`addis 0x10; addi 0x20`) EventQueue<InUpdateBody,16384>
//     mRemoveBodyQueue     @  2,097,200   (`addis 0x20; addi 0x30`) EventQueue<InRemoveBody,16384>
//     mForceNoPaddingQueue @  2,359,360   (`addis 0x24; addi 0x40`) EventQueue<InForceNoPadding,128>
//     sizeof(InputBuffer)  ==  2,359,888  (IOBufferStack::Free(a1, *a2, 2359888) @0x828C53F0)
// Each queue's miLength is re-zeroed after its Construct (`stw 0` at +0x18 / +0x100028 /
// +0x200038 / +0x240048 -- i.e. queueBase+8, the BaseEventQueue length word), and
// DestroyIOBuffer clears the same four words in reverse order before IOBuffer::Destruct.
// The element strides are measured from the per-instantiation AddEvent bodies:
//     InAddBody        64  (0x828B85D8 `slwi r10,r10,6`)
//     InUpdateBody     64  (0x828B8730 `slwi r10,r10,6`)
//     InRemoveBody     16  (0x828B8888 `slwi r11,r11,4`)
//     InForceNoPadding  4  (0x828B89D0 `slwi r11,r11,2`, a single 32-bit move)
// and the queue capacities from the per-instantiation Construct bodies (0x828C4B80 /
// 0x828C4BF0 / 0x828C4C60 store 0x4000; 0x828C4CD0 stores 0x80).
//
// OutputBuffer: IOBufferStack::CreateIOBuffer<OutputBuffer> @0x828CC780 allocates
// 262,160 bytes, sets the status byte, then constructs EventQueue<OverlappingPair,16384>
// at this+4 and clears its length -- i.e. the whole buffer is the one queue.
//
// ⚠️ CONSOLE-vs-HOST: every byte offset above is an X360 (32-bit) value quoted as
// provenance. Nothing here is spelled as an offset; the members are named and the host
// compiler lays them out. It happens that sizeof(InputBuffer) is 2,359,888 on BOTH
// targets (the x64 BaseEventQueue header grows 12->16 bytes only for the 4-byte-element
// force-no-padding queue, which the X360 tail padding already absorbed), while
// sizeof(OutputBuffer) legitimately differs (the queue moves from +4 to +8 behind the
// widened event pointer). The static_asserts below pin the ELEMENT layouts, which are
// identical on both targets, and the input buffer's total.
// ===========================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                     // Vector3 (InUpdateBody::mPadding)

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                          // CgsModule::IOBuffer (base of both buffers)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                        // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                // CgsModule::Event (the empty queued-event base)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"            // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/CgsOverlappingPair.h"             // CgsSceneManager::OverlappingPair (output element)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddForCollision.h"  // InEventAddForCollision::CullingGroup
#include "rw/physics/rigidbody.h"                                               // rw::physics::BodyState

#include <cstddef>  // offsetof (the layout tripwires below)

// rw::collision::AABBox is only NAMED here (the two producers take it by pointer). Its real
// home, vendor/renderware/collision/AABBox.hpp, pulls the SDKs/EATech rw::math::vpu::Vector3
// CLASS into a TU that already has the vendor 4-lane POD via BrnCommonTypes.h, and the two
// cannot coexist -- a PRE-EXISTING FORK of rw::math::vpu::Vector3
// (SDKs/EATech/include/rw/math/vpu/vector3.h:26 vs vendor/renderware/include/rw/math/vpu/
// types.h:24) that is owned by neither this cluster nor any of the three committed readers
// that already hit it: CgsSceneSweeper_wQ5_01.cpp:26-53, CgsVolumeManager.cpp:54-77 and
// PropManager_wQ4_03.cpp:289-301. It is REPORTED, not worked around. MEASURED for this
// header too: adding the AABBox.hpp include here fails with `C2011:
// "rw::math::vpu::Vector3": struct redefinition` plus a >100-error cascade.
// Forward declaration per the AGENTS.md pointer-only exception (same as CgsSceneSweeper.h:143).
namespace rw { namespace collision { class AABBox; } }

namespace CgsSceneManager
{
namespace OverlapGenerationIO
{
    // ⚠️ PLACEHOLDER TYPE, not a console type: the byte image of rw::collision::AABBox
    // spelled over the vendor POD Vector3 this header speaks, so InAddBody/InUpdateBody can
    // carry the world box BY VALUE (which they must -- the console copies all 32 bytes into
    // the queued event, and ProcessAddBodyQueue @0x828C1D18 then hands the EVENT POINTER
    // straight to SceneSweeper::AddObject as `const AABBox*`). Two 16-byte corner rows,
    // mMin @+0x00 / mMax @+0x10 -- an image PINNED by the already-mounted layout oracle
    // GameSource/Physics/PropManager/PropManager_wQ4_03_embed_check.cpp (build_game_exe.bat
    // line 1748), which CAN include AABBox.hpp and static_asserts sizeof==32 /
    // offsetof(mMin)==0 / offsetof(mMax)==16, so a change to AABBox breaks THAT gate rather
    // than this header. RETIRE THIS TYPE the moment the Vector3 fork is healed: the members
    // below become plain `rw::collision::AABBox mAabb;` with AABBox.hpp included, and nothing
    // else changes (the layout is identical by construction).
    struct alignas(16) AABBoxRows
    {
        Vector3 mMin;   // rw::collision::AABBox +0x00
        Vector3 mMax;   // rw::collision::AABBox +0x10
    };
    static_assert(sizeof(AABBoxRows) == 32, "AABBox image is two 16-byte rows");

    // CgsOverlapGenerationModuleIO.h:38 -- the capacity of the three body queues.
    // DWARF spells this as a namespace-scope global constant (guMAX_NUM_BODIES), not a
    // KU_ class constant; the DWARF name wins over the house convention here.
    const u32 guMAX_NUM_BODIES = 16384;

    // The force-no-padding queue is NOT sized by guMAX_NUM_BODIES: its per-instantiation
    // Construct @0x828C4CD0 stores 0x80, and the DWARF typedef at :115 spells 128.
    const u32 guMAX_NUM_FORCE_NO_PADDING = 128;

    // ---- input events ---------------------------------------------------------------
    // All four derive from the empty CgsModule::Event queued-event base (the DWARF dump
    // prints the base unqualified as `Event`; CgsModule::Event is the tree's single
    // canonical empty base and is what the identically-shaped TriangleCacheManagerIO
    // events already use). Empty base -> EBO -> zero layout cost, so mAabb still sits at
    // offset 0, which is load-bearing: ProcessAddBodyQueue @0x828C1D18 passes the EVENT
    // POINTER itself where SceneSweeper::AddObject wants `const AABBox*` (`mr r5,r31`).

    // CgsOverlapGenerationModuleIO.h:54. Produced by InputBuffer::AddBody (inlined into
    // SceneManagerModule::AddBody @0x828BA498), consumed by
    // OverlapGenerationModule::ProcessAddBodyQueue @0x828C1D18.
    //   X360 producer stores: 4x ld/std off the caller's box -> +0x00..+0x1F;
    //   stw r7 -> +0x20; stw r5 -> +0x24; stw r8 -> +0x28; std r9 -> +0x30.
    //   X360 consumer reads:  r5 = the event pointer (== &mAabb); lwz 0x24 -> muIndex;
    //   lwz 0x28 -> meBodyState; ld 0x30 -> mVolumeInstanceID.
    struct alignas(16) InAddBody : public CgsModule::Event
    {
        // The culling-group tag: the same u32 typedef the add-for-collision event carries
        // (DWARF names the member's type `InEventAddForCollision::CullingGroup`).
        typedef SceneManagerIO::InEventAddForCollision::CullingGroup CullingGroup;

        AABBoxRows             mAabb;              // :56  +0x00 (DWARF type: AABBox; see AABBoxRows)
        CullingGroup           mCullGroup;         // :57  +0x20
        u32                    muIndex;            // :58  +0x24 sweeper object index
        rw::physics::BodyState meBodyState;        // :59  +0x28 STATIC/FROZEN/ACTIVE body
        VolumeInstanceId       mVolumeInstanceID;  // :62  +0x30 (packed 64-bit handle)
    };

    // CgsOverlapGenerationModuleIO.h:111. Produced by InputBuffer::ForceNoPadding
    // (inlined into SceneManagerModule::ProcessForceNoPaddingEvent @0x828CFC80, which
    // stacks a single word and appends it), consumed by
    // OverlapGenerationModule::ProcessForceNoPaddingQueue @0x828B56D8.
    // NOTE this is a one-word STRUCT, not a bare u32: the DWARF names the field.
    struct InForceNoPadding : public CgsModule::Event
    {
        u32 muVolInstIndex;  // :113 +0x00 (the volume-instance index, == sweeper object index)
    };

    // CgsOverlapGenerationModuleIO.h:144. Produced by InputBuffer::UpdateBody @0x828BA430
    // (the one producer the X360 emits OUT OF LINE), consumed by
    // OverlapGenerationModule::ProcessUpdateBodyQueue @0x828C1DE8.
    //   X360 producer stores: 4x ld/std off the caller's box -> +0x00..+0x1F;
    //   stvx128 v1 -> +0x20; stw r4 -> +0x30; std r6 -> +0x38.
    struct alignas(16) InUpdateBody : public CgsModule::Event
    {
        AABBoxRows            mAabb;              // :146 +0x00 (DWARF type: AABBox; see AABBoxRows)
        Vector3               mPadding;           // :147 +0x20 (the sweeper's padding lane)
        u32                   muIndex;            // :148 +0x30 sweeper object index
        VolumeInstanceId      mVolumeInstanceID;  // :149 +0x38
    };

    // CgsOverlapGenerationModuleIO.h:163. Produced by InputBuffer::RemoveBody (inlined
    // into SceneManagerModule::ProcessRemoveForCollisionEvent @0x828CF828: `v10 = *a2`
    // (the 8-byte id) then `v11 = index`), consumed by
    // OverlapGenerationModule::ProcessRemoveBodyQueue @0x828C1E90.
    struct InRemoveBody : public CgsModule::Event
    {
        VolumeInstanceId mVolumeInstanceID;  // :165 +0x00
        u32              muIndex;            // :166 +0x08 sweeper object index
    };

    // ---- the queue instantiations ----------------------------------------------------
    // DWARF declares these four typedefs at :64 / :115 / :151 / :168 -- i.e. each one
    // immediately after its element struct and well before InputBuffer opens at :180 --
    // so they are namespace-scope typedefs. (The dwarfdump renders them qualified as
    // `InputBuffer::InAddBodyQueue` because the typedef DIE is reachable through the
    // class that uses it; the decl_line is what pins the scope. Layout-neutral either
    // way, and namespace scope is the spelling every existing consumer already uses.)
    typedef CgsModule::EventQueue<InAddBody, 16384>        InAddBodyQueue;         // :64
    typedef CgsModule::EventQueue<InForceNoPadding, 128>   InForceNoPaddingQueue;  // :115
    typedef CgsModule::EventQueue<InUpdateBody, 16384>     InUpdateBodyQueue;      // :151
    typedef CgsModule::EventQueue<InRemoveBody, 16384>     InRemoveBodyQueue;      // :168

    // CgsOverlapGenerationModuleIO.h:242 -- the output element queue.
    typedef CgsModule::EventQueue<OverlappingPair, 16384>  OutOverlappingPairQueue;

    // ---- the input buffer ------------------------------------------------------------
    // CgsOverlapGenerationModuleIO.h:180. Constructed/destroyed through the module IO
    // buffer stack (CreateIOBuffer<InputBuffer> / DestroyIOBuffer<InputBuffer>
    // @0x828C53F0), which is why Construct/Destruct are plain members rather than a
    // ctor/dtor pair.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        void Construct();   // :185  @0x828CB918 (out of line in the X360 build)
        void Destruct();    // :189  (inlined into DestroyIOBuffer<InputBuffer> @0x828C53F0)

        // ---- producers (all four inlined into their SceneManagerModule callers except
        //      UpdateBody, which the X360 emits out of line at 0x828BA430) -------------
        void AddBody(u32 luIndex, const rw::collision::AABBox* lpAabb,
                     InAddBody::CullingGroup lCullGroup, rw::physics::BodyState leBodyState,
                     VolumeInstanceId lVolumeInstanceID);                       // :200
        void RemoveBody(u32 luIndex, VolumeInstanceId lVolumeInstanceID);       // :205
        void UpdateBody(u32 luIndex, const rw::collision::AABBox* lpAabb,
                        Vector3 lvPadding, VolumeInstanceId lVolumeInstanceID); // :212 @0x828BA430
        void ForceNoPadding(u32 luVolInstIndex);                                // :216

        // ---- accessors -------------------------------------------------------------
        // Each asserts the matching IOBuffer lock; the X360 emits a separate out-of-line
        // copy per accessor whose baked line number is quoted here.
        const InAddBodyQueue*        GetAddBodyQueue() const;         // :222 @0x828AFEE8 -> this+16
        const InUpdateBodyQueue*     GetUpdateBodyQueue() const;      // :223 @0x828AFF90 -> this+1048608
        const InRemoveBodyQueue*     GetRemoveBodyQueue() const;      // :224 @0x828B0038 -> this+2097200
        const InForceNoPaddingQueue* GetForceNoPaddingQueue() const;  // :225 @0x828B00E0 -> this+2359360

        InAddBodyQueue*        GetAddBodyQueue();                     // :227 @0x828B0188 -> this+16
        InUpdateBodyQueue*     GetUpdateBodyQueue();                  // :228 @0x828B0230 -> this+1048608
        InRemoveBodyQueue*     GetRemoveBodyQueue();                  // :229 @0x828B02D8 -> this+2097200
        InForceNoPaddingQueue* GetForceNoPaddingQueue();              // :230 @0x828B0380 -> this+2359360

    private:
        InAddBodyQueue        mAddBodyQueue;         // :234  X360 +16
        InUpdateBodyQueue     mUpdateBodyQueue;      // :235  X360 +1,048,608
        InRemoveBodyQueue     mRemoveBodyQueue;      // :236  X360 +2,097,200
        InForceNoPaddingQueue mForceNoPaddingQueue;  // :237  X360 +2,359,360
    };

    // ---- the output buffer -----------------------------------------------------------
    // CgsOverlapGenerationModuleIO.h:253. Filled by
    // OverlapGenerationModule::OutputCollidingPairs @0x828C1F58 under a write lock and
    // drained by SceneManagerModule::BridgeOverlapGenerationToOverlapCulling @0x828BA538.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        void Construct();  // :258 (inlined into CreateIOBuffer<OutputBuffer>  @0x828CC780)
        void Destruct();   // :262 (inlined into DestroyIOBuffer<OutputBuffer> @0x828C5068)

        const OutOverlappingPairQueue* GetOverlappingPairQueue() const;  // :266 @0x828B0428 -> this+4
        OutOverlappingPairQueue*       GetOverlappingPairQueue();        // :267 @0x828B04D0 -> this+4

    private:
        OutOverlappingPairQueue mOverlappingPairQueue;  // :271  X360 +4
    };

    // ---- layout tripwires --------------------------------------------------------------
    // The element sizes/offsets below are identical on the X360 and on the x64 host (no
    // member of any event is a pointer), so they can be asserted directly. sizeof of the
    // buffers themselves is asserted only where the console value survives widening.
    static_assert(sizeof(InAddBody) == 64, "InAddBody stride == 0x40 (AddEvent 0x828B85D8 slwi 6)");
    static_assert(offsetof(InAddBody, mAabb) == 0x00, "InAddBody::mAabb at +0x00");
    static_assert(offsetof(InAddBody, mCullGroup) == 0x20, "InAddBody::mCullGroup at +0x20 (stw r7)");
    static_assert(offsetof(InAddBody, muIndex) == 0x24, "InAddBody::muIndex at +0x24 (stw r5)");
    static_assert(offsetof(InAddBody, meBodyState) == 0x28, "InAddBody::meBodyState at +0x28 (stw r8)");
    static_assert(offsetof(InAddBody, mVolumeInstanceID) == 0x30, "InAddBody::mVolumeInstanceID at +0x30 (std r9)");

    static_assert(sizeof(InUpdateBody) == 64, "InUpdateBody stride == 0x40 (AddEvent 0x828B8730 slwi 6)");
    static_assert(offsetof(InUpdateBody, mAabb) == 0x00, "InUpdateBody::mAabb at +0x00");
    static_assert(offsetof(InUpdateBody, mPadding) == 0x20, "InUpdateBody::mPadding at +0x20 (stvx128 v1)");
    static_assert(offsetof(InUpdateBody, muIndex) == 0x30, "InUpdateBody::muIndex at +0x30 (stw r4)");
    static_assert(offsetof(InUpdateBody, mVolumeInstanceID) == 0x38, "InUpdateBody::mVolumeInstanceID at +0x38 (std r6)");

    static_assert(sizeof(InRemoveBody) == 16, "InRemoveBody stride == 0x10 (AddEvent 0x828B8888 slwi 4)");
    static_assert(offsetof(InRemoveBody, mVolumeInstanceID) == 0x00, "InRemoveBody::mVolumeInstanceID at +0x00");
    static_assert(offsetof(InRemoveBody, muIndex) == 0x08, "InRemoveBody::muIndex at +0x08");

    static_assert(sizeof(InForceNoPadding) == 4, "InForceNoPadding stride == 4 (AddEvent 0x828B89D0 slwi 2)");

    // The whole input buffer: IOBufferStack::Free(..., 2359888) @0x828C53F0. This total
    // survives the x64 widening intact (see the header banner), so it is asserted.
    static_assert(sizeof(InputBuffer) == 2359888, "sizeof(OverlapGenerationIO::InputBuffer) == 2,359,888 (DestroyIOBuffer 0x828C53F0)");
}
}
