#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"

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
// NOTE: pointer members are 4 bytes on the X360 and 8 bytes on the PC host; do NOT
// pin absolute offsets across them. Members are accessed BY NAME below.

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
        // CgsModel.h:268 - table of renderable pointers (mu8NumRenderables long).
        Renderable** mppRenderables;
        // CgsModel.h:269 - per-state index into mppRenderables (mu8NumStates long);
        //                  K_INDEX_UNUSED (255) marks an absent state.
        u8* mpu8StateRenderableIndices;
        // CgsModel.h:270 - per-state LOD switch distance (mu8NumStates long).
        f32* mpfLodDistances;
        // CgsModel.h:271
        s32 miGameExplorerIndex;
        // CgsModel.h:272
        u8 mu8NumRenderables;
        // CgsModel.h:273
        u8 mu8Flags;
        // CgsModel.h:274
        u8 mu8NumStates;
        // CgsModel.h:275
        u8 mu8VersionNumber;
    };
}
