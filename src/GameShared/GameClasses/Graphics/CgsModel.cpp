#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"  // ShaderConstantTable

// CgsModel.cpp - the three CgsGraphics::Model accessors bodied store-for-store from
// the X360 asm (GetRenderable @0x822A0AC8, DoesStateExist @0x822A0B60,
// GetLodDistance @0x822A0C00). The DWARF source paths report these at
// GameShared/GameClasses/Graphics/CgsModel.h:309/310, :344 and :367 (the asserts
// are inline accessors in the header on console); they are gathered into this TU.
//
// All member reads are BY NAME; offsets quoted in CgsModel.h match the asm
// (lbz 0x10/0x12, lwz 0/4/8 of `this`).

namespace CgsGraphics
{
    // GetRenderable @ 0x822A0AC8
    //   lbz   r11, 0x12(this)            ; mu8NumStates
    //   cmpw  leState, r11 ; assert leState < mu8NumStates
    //   lwz   r11, 4(this) ; lbzx r11, r11, leState  -> mpu8StateRenderableIndices[leState]
    //   assert !(index == 255 || index >= mu8NumRenderables)  -> "State does not exist"
    //   lwz   r11, 4(this) ; lwz r10, 0(this)
    //   lbzx  r11, r11, leState ; rotlwi r11, r11, 2 ; lwzx r3, r11, r10
    //     -> mppRenderables[index]  (rotlwi by 2 == *4 byte stride of a 32-bit pointer table)
    const Renderable* Model::GetRenderable(State leState) const
    {
        CGS_ASSERT(leState < mu8NumStates, "leState < mu8NumStates");

        const u32 luIndex = mpu8StateRenderableIndices[leState];
        CGS_ASSERT(!(luIndex == 255u || luIndex >= mu8NumRenderables), "State does not exist");

        return mppRenderables[mpu8StateRenderableIndices[leState]];
    }

    // DoesStateExist @ 0x822A0B60
    //   cmpwi leState, 0x20 ; assert leState < E_STATE_COUNT
    //   lbz   r10, 0x12(this)                 ; mu8NumStates
    //   r11 = mu8NumStates - 1 ; if (leState < mu8NumStates - 1) r11 = leState
    //     -> clamped index = min(leState, mu8NumStates - 1)
    //   lwz   r9, 4(this) ; lbzx r11, r9, r11 ; r11 -= 255 ; cntlzw ; extract bit ->
    //     bit set iff index == 255 ; xori 1 -> (index != 255)
    //   result = (leState < mu8NumStates) & (clampedIndex != 255)
    bool Model::DoesStateExist(State leState) const
    {
        CGS_ASSERT(leState < E_STATE_COUNT, "leState < E_STATE_COUNT");

        u32 luClampedIndex = mu8NumStates - 1u;
        if (static_cast<u32>(leState) < mu8NumStates - 1u)
        {
            luClampedIndex = static_cast<u32>(leState);
        }

        const bool lbInRange = static_cast<u32>(leState) < mu8NumStates;
        const bool lbUsed = mpu8StateRenderableIndices[luClampedIndex] != 255u;
        return lbInRange && lbUsed;
    }

    // GetLodDistance @ 0x822A0C00
    //   lbz   r11, 0x12(this) ; cmplw luLodIndex, r11 ; assert luLodIndex < mu8NumStates
    //   lwz   r11, 8(this) ; slwi r10, luLodIndex, 2 ; lfsx f1, r10, r11
    //     -> mpfLodDistances[luLodIndex]  (single-precision load; returned in f1)
    f32 Model::GetLodDistance(u32 luLodIndex) const
    {
        CGS_ASSERT(luLodIndex < mu8NumStates, "Invalid LOD index");

        return mpfLodDistances[luLodIndex];
    }

    // The three trivial field accessors the DWARF declares as header inlines
    // (CgsModel.h:182 / :189 / :213 -- no out-of-line X360 symbols exist, they are
    // inlined into every caller). They are gathered here beside the other three
    // accessors; each is a single named-field read, pinned by the same asm the
    // accessors above quote:
    //   GetNumLods        -> mu8NumStates       (lbz 0x12; every LOD walk bounds on it,
    //                                            and GetLodDistance asserts against it)
    //   GetNumRenderables -> mu8NumRenderables  (lbz 0x10; GetRenderable's index bound)
    //   GetVersionNumber  -> mu8VersionNumber   (lbz 0x13)
    // (They previously resolved to WorldLinkStubs "return 0" gates, which made every
    // streamed instance fail RenderInstance's "Model in unit has no lods!" assert.)
    u32 Model::GetNumLods() const
    {
        return mu8NumStates;
    }

    u32 Model::GetNumRenderables() const
    {
        return mu8NumRenderables;
    }

