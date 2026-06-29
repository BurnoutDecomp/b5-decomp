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

    // Per-volume reference produced by the broad phase. The DWARF DoPairQuery signature
    // names a VolRef::Volume*; that argument is only used by the (unreconstructed)
    // DoPairQuery body, so it is an opaque forward-declared pointer type here.
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
        void ProcessOverlapsQueue(OverlapCullingIO::OutputBuffer* lpOutputBuffer,
                                  const OverlapCullingIO::InputBuffer* lpInputBuffer);    // @ 0x828D0330 :237
        void ProcessAddInternalVolumeQueue(const OverlapCullingIO::InputBuffer* lpInputBuffer);     // @ 0x828B5420 :307
        void ProcessRemoveInternalVolumeQueue(const OverlapCullingIO::InputBuffer* lpInputBuffer);  // @ :346
        void ProcessOverlap(OverlappingPair& lrOverlappingPair,
                            OverlapCullingIO::OutputBuffer* lpOutputBuffer);             // @ :380

        // The narrow-phase pair query (DWARF CgsOverlapCullingModule.cpp:469). Large
        // RenderWare-driven body; declaration faithful, body not reconstructed this pass.
        u32 DoPairQuery(OverlapCullingIO::OutputBuffer* lpOutputBuffer,
                        const VolumeInstance* lpVolumeInstanceA, u32 luVolumeIndexA,
                        const VolRef::Volume* lpVolumeA,
                        const VolumeInstance* lpVolumeInstanceB, u32 luVolumeIndexB,
                        const VolRef::Volume* lpVolumeB, f32 lfPadding);                  // @ 0x828C1A18 :469

        bool IsInsideEscapeVolume(s32 liVolumeInstanceIndex);                             // @ :600
        void DoInternalCollision(s32 liVolumeInstanceIndex,
                                 OverlapCullingIO::OutputBuffer* lpOutputBuffer);         // @ :644
        void ProcessInternalCollisions(OverlapCullingIO::OutputBuffer* lpOutputBuffer);   // @ :682

        // --- members (DWARF CgsOverlapCullingModule.h:148-175; offsets documented) ---
        EPrepareStage mePrepareStage;   // X360 word 138 (+0x228 from ModuleSingleBuffered base region)
        EReleaseStage meReleaseStage;   // X360 word 139

        VolumeManager*   mpVolumeManager;    // X360 word 140
        EntityManager*   mpEntityManager;    // X360 word 141

        void* mpContactGenerator;  // X360 word 143 (ContactGenerator*; used by DoPairQuery)

        rw::collision::VolumeVolumeQuery* mpVolVolQuery;  // X360 word ... (the constructed query handle)

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
