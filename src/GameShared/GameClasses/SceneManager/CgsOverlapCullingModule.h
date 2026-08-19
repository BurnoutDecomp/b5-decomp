#pragma once

// ===========================================================================
// CgsSceneManager::OverlapCullingModule
//   Home: GameShared/GameClasses/SceneManager/CgsOverlapCullingModule.h
//         GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapCullingModule.cpp
//
// The contact-generation "narrow phase" stage that takes the candidate overlap
// pairs the OverlapGenerationModule produced and resolves them into contacts, and
// also drives the internal-collision (nested-volume) bookkeeping. It is a
// CgsModule::ModuleSingleBuffered sub-module (the X360 Construct/Prepare/Release
// bodies chain to ModuleSingleBuffered::Construct/Prepare/Release -- proven from the
// disassembly). Embedded BY VALUE in CgsSceneManager::SceneManagerModule
// (mOverlapCuller); SceneManagerModule drives Construct/Destruct/Prepare and the
// per-frame CullOverlaps.
//
// Class shape (members, enums, virtuals, helper methods) is from the DecFIGS DWARF
// (CgsOverlapCullingModule.h / .cpp). It is the direct sibling of
// FineIntersectionTestModule (same prepare/release two-step handshake, same embedded
// rw::collision::VolumeVolumeQuery scratch buffer + handle pointer, same _AssertLayout
// pin). Per-member X360 byte offsets are documented from the asm word-indices; on the
// x64 PC build absolute offsets diverge (pointer width + base layout), so _AssertLayout
// pins only member ORDER, exactly as the FineIntersectionTestModule sibling.
//
// CullOverlaps(void*, void*) is kept as the SceneManagerModule-facing entry; it forwards
// to the typed Update(InputBuffer*, OutputBuffer*) the DWARF/asm attests (X360 vtable
// Update slot).
// ===========================================================================

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"  // CgsModule::ModuleSingleBuffered (base)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"          // CgsContainers::BitArray
#include "GameShared/GameClasses/SceneManager/CgsOverlappingPair.h" // CgsSceneManager::OverlappingPair

namespace rw { namespace collision { class VolumeVolumeQuery; } }

namespace CgsSceneManager
{
    // Scene-manager neighbours, taken by pointer only here (forward-declared to avoid a
    // whole-SceneManager include cascade; their real homes are CgsEntityManager.h /
    // CgsVolumeManager.h, included by the .cpp where needed).
    class EntityManager;
    class VolumeManager;
    struct VolumeInstance;

    // NOTE: the X360 member is `ContactGenerator* mpContactGenerator` (DWARF
    // CgsOverlapCullingModule.h:154) -- the fine narrow-phase query owner that produces
    // the contacts. In the committed tree `CgsSceneManager::ContactGenerator` is already
    // taken as a NAMESPACE (CgsContactGenerationIO.h's QueryAccumulator IO payload), so to
    // avoid forking that name the handle is modelled as an opaque void* here. It is only
    // stored (SetContactGenerator) and read by the unreconstructed DoPairQuery body.

    // The scene manager's own spelling of a serialised RenderWare collision volume --
    // an INCOMPLETE type by design, forward-declared identically in
    // CgsVolumeStore.h:82 (this is the same entity, not a second one). It is what
    // VolumeManager::GetRwVolume hands back and what DoPairQuery forwards to the
    // rw::collision narrow phase; the ONE place that needs its interior
    // (CgsVolumeManager.cpp:212) reinterpret_casts it to rw::collision::Volume with the
    // DWARF's own note that volume.h:39 typedefs exactly that. DoPairQuery does the
    // same, for the same reason, at three sites in its .cpp.
    namespace VolRef { struct Volume; }

    namespace OverlapCullingIO
    {
        struct InputBuffer;
        struct OutputBuffer;
    }

    class OverlapCullingModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // The prepare/release two-step handshake stages (DWARF CgsOverlapCullingModule.h:52/59).
        enum EPrepareStage
        {
            PREPARESTAGE_START   = 0,
            PREPARESTAGE_MANAGER = 1,
            PREPARESTAGE_DONE    = 2,
        };

        enum EReleaseStage
        {
            RELEASESTAGE_START   = 0,
            RELEASESTAGE_MANAGER = 1,
            RELEASESTAGE_DONE    = 2,
        };

        // The X360 budgets 0x62000 (401408) bytes for the in-class VolumeVolumeQuery scratch
        // and asserts the actual GetResourceDescriptor size fits (Construct @0x828C18E8, asserts
        // size <= 0x62000 and 16-byte alignment).
        static const u32 KU_VOL_QUERY_MEM_SIZE = 401408;

