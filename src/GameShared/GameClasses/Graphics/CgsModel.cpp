#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"  // ShaderConstantTable
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h" // DrawRenderable::AddToBin

#include <algorithm>   // std::sort (the X360 links MSVC's std::_Sort<ModelInstanceInfo*,int>)

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
    // DWARF-ATTESTED NAME (DecFIGS dwarfdump CgsModel.cpp:540):
    //     CgsGraphics::gaTheFirstFewIntegersAsVectors  (Vector4[5])
    //
    // SetupShaderConstantsForInstancing splices ONLY the w lane of these five vectors
    // into constant 7 ("InstancingIndexArray"); the xyz lanes come from the caller.
    // The w lane is the per-instance INDEX the instancing vertex shader uses to pick
    // its matrix out of constant 6, so these five values decide whether a group of N
    // instanced props renders at N transforms or collapses onto matrix[0].
    //
    // RESOLVED 2026-08-12 (conductor, targeted IDA export on a DB copy). The wave's
    // first pass measured all 80 bytes at 0x830111C0 as ZERO and flagged it SUSPECT.
    // That reading was correct but misleading: 0x830111C0 is .bss, so the shipped image
    // is zero BY DEFINITION and the real values are written at runtime by a C++ dynamic
    // initialiser. IDA never turned that region into a named function, which is why a
    // whole-export pseudocode scan found no writer.
    //
    // The initialiser is at 0x82C6BFE0. It builds five Vector4s on the stack, filling
    // every xyz lane from flt_82001CC0 and each w lane from a different constant, then
    // stores the block to unk_830111C0 (0x82C6C054..0x82C6C060). Reading those five
    // constants out of the database settles it:
    //     flt_82001CC0 = 0x00000000 = 0.0f   (v0.w, and every xyz lane)
    //     flt_82001C98 = 0x3F800000 = 1.0f   (v1.w)
    //     flt_82001D9C = 0x40000000 = 2.0f   (v2.w)
    //     flt_82004270 = 0x40400000 = 3.0f   (v3.w)
    //     flt_82004EF4 = 0x40800000 = 4.0f   (v4.w)
    // i.e. exactly the 0,1,2,3,4 the DWARF name promises. Had the measured-zero table
    // shipped, every instanced prop group would have drawn N props on top of each other.
    // ------------------------------------------------------------------------
    static const rw::math::vpu::Vector4
    gaTheFirstFewIntegersAsVectors[Model::KU_MAX_INSTANCES_PER_GROUP] =
    {
        { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 2.0f },
        { 0.0f, 0.0f, 0.0f, 3.0f }, { 0.0f, 0.0f, 0.0f, 4.0f },
    };

    // ========================================================================
    // Model::SetupShaderConstantsForInstancing  @ 0x827FBB98
    //
    // Publish one instanced draw group's per-instance data into the shader-constant
    // table. Two blocks, in the console's order (7 then 6):
    //
    //   constant 7 "InstancingIndexArray"  (5 x Vector4, declared size 16 x 5)
    //       entry i = { lpaModelInstancingIndexArray[i].xyz,
    //                   gaTheFirstFewIntegersAsVectors[i].w }
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
            laIndexArray[luEntry].w = gaTheFirstFewIntegersAsVectors[luEntry].w;
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

// =============================================================================
// CgsGraphics::ModelInstanceCollector -- the instanced-batch collector.
//
// A NAMESPACE of free functions over file-scope state, not a class (DecFIGS
// dwarfdump CgsModel.cpp:600-648; the X360 prologues take no `this`). All four
// entry points carry assert strings naming this very file
// ("..\..\..\GameShared\GameClasses\Graphics/CgsModel.cpp" lines 668 / 689 /
// 708), so this is their original home and they are gathered here rather than
// in a new TU.
// =============================================================================
namespace ModelInstanceCollector
{
    // ---- Collection state (X360 .bss, CgsModel.cpp:628-641) -----------------
    // The console addresses are recorded only to tie each name to the asm; NO
    // console offset or stride is used anywhere below -- every access goes
    // through a named object or an array subscript.