    u32 Model::GetVersionNumber() const
    {
        return mu8VersionNumber;
    }

    // The global runtime shader-constant register (X360 symbol mShaderConstantTable,
    // bodied by the CgsShaderConstants TU). Same extern the other consumers carry.
    extern ::ShaderConstantTable mShaderConstantTable;

    // ------------------------------------------------------------------------
    // The .w-lane source for shader constant 7 -- the X360's `unk_830111C0`.
    //
    // SetupShaderConstantsForInstancing splices ONLY the w lane of these five vectors
    // into constant 7 ("InstancingIndexArray"); the xyz lanes come from the caller.
    // MEASURED, not assumed: the shipped image has all 80 bytes at 0x830111C0 ZERO, and
    // a whole-export scan for the symbol finds exactly ONE reference -- this read. So on
    // the console the splice contributes 0.0f, i.e. it clears the w lane. Modelled as the
    // zero-initialised file-scope table it is, rather than folded away, so that the day a
    // writer turns up (the export set is known to have holes) there is a named thing to
    // fill in.
    // ------------------------------------------------------------------------
    static const rw::math::vpu::Vector4
    saInstancingIndexWLanes[Model::KU_MAX_INSTANCES_PER_GROUP] =
    {
        { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f },
    };

    // ========================================================================
    // Model::SetupShaderConstantsForInstancing  @ 0x827FBB98
    //
    // Publish one instanced draw group's per-instance data into the shader-constant
    // table. Two blocks, in the console's order (7 then 6):
    //
    //   constant 7 "InstancingIndexArray"  (5 x Vector4, declared size 16 x 5)
    //       entry i = { lpaModelInstancingIndexArray[i].xyz, saInstancingIndexWLanes[i].w }
    //       -- the five `vrlimi128 vX, vY, 1, 0` inserts (mask 1 == lane w) at
    //       0x827FBBF8..0x827FBC30, then the inlined Vector4* SetShaderConstantArrayData
    //       (the CgsShaderConstants.h:492/:496 assert pair + FastNonOverlappedVectorMemcpy
    //       of maConstants[7].mu8NumEntries quad-words).
    //
    //   constant 6 "InstancingMatrixArray" (5 x Matrix44Affine, declared size 64 x 5)
    //       entries 0..count-1 = *lpaModelInstancingArray[i]  (a straight 64-byte copy;
    //       the argument really is an array of POINTERS to matrices -- `_R10 = *v23`
    //       then four lvx128 off _R10)
    //       entries count..4    = zero  (the `vspltisw v0, 0` fill loop)
    //       uploaded through the Matrix44Affine* SetShaderConstantArrayData (sub_827FB918).
    //
    // Both blocks are always FIVE entries long because that is what the table declares;
    // that is exactly why the tail is zero-filled instead of left alone.
    // ========================================================================
    void Model::SetupShaderConstantsForInstancing(
        s32 liModelInstanceCount,
        const rw::math::vpu::Matrix44Affine* const* lpaModelInstancingArray,
        const rw::math::vpu::Vector4* lpaModelInstancingIndexArray)
    {
        CGS_ASSERT(liModelInstanceCount <= static_cast<s32>(KU_MAX_INSTANCES_PER_GROUP),
                   "liModelInstanceCount <= int32_t(Model::KU_MAX_INSTANCES_PER_GROUP)");
        CGS_ASSERT(lpaModelInstancingArray != 0, "lpaModelInstancingArray != NULL");

        rw::math::vpu::Vector4        laIndexArray[KU_MAX_INSTANCES_PER_GROUP];
        rw::math::vpu::Matrix44Affine laMatrixArray[KU_MAX_INSTANCES_PER_GROUP];

        for (u32 luEntry = 0; luEntry < KU_MAX_INSTANCES_PER_GROUP; ++luEntry)
        {
            laIndexArray[luEntry].x = lpaModelInstancingIndexArray[luEntry].x;
            laIndexArray[luEntry].y = lpaModelInstancingIndexArray[luEntry].y;
            laIndexArray[luEntry].z = lpaModelInstancingIndexArray[luEntry].z;
            laIndexArray[luEntry].w = saInstancingIndexWLanes[luEntry].w;
        }

        for (s32 liInstance = 0; liInstance < liModelInstanceCount; ++liInstance)
        {
            laMatrixArray[liInstance] = *lpaModelInstancingArray[liInstance];
        }
        for (u32 luPad = static_cast<u32>(liModelInstanceCount);
             luPad < KU_MAX_INSTANCES_PER_GROUP; ++luPad)
        {
            laMatrixArray[luPad].SetZero();
        }

        mShaderConstantTable.SetShaderConstantArrayData(7u, laIndexArray);
        mShaderConstantTable.SetShaderConstantArrayData(6u, laMatrixArray);
    }
}
