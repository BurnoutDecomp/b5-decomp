#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"   // Ptr32<T> (the 32-bit slot)
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"  // CgsGraphics::DispatchFrame (ModelInstanceCollector)

#include <cstddef>

// CgsModel.h - the CgsGraphics::Model wrapper/accessor.
//
// Reconstructed from the DecFIGS DWARF (GameShared/GameClasses/Graphics/CgsModel.h)
// plus the X360 pseudocode/asm for the three accessor entry points bodied in
// CgsModel.cpp (GetRenderable, DoesStateExist, GetLodDistance). Only the surface
// needed to compile those three functions is reconstructed here; the heavy
// render/IO methods declared by the DWARF (RenderModelOnly, RenderZOnly, ...) are
// left as honest declarations to be bodied by their own TUs.
//
// X360 member layout (32-bit target) confirmed against the asm:
//   +0x00 mppRenderables               Renderable**   (lwz 0(this) in GetRenderable)
//   +0x04 mpu8StateRenderableIndices   u8*            (lwz 4(this))
//   +0x08 mpfLodDistances              f32*           (lwz 8(this))
//   +0x0C miGameExplorerIndex          s32
//   +0x10 mu8NumRenderables            u8             (lbz 0x10(this))
//   +0x11 mu8Flags                     u8
//   +0x12 mu8NumStates                 u8             (lbz 0x12(this))
//   +0x13 mu8VersionNumber             u8
//
// *** ON-DISC LAYOUT (PVS wave 2026-07-27) ***
// Model is NOT a host object: it IS the streamed resource body that
// CgsResource::ModelResourceType relocates in place (@0x828A8578 rebases exactly
// the three u32 slots at +0/+4/+8, and GetSerialisedResourceDescriptor sizes the
// header at 20 bytes). Modelling the three tables as host-width pointers doubled
// every offset past the first, so mpu8StateRenderableIndices read the splice of
// the +4/+8 slots -- an access violation in Model::DoesStateExist the moment the
// first streamed TRK_UNIT instance list was walked. The slots are pinned to the
// console's 4 bytes with Ptr32<T> (the low-4 GB PointerFromU32 convention); every
// member is still accessed BY NAME, and the static_assert below is the tripwire.

namespace renderengine { class Texture; }   // Model::UsesTexture param (pointer only)

namespace CgsGraphics
{
    // A renderable instance referenced by the model. Forward-declared only; the
    // concrete RenderWare-backed type is reconstructed in its own TU
    // (CgsRwRenderableResourceType). GetRenderable returns a pointer into the
    // model's renderable table, so an opaque forward declaration is sufficient.
    // RECONCILE 2026-07-24: the renderable's ledger home is GLOBAL-scope ::Renderable
    // (Dispatch/Renderable.h); the old CgsGraphics-scoped forward was the stale guess
    // that header already documents. Alias it in so existing spellings keep working.
    using ::Renderable;

    // CgsModel.h:61 (DWARF)
    const u32 KU32_MAX_NUM_UNIQUE_KEYS = 2048;

    struct Model
    {
        // CgsModel.h:78 (DWARF) - the LOD / game-specific state index space.
        enum State
        {
            E_STATE_LOD_0 = 0,
            E_STATE_LOD_1 = 1,
            E_STATE_LOD_2 = 2,
            E_STATE_LOD_3 = 3,
            E_STATE_LOD_4 = 4,
            E_STATE_LOD_5 = 5,
            E_STATE_LOD_6 = 6,
            E_STATE_LOD_7 = 7,
            E_STATE_LOD_8 = 8,
            E_STATE_LOD_9 = 9,
            E_STATE_LOD_10 = 10,
            E_STATE_LOD_11 = 11,
            E_STATE_LOD_12 = 12,
            E_STATE_LOD_13 = 13,
            E_STATE_LOD_14 = 14,
            E_STATE_LOD_15 = 15,
            E_STATE_GAME_SPECIFIC_0 = 16,
            E_STATE_GAME_SPECIFIC_1 = 17,
            E_STATE_GAME_SPECIFIC_2 = 18,
            E_STATE_GAME_SPECIFIC_3 = 19,
            E_STATE_GAME_SPECIFIC_4 = 20,
            E_STATE_GAME_SPECIFIC_5 = 21,
            E_STATE_GAME_SPECIFIC_6 = 22,
            E_STATE_GAME_SPECIFIC_7 = 23,
            E_STATE_GAME_SPECIFIC_8 = 24,
            E_STATE_GAME_SPECIFIC_9 = 25,
            E_STATE_GAME_SPECIFIC_10 = 26,
            E_STATE_GAME_SPECIFIC_11 = 27,
            E_STATE_GAME_SPECIFIC_12 = 28,
            E_STATE_GAME_SPECIFIC_13 = 29,
            E_STATE_GAME_SPECIFIC_14 = 30,
            E_STATE_GAME_SPECIFIC_15 = 31,
            E_STATE_COUNT = 32,
            E_STATE_INVALID = 32,
        };