    // 0x83010F94 -- Begin sets, End clears; Add/End/Flush assert on it.
    bool sbIsDuringInstanceCollection = false;
    // 0x83010F98 -- how many entries of the buffer are live.
    int siInstanceRenderingBufferLength = 0;
    // 0x83010A70 -- the submission buffer (console: 100 * 12 bytes).
    ModelInstanceInfo saInstanceRenderingBufferArray[KI_MAX_INSTANCES_PER_COLLECTION_PASS];
    // 0x83010A44 -- one group's world matrices, gathered for the shader upload.
    // Console span 0x83010A44..0x83010A58 == 5 * 4 bytes, which is what pins the
    // group ceiling at Model::KU_MAX_INSTANCES_PER_GROUP.
    const rw::math::vpu::Matrix44Affine* saInstanceGroupMatrixArray[Model::KU_MAX_INSTANCES_PER_GROUP];
    // 0x83011310 -- one group's per-instance stream data (constant 7's xyz lanes).
    rw::math::vpu::Vector4 saInstanceGroupStreamDataArray[Model::KU_MAX_INSTANCES_PER_GROUP];

    // ---- Pass parameters, latched by BeginInstanceCollection ----------------
    CgsGraphics::DispatchFrame* spDispatchFrame = 0;            // 0x83010A64
    // 0x83010A58. Latched by Begin and read by NO recovered function -- faithful
    // dead state, kept because the store is in the asm.
    const rw::math::vpu::Matrix44* spCameraViewProjection = 0;
    s32  siModelOnlyDisplayList = 0;                            // 0x83010A5C
    s32  siOpaqueList = 0;                                      // 0x83010A6C
    s32  siTransparentList = 0;                                 // 0x83010A68
    u8   su8PreZList = 0;                                       // 0x83010A60
    bool sbEnableZOnlyRenderPath = false;                       // 0x83010A61

    // ---- Tunables (X360 .data @ 0x82F30F58..0x82F30F64, CgsModel.cpp:645-648)
    // These four live in INITIALISED data (their neighbours at 0x82F30F4C..54 are
    // relocated string pointers), so each shipped non-zero; the flush reloads
    // them every iteration, which is why they are mutable globals here too
    // rather than folded constants.
    int  siMaxInstancesPerCollectionPass = KI_MAX_INSTANCES_PER_COLLECTION_PASS;  // 0x82F30F58
    int  siMaxInstancesPerGroup = static_cast<int>(Model::KU_MAX_INSTANCES_PER_GROUP); // 0x82F30F5C
    // 0x82F30F60. A run shorter than this is dropped instead of drawn. A run is
    // always >= 1 entry long, so the only shipped values that keep props on
    // screen are 0 or 1 -- behaviourally identical. Modelled as 1.
    int  siMinInstancesPerGroup = 1;
    // 0x82F30F64. When false the flush treats the buffer as empty (and clears
    // it), i.e. the whole instanced path is off.
    bool sbInstanceRenderingEnabled = true;

    // -------------------------------------------------------------------------
    // operator<  (CgsModel.cpp:609) -- inlined into the std::sort instantiation
    // std::_Sort<ModelInstanceInfo*,int> @0x82801A38. Recovered from _Med3
    // @0x827EFF78, which compares +0x00 with `cmplw` (UNSIGNED -- a pointer) and
    // falls through to +0x04 with `cmpw` (SIGNED -- the State enum) on a tie.
    // -------------------------------------------------------------------------
    bool operator<(const ModelInstanceInfo& lrLeft, const ModelInstanceInfo& lrRight)
    {
        if (lrLeft.mpModel != lrRight.mpModel)
        {
            return lrLeft.mpModel < lrRight.mpModel;
        }
        return lrLeft.meLodState < lrRight.meLodState;
    }