        // The internal-collision tables are sized to KI_MAX_NUM_VOLUME_INSTANCES (asm bound
        // 0x13B8 == 5048; the assert strings name KI_MAX_NUM_VOLUME_INSTANCES).
        static const u32 KU_MAX_NUM_VOLUME_INSTANCES = 5048;

        OverlapCullingModule();

        // --- virtual module interface (overrides ModuleSingleBuffered / Module) ---
        void Construct() override;                                       // @ 0x828C18E8
        bool Release() override;                                         // @ 0x828AB448
        void Destruct() override;                                        // @ 0x828C... (trivial)

        // Typed prepare/update. These are the module's own typed entry points (not the
        // no-arg base virtuals); SceneManagerModule calls them directly.
        bool Prepare(EntityManager* lpEntityManager, VolumeManager* lpVolumeManager);  // @ 0x828B52B8
        void Update(OverlapCullingIO::InputBuffer* lpInputBuffer,
                    OverlapCullingIO::OutputBuffer* lpOutputBuffer);                    // @ 0x828C... (vtable Update)

        // SceneManagerModule-facing entry (CgsSceneManagerModule.cpp UpdateContactGeneration):
        // forwards to the typed Update.
        void CullOverlaps(void* lpInputBuffer, void* lpOutputBuffer);

        void SetContactGenerator(void* lpContactGenerator);  // CgsOverlapCullingModule.h:95 (X360 ContactGenerator*)

        void ProcessAccumulatedQueries(OverlapCullingIO::OutputBuffer* lpOutputBuffer);  // @ 0x828C... :428

        // Never called at runtime; pins the asm-attested member ORDER.
        static void _AssertLayout();

    private:
        // --- the per-frame queue processors (DWARF private helpers) ---
        // Bodies: ContactGen/CgsOverlapCullingModule.cpp (this class's main TU).
        void ProcessOverlapsQueue(OverlapCullingIO::OutputBuffer* lpOutputBuffer,
                                  const OverlapCullingIO::InputBuffer* lpInputBuffer);    // @ 0x828D0330 :237
        void ProcessAddInternalVolumeQueue(const OverlapCullingIO::InputBuffer* lpInputBuffer);     // @ 0x828B5420 :307
        void ProcessRemoveInternalVolumeQueue(const OverlapCullingIO::InputBuffer* lpInputBuffer);  // @ :346
        void ProcessOverlap(OverlappingPair& lrOverlappingPair,
                            OverlapCullingIO::OutputBuffer* lpOutputBuffer);             // @ 0x828CAF38 :380

        // The narrow-phase pair query -- the culler's single contact PRODUCER, shared by
        // both the overlap path (ProcessOverlap) and the internal-collision path
        // (DoInternalCollision). DWARF CgsOverlapCullingModule.cpp:469 gives the exact
        // declaration, RETURN TYPE INCLUDED:
        //     void DoPairQuery(OutputBuffer*, const VolumeInstance*, uint32_t,
        //                      const VolRef::Volume*, const VolumeInstance*, uint32_t,
        //                      const VolRef::Volume*, float_t);
        // (It was declared `u32` here while the body was a `return 0` shell. The X360
        // body falls through to __restgprlr_16 without setting r3, which is why
        // Hex-Rays types both this and its caller `int`; nothing reads the value --
        // DoInternalCollision @0x828CB1D8 tail-calls it and discards r3.)
        //
        // ⚠️ luVolumeInstanceIndexA/B RENAMED 2026-08-19 (E3b's finding, confirmed
        // here from DoPairQuery's own asm). They were `luVolumeIndexA`/`luVolumeIndexB`,
        // which named the WRONG INDEX SPACE: a VolumeManager volume index is
        // `lpVolumeInstance->miVolumeIndex`, and that is what GetRwVolume is fed one
        // line earlier in every caller. These two are ENTITY-MANAGER VOLUME-INSTANCE
        // indices -- DoPairQuery stashes them in r17/r16 and stores them verbatim into
        // CgsSceneManager::Contact::muVolumeInstanceA/B (0x828C1B20/0x828C1B28 and
        // 0x828C1C7C/0x828C1C84), which is the pair of ids the world bridge resolves.
        void DoPairQuery(OverlapCullingIO::OutputBuffer* lpOutputBuffer,
                         const VolumeInstance* lpVolumeInstanceA, u32 luVolumeInstanceIndexA,
                         const VolRef::Volume* lpVolumeA,
                         const VolumeInstance* lpVolumeInstanceB, u32 luVolumeInstanceIndexB,
                         const VolRef::Volume* lpVolumeB, f32 lfPadding);                 // @ 0x828C1A18 :469