        // CgsModel.h:125-129 (DWARF) - model-wide constants.
        static const u8  K_INDEX_UNUSED = 255;
        static const u32 KU_LODCOUNT = 16;
        static const u32 KU_MAX_INSTANCES_PER_GROUP = 5;
        static const u32 KU_OBJECTS_PER_JOB_BLOCK = 128;

        // --- Accessors bodied in CgsModel.cpp (this group) ---

        // CgsModel.h:179
        // Assert-text-attested flag API ("lbInstancing == GetFlag(
        // CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER)", CgsModel.h:412):
        // bit 0 of mu8Flags marks instance-shader models.
        enum EFlag
        {
            E_FLAG_MODEL_USES_INSTANCE_SHADER = 0x01,
        };
        bool GetFlag(EFlag leFlag) const { return (mu8Flags & static_cast<u8>(leFlag)) != 0; }

        const Renderable* GetRenderable(State leState) const;

        // CgsModel.h:186
        bool DoesStateExist(State leState) const;

        // CgsModel.h:210
        f32 GetLodDistance(u32 luLodIndex) const;

        // CgsModel.cpp:555 (the assert file/line the X360 body carries) --
        // SetupShaderConstantsForInstancing @0x827FBB98. STATIC: the console call
        // (RenderRaceCar @0x822D154C) passes the instance count in r3, so there is no
        // `this`. Publishes the per-instance blocks the instancing vertex shader reads:
        // constant 6 "InstancingMatrixArray" and constant 7 "InstancingIndexArray".
        static void SetupShaderConstantsForInstancing(
            s32 liModelInstanceCount,
            const rw::math::vpu::Matrix44Affine* const* lpaModelInstancingArray,
            const rw::math::vpu::Vector4* lpaModelInstancingIndexArray);

        // --- Other declared members (bodied by their own TUs) ---

        // CgsModel.h:182
        u32 GetNumRenderables() const;
        // CgsModel.h:189
        u32 GetVersionNumber() const;
        // CgsModel.h:213
        u32 GetNumLods() const;

        // Does any of this model's renderables reference lpTexture? (X360 0x... ; the texture-pool
        // debug browser counts models/instances that use the selected texture.) Bodied in CgsModel.cpp.
        bool UsesTexture(const renderengine::Texture* lpTexture) const;

    protected:
        // CgsModel.h:268 - table of renderable pointers (mu8NumRenderables long). +0x00
        Ptr32<Ptr32<Renderable> > mppRenderables;
        // CgsModel.h:269 - per-state index into mppRenderables (mu8NumStates long);
        //                  K_INDEX_UNUSED (255) marks an absent state. +0x04
        Ptr32<u8> mpu8StateRenderableIndices;
        // CgsModel.h:270 - per-state LOD switch distance (mu8NumStates long). +0x08
        Ptr32<f32> mpfLodDistances;
        // CgsModel.h:271
        s32 miGameExplorerIndex;   // +0x0C
        // CgsModel.h:272
        u8 mu8NumRenderables;      // +0x10
        // CgsModel.h:273
        u8 mu8Flags;               // +0x11
        // CgsModel.h:274
        u8 mu8NumStates;           // +0x12
        // CgsModel.h:275
        u8 mu8VersionNumber;       // +0x13
    };

    // The console header size ModelResourceType::GetSerialisedResourceDescriptor adds (20).
    static_assert(sizeof(Model) == 20, "CgsGraphics::Model must be the console's 20-byte on-disc header");