    // -------------------------------------------------------------------------
    // AreCompatibleInstances (CgsModel.cpp:621) -- inlined into the flush's run
    // scan @0x82801CB8: same model AND same LOD state, nothing else.
    // -------------------------------------------------------------------------
    bool AreCompatibleInstances(const ModelInstanceInfo& lrLeft,
                                const ModelInstanceInfo& lrRight)
    {
        return lrLeft.mpModel == lrRight.mpModel
            && lrLeft.meLodState == lrRight.meLodState;
    }

    // =========================================================================
    // BeginInstanceCollection @ 0x827E6E58  (36 instructions)
    //
    // Latch the pass parameters and open the collection. Store order is exactly
    // the asm's (frame, viewProj, modelOnlyList, opaqueList, transparentList,
    // length=0, preZList, zOnlyPath, flag=1) at 0x827E6EB8..0x827E6EDC.
    // =========================================================================
    void BeginInstanceCollection(CgsGraphics::DispatchFrame* lpDispatchFrame,
                                 const rw::math::vpu::Matrix44* lpCameraViewProjection,
                                 s32 liModelOnlyDisplayList,
                                 s32 liOpaqueList,
                                 s32 liTransparentList,
                                 u8 lu8PreZList,
                                 bool lbEnableZOnlyRenderPath)
    {
        CGS_ASSERT(!sbIsDuringInstanceCollection, "!sbIsDuringInstanceCollection");

        spDispatchFrame                 = lpDispatchFrame;
        spCameraViewProjection          = lpCameraViewProjection;
        siModelOnlyDisplayList          = liModelOnlyDisplayList;
        siOpaqueList                    = liOpaqueList;
        siTransparentList               = liTransparentList;
        siInstanceRenderingBufferLength = 0;
        su8PreZList                     = lu8PreZList;
        sbEnableZOnlyRenderPath         = lbEnableZOnlyRenderPath;
        sbIsDuringInstanceCollection    = true;
    }

    // =========================================================================
    // AddInstance @ 0x82801FD0  (37 instructions)
    //
    // Buffer one submission and auto-flush when the buffer is full. The asm
    // writes the record in the order +0x00 (model), +0x08 (matrix), +0x04
    // (state) -- i.e. argument order 1, 2, 3 -- and compares the POST-increment
    // length against siMaxInstancesPerCollectionPass, so the flush happens after
    // the last record has been written.
    // =========================================================================
    void AddInstance(Model* lpModel,
                     const rw::math::vpu::Matrix44Affine* lpMatrix,
                     Model::State leLodState)
    {
        CGS_ASSERT(sbIsDuringInstanceCollection, "sbIsDuringInstanceCollection");

        ModelInstanceInfo& lrInfo =
            saInstanceRenderingBufferArray[siInstanceRenderingBufferLength];
        ++siInstanceRenderingBufferLength;

        lrInfo.mpModel    = lpModel;
        lrInfo.mpMatrix   = lpMatrix;
        lrInfo.meLodState = leLodState;

        if (siInstanceRenderingBufferLength == siMaxInstancesPerCollectionPass)
        {
            FlushInstanceCollection();
        }
    }

    // =========================================================================
    // EndInstanceCollection @ 0x82802068  (24 instructions)
    // =========================================================================
    void EndInstanceCollection()
    {
        CGS_ASSERT(sbIsDuringInstanceCollection, "sbIsDuringInstanceCollection");

        FlushInstanceCollection();
        sbIsDuringInstanceCollection = false;
    }