        // --- the internal-collision half -------------------------------------------
        // BODIES LIVE IN THE PARTFILE ContactGen/CgsOverlapCullingModule_wQ5_01.cpp
        // (wave Q5, cluster E3b). Declarations stay here; do NOT re-add bodies for
        // these three to CgsOverlapCullingModule.cpp -- that is an instant LNK2005.
        bool IsInsideEscapeVolume(s32 liVolumeInstanceIndex);                             // @ 0x828CB0A8 :600
        void DoInternalCollision(s32 liVolumeInstanceIndex,
                                 OverlapCullingIO::OutputBuffer* lpOutputBuffer);         // @ 0x828CB1D8 :644
        void ProcessInternalCollisions(OverlapCullingIO::OutputBuffer* lpOutputBuffer);   // @ 0x828CB308 :682

        // --- members (DWARF CgsOverlapCullingModule.h:148-175; offsets documented) ---
        EPrepareStage mePrepareStage;   // X360 word 138 (+0x228 from ModuleSingleBuffered base region)
        EReleaseStage meReleaseStage;   // X360 word 139

        VolumeManager*   mpVolumeManager;    // X360 word 140
        EntityManager*   mpEntityManager;    // X360 word 141

        // ⚠️ WORD INDEX CORRECTED 2026-08-19: this member is X360 word 142 (+0x238), not
        // 143. Word 143 (+0x23C) is mpVolVolQuery -- proven twice: Construct @0x828C18E8
        // stores the Initialize result to `this+572` (== 0x23C), and DoPairQuery's
        // aggregate arm loads the query handle with `lwz r11, 0x23C(r27)`
        // (0x828C1B8C / 0x828C1BC8). Nothing in the X360 export reads +0x238 at all,
        // which is consistent with mpContactGenerator being write-only in this build
        // (SetContactGenerator is its sole writer).
        void* mpContactGenerator;  // X360 word 142 (+0x238) (ContactGenerator*)

        // X360 word 143 (+0x23C). Set by Construct, read by DoPairQuery's prim x
        // aggregate arm (which fills its 1xN input block in place and calls
        // GetPrimitiveIntersections on it).
        rw::collision::VolumeVolumeQuery* mpVolVolQuery;

        // The in-class VolumeVolumeQuery scratch buffer (Construct partitions this; the X360
        // asserts the descriptor size <= KU_VOL_QUERY_MEM_SIZE and 16-byte alignment).
        alignas(16) u8 maVolumeVolumeQueryMem[KU_VOL_QUERY_MEM_SIZE];

        // Internal-collision bookkeeping, sized to KI_MAX_NUM_VOLUME_INSTANCES.
        CgsContainers::BitArray<KU_MAX_NUM_VOLUME_INSTANCES> mabIsUsingInternalCollision;
        u32 mauInternalVolumeInstanceIndex[KU_MAX_NUM_VOLUME_INSTANCES];
        u32 mauEscapeVolumeInstanceIndex[KU_MAX_NUM_VOLUME_INSTANCES];

        // Per-frame statistics counters (DWARF CgsOverlapCullingModule.h:168-175). The asm
        // word-indices (a1[110750]..a1[110757]) land these as a contiguous block.
        u32 muNumPrimPrimPairs;    // word 110750
        u32 muNumPrimAggPairs;     // word 110751
        u32 muNumOtherPairs;       // word 110752
        u32 muNumInstanceQueries;  // word 110753
        u32 muNumPPQs;             // word ...
        u32 muNumIntersections;    // word ...
        u32 muNumPrimPrimContacts; // word 110756
        u32 muNumPrimAggContacts;  // word 110757
    };

    // Post-increment over the prepare/release stage enums (DWARF CgsOverlapCullingModule.h:182/183),
    // used by Prepare/Release to advance the handshake. Mirrors the FineIntersectionTestModule
    // sibling's operator++ pair.
    inline OverlapCullingModule::EPrepareStage
    operator++(OverlapCullingModule::EPrepareStage& leStage, int)
    {
        const OverlapCullingModule::EPrepareStage leOld = leStage;
        leStage = static_cast<OverlapCullingModule::EPrepareStage>(static_cast<int>(leStage) + 1);
        return leOld;
    }

    inline OverlapCullingModule::EReleaseStage
    operator++(OverlapCullingModule::EReleaseStage& leStage, int)
    {
        const OverlapCullingModule::EReleaseStage leOld = leStage;
        leStage = static_cast<OverlapCullingModule::EReleaseStage>(static_cast<int>(leStage) + 1);
        return leOld;
    }
}