    // =========================================================================
    // CgsGraphics::ModelInstanceCollector   (CgsModel.h:279 -- a NAMESPACE, not
    // a class; DecFIGS dwarfdump GameShared/GameClasses/Graphics/CgsModel.cpp
    // declares every member at namespace scope and the X360 asm confirms it:
    // every entry point is a plain function with NO implicit `this`, and all of
    // the state lives in file-scope statics in CgsModel.cpp.)
    //
    // The instanced-batch collector for the prop draw path. A pass brackets its
    // model submissions between BeginInstanceCollection / EndInstanceCollection
    // and drops one ModelInstanceInfo per visible prop with AddInstance; the
    // flush sorts the buffer by (model, LOD state), walks it in runs of at most
    // Model::KU_MAX_INSTANCES_PER_GROUP compatible entries, publishes each run's
    // world matrices into the instancing shader constants and stamps ONE
    // DrawRenderable command for the whole run.
    //
    // X360 entry points (all four in this namespace):
    //   BeginInstanceCollection @ 0x827E6E58   (36 instructions)
    //   AddInstance             @ 0x82801FD0   (37)
    //   EndInstanceCollection   @ 0x82802068   (24)
    //   FlushInstanceCollection @ 0x82801B48   (290)
    // and the two inlined predicates, attested by the DWARF and recovered from
    // the std::sort instantiation / the flush's run scan:
    //   operator<               (CgsModel.cpp:609, inlined into std::_Sort)
    //   AreCompatibleInstances  (CgsModel.cpp:621, inlined into the flush)
    //
    // There is NO collector object to allocate or own: the X360 has exactly one
    // set of statics, so a caller just calls the free functions.
    // =========================================================================
    namespace ModelInstanceCollector
    {
        // CgsModel.cpp:603 -- one buffered prop submission.
        //
        // *** HOST-vs-CONSOLE SIZING ***  The X360 record is 12 bytes (three
        // 32-bit slots) and AddInstance/the flush index it with a literal `12 *
        // i` stride. This is pure runtime scratch -- it is never serialised and
        // never shared with the GPU -- so the host record legitimately grows to
        // 24 bytes (8-byte Model*/Matrix44Affine*). NOTHING may reintroduce the
        // console's 12; every access here is by array subscript + member name.
        struct ModelInstanceInfo
        {
            // CgsModel.cpp:604 -- X360 +0x00 (`stw r30, 0(r11)` in AddInstance)
            Model* mpModel;
            // CgsModel.cpp:605 -- X360 +0x04 (`stw r28, 4(r11)`, the 3rd argument)
            Model::State meLodState;
            // CgsModel.cpp:606 -- X360 +0x08 (`stw r29, 8(r11)`, the 2nd argument)
            const rw::math::vpu::Matrix44Affine* mpMatrix;

            // Never called; pins the member ORDER (the only layout fact that is
            // platform-independent). Deliberately expressed against sizeof(void*)
            // rather than the console's 0/4/8 so it stays true on the x64 host.
            static void _AssertLayout()
            {
                static_assert(offsetof(ModelInstanceInfo, mpModel) == 0,
                              "ModelInstanceInfo::mpModel is the first member");
                static_assert(offsetof(ModelInstanceInfo, meLodState) == sizeof(Model*),
                              "ModelInstanceInfo::meLodState follows mpModel");
                static_assert(offsetof(ModelInstanceInfo, mpMatrix) == 2 * sizeof(void*),
                              "ModelInstanceInfo::mpMatrix is the third member");
            }
        };

        // CgsModel.cpp:600 -- the buffered-submission ceiling; AddInstance
        // auto-flushes when the buffer fills.
        const s32 KI_MAX_INSTANCES_PER_COLLECTION_PASS = 100;

        // CgsModel.cpp:609 -- the std::sort ordering: by model pointer, then by
        // LOD state. (Recovered from the _Med3/_Unguarded_partition
        // instantiations, which compare +0x00 unsigned then +0x04 signed.)
        bool operator<(const ModelInstanceInfo& lrLeft, const ModelInstanceInfo& lrRight);

        // CgsModel.cpp:621 -- may these two share one instanced draw?
        bool AreCompatibleInstances(const ModelInstanceInfo& lrLeft,
                                    const ModelInstanceInfo& lrRight);

        // CgsModel.cpp:664 -- open a collection pass. Argument order/types from
        // the DWARF, confirmed store-for-store against the X360 prologue
        // (r3..r9) and against the sole caller,
        // BrnWorld::PropEntityModule::GenerateDispatchLists @0x822FB4F0, which
        // passes lu8PreZList = 255 ("no pre-Z list").
        void BeginInstanceCollection(DispatchFrame* lpDispatchFrame,
                                     const rw::math::vpu::Matrix44* lpCameraViewProjection,
                                     s32 liModelOnlyDisplayList,
                                     s32 liOpaqueList,
                                     s32 liTransparentList,
                                     u8 lu8PreZList,
                                     bool lbEnableZOnlyRenderPath);

        // CgsModel.cpp:687 -- buffer one prop submission.
        void AddInstance(Model* lpModel,
                         const rw::math::vpu::Matrix44Affine* lpMatrix,
                         Model::State leLodState);

        // CgsModel.cpp:706 -- flush the tail and close the pass.
        void EndInstanceCollection();

        // CgsModel.cpp:714 -- sort + group + draw everything buffered so far.
        void FlushInstanceCollection();
    }
}