    // =========================================================================
    // FlushInstanceCollection @ 0x82801B48  (290 instructions)
    //
    // Sort the buffer, then walk it in runs of at most siMaxInstancesPerGroup
    // compatible entries and emit one instanced DrawRenderable per run.
    //
    // ASM DECODE NOTES (things the pseudocode hides):
    //  * 0x82801BB0..0x82801BC4 -- std::sort's three arguments are
    //    (first, first + length, (last - first) / 12); the divide by the record
    //    size is the giveaway that this is MSVC's std::_Sort(_First,_Last,_Ideal),
    //    i.e. plain `std::sort(first, last)`. The console 12 is sizeof() on the
    //    console; the host must never see it.
    //  * 0x82801D28 + 0x82801D50 -- the `stvx128 v127(zero)` followed by a
    //    self-overlapping `_blkmov(arr+16, arr, (16*n - 9) & ~7)` is the
    //    compiler's zero-fill of the first n Vector4s of the group stream-data
    //    array: 16 zeroed bytes propagated forward over 16*n-16 more.
    //  * 0x82801F40 -- the dispatch list's submitted-packet count is read BEFORE
    //    BeginPacket; `clrlwi r11,r29,25 ; cntlzw ; extrwi r5,r11,1,26` is the
    //    strength-reduced `(count % 128) == 0` (Model::KU_OBJECTS_PER_JOB_BLOCK)
    //    "resend every shader constant" cadence.
    //  * 0x82801F50..0x82801F84 -- the four stack-passed AddToBin arguments land
    //    at caller-sp +0x57 / +0x5F / +0x64 / +0x6F, which are exactly the
    //    `arg_57 / arg_5F / arg_64 / arg_6F` the callee reads at 0x827FA21C..
    //    0x827FA234 into the DrawRenderableDispatchThreadInfo trailer -- so they
    //    are (preZList, preZTechnique, instanceCount, excludeMeshBits) in that
    //    declaration order.
    //  * 0x82801F88..0x82801F9C -- `lwz r5,0x10(bin); stw 0,0x10(bin);
    //    stw 0,0x14(bin)` is DispatchBin::EndPacket() inlined, and its result is
    //    Submit's packet argument with sort key 0.
    // =========================================================================
    void FlushInstanceCollection()
    {
        s32 liInstanceCount;
        if (sbInstanceRenderingEnabled)
        {
            liInstanceCount = siInstanceRenderingBufferLength;
        }
        else
        {
            liInstanceCount = 0;
            siInstanceRenderingBufferLength = 0;
        }

        std::sort(saInstanceRenderingBufferArray,
                  saInstanceRenderingBufferArray + liInstanceCount);

        s32 liGroupFirst = 0;
        while (liGroupFirst < liInstanceCount)
        {
            // Extend the run while the entries stay compatible, but never past
            // siMaxInstancesPerGroup entries (nor past the buffer's end).
            s32 liGroupLimit = liGroupFirst + siMaxInstancesPerGroup;
            if (liGroupLimit > liInstanceCount)
            {
                liGroupLimit = liInstanceCount;
            }

            s32 liGroupLast = liGroupFirst + 1;
            while (liGroupLast < liGroupLimit
                   && AreCompatibleInstances(saInstanceRenderingBufferArray[liGroupFirst],
                                             saInstanceRenderingBufferArray[liGroupLast]))
            {
                ++liGroupLast;
            }

            const ModelInstanceInfo& lrHead = saInstanceRenderingBufferArray[liGroupFirst];
            Model* const lpModel = lrHead.mpModel;
            const Model::State leLodState = lrHead.meLodState;
            const rw::math::vpu::Matrix44Affine* const lpHeadMatrix = lrHead.mpMatrix;
            s32 liGroupCount = liGroupLast - liGroupFirst;

            if (liGroupCount > 0)
            {
                for (s32 liEntry = 0; liEntry < liGroupCount; ++liEntry)
                {
                    saInstanceGroupStreamDataArray[liEntry].SetZero();
                }
                for (s32 liEntry = 0; liEntry < liGroupCount; ++liEntry)
                {
                    saInstanceGroupMatrixArray[liEntry] =
                        saInstanceRenderingBufferArray[liGroupFirst + liEntry].mpMatrix;
                }
            }

            if (liGroupCount >= siMinInstancesPerGroup)
            {
                // ---------------------------------------------------------
                // Model::RenderModelOnly INLINED (the X360 asserts here name
                // "...\graphics\CgsModel.h" lines 399/400/403/404/409/410/412,
                // i.e. the header's inline definition -- the DWARF declares it
                // at CgsModel.h:158 with 16 parameters).
                //
                // PARKED, deliberately: there is NO out-of-line RenderModelOnly
                // symbol anywhere in the X360 export, so the only evidence for
                // its body is this expansion plus four others (RenderModel
                // @0x822C4918, RenderRaceCar @0x822CF6A0, RenderTrafficCar
                // @0x82728B08, WorldEntityModule::GenerateDispatchLists
                // @0x822D5AB0). This expansion has the transform, LOD-select and
                // non-instanced branches folded away, so de-inlining it into a
                // general 16-argument member from THIS site alone would be
                // invention. It is reproduced verbatim here and left for the TU
                // that reconciles all five expansions.
                // ---------------------------------------------------------
                CGS_ASSERT(leLodState < Model::E_STATE_COUNT, "leState < E_STATE_COUNT");

                // GetRenderable is mppRenderables[mpu8StateRenderableIndices[state]]
                // and carries the inline's own "State does not exist" bound check
                // (index != 255 && index < mu8NumRenderables) -- 0x82801DC0..0x82801E04
                // is that accessor, byte for byte.
                const Renderable* lpRenderable = lpModel->GetRenderable(leLodState);
                CGS_ASSERT(lpRenderable != 0, "Missing renderable in a model");
                CGS_ASSERT(spDispatchFrame != 0, "lpDispatchFrame");

                DispatchList* lpDispatchList =
                    spDispatchFrame->GetList(static_cast<u32>(siModelOnlyDisplayList));
                // X360: `addi r31, r26, 0x80` -- the frame's embedded bin.
                DispatchBin* lpDispatchBin = &spDispatchFrame->GetBin();
                CGS_ASSERT(lpDispatchBin != 0, "lpDispatchBin");
                CGS_ASSERT(lpDispatchList != 0, "lpDispatchList");
                CGS_ASSERT(lpModel->GetFlag(Model::E_FLAG_MODEL_USES_INSTANCE_SHADER),
                           "lbInstancing == GetFlag(CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER)");

                // 0x82801F18: the empty-run fallback -- draw the head entry on
                // its own out of a one-element stack array. Unreachable while a
                // run is always >= 1 long, but it is in the binary.
                const rw::math::vpu::Matrix44Affine* laSingleInstanceMatrix[1];
                const rw::math::vpu::Matrix44Affine* const* lpaGroupMatrices =
                    saInstanceGroupMatrixArray;
                if (liGroupCount == 0)
                {
                    laSingleInstanceMatrix[0] = lpHeadMatrix;
                    lpaGroupMatrices = laSingleInstanceMatrix;
                    liGroupCount = 1;
                }

                Model::SetupShaderConstantsForInstancing(liGroupCount, lpaGroupMatrices,
                                                         saInstanceGroupStreamDataArray);

                const bool lbSendAllShaderConstants =
                    (lpDispatchList->GetCount() % Model::KU_OBJECTS_PER_JOB_BLOCK) == 0u;

                lpDispatchBin->BeginPacket();
                // Argument roles are positional against the X360 register/stack
                // assignment at 0x82801F48..0x82801F84 (see the decode note
                // above). Note r9 -- the byte the command word packs as the
                // technique index -- receives `sbEnableZOnlyRenderPath ? 1 : 0`,
                // the same flag that r10 receives as the Z-only bool; that is
                // what the binary does, oddity and all.
                DrawRenderable::AddToBin(lpRenderable,
                                         spDispatchFrame,
                                         lbSendAllShaderConstants,
                                         static_cast<s8>(siOpaqueList),
                                         static_cast<s8>(siTransparentList),
                                         0u,                                             // frustum-enable byte
                                         sbEnableZOnlyRenderPath ? 1u : 0u,               // technique byte
                                         sbEnableZOnlyRenderPath,                         // lbZOnly
                                         su8PreZList,                                     // caller-sp +0x57
                                         1u,                                              // caller-sp +0x5F
                                         liGroupCount,                                    // caller-sp +0x64
                                         0u);                                             // caller-sp +0x6F

                DispatchCommand* lpPacket = lpDispatchBin->EndPacket();
                lpDispatchList->Submit(0, lpPacket);
            }

            liGroupFirst = liGroupLast;
        }

        siInstanceRenderingBufferLength = 0;
    }
}
}
