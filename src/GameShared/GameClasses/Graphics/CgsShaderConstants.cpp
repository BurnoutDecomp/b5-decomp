#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (SetSize / SetNumEntries)
// renderengine::ProgramBufferData / ProgramVariableHandle / ProgramBuffer::GetVariableHandleByName
// -- the program-side half of ShaderConstantsExternal::FixUp(const ProgramBuffer*).
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint / gxMessageFilterFlags
#include <cstring>                                          // strcmp (the PC instancing substitute table)

// CgsShaderConstants.cpp - functions of the CGS shader-constant subsystem:
//   * ShaderConstantsExternal::FixUp / HasShaderConstant  (on-disk External block)
//   * ShaderConstantsInternal::FixUp                      (on-disk Internal block)
//   * ShaderConstantTable::BeginFrame                     (runtime: prime dirty list)
//   * ShaderConstantTable::AddDirtyConstantsToDispatchBin (runtime: drain dirty list)
//   * ShaderConstantTableElement::SetSize / SetNumEntries (recompute cached qw array size)
//
// Reconstructed from the DecFIGS DWARF + X360 pseudocode. The relocation FixUps add the
// load base (a 32-bit X360 address) to every internal pointer; on PC we replicate the
// exact 32-bit pointer arithmetic via uintptr-sized words so the on-disk layout and the
// console behaviour are preserved. The FixUp/frame functions do not assert; the two
// ShaderConstantTableElement setters do (CGS_ASSERT: luNumQwInArray <= 0xFFFF).

// ============================================================================================
// *** SERIALISED-BLOCK SEAM (world-data wave 2026-07-27) ***
// The three ShaderConstants* blocks are SERIALISED RESOURCE DATA, not host objects: they live
// inside a Material / ShaderTechnique blob exactly as the console laid them out, i.e. a run of
// 32-BIT words. The consumer that owns that blob agrees --
// CgsResource::MaterialResourceType::FixUp walks the material with explicit `u32` slot pokes
// (+0x00/+0x0C/+0x10/+0x14/+0x18, sampler stride 20) and the world-data porter emits the
// console 32-bit form for Material/MaterialTechnique/TextureState/VertexDescriptor.
//
// The struct declarations in CgsShaderConstants.h are the HOST-width (x64) shape, so walking a
// serialised block through them reads each pointer member at twice its real offset. That is
// exactly what killed the first TRK_UNIT52_GR.BNDL load: MaterialResourceType::FixUp ->
// ShaderConstantsInternal::FixUp read mppaConstantsInstanceData across the +0x10/+0x14 word
// pair and dereferenced 0x01000000_04769674.
//
// So every function below that touches a block *as loaded from disc* goes through the
// SerialisedInternal / SerialisedExternal / SerialisedCPU views: the console word layout, with
// the u32 slots resolved through the project's low-4 GB PointerFromU32 convention (the engine's
// whole root allocation is reserved below 4 GB -- see CgsMemory::LowMemory).
//
// FLAG (open): the RUNTIME accessors that are not yet bodied (ShaderConstantsInternal::
// GetValue/GetConstant/Dispatch*ShaderConstants) must use the same view when they land -- do
// NOT read the host-width members on a streamed block.
// ============================================================================================
// The single global runtime table (X360 `mShaderConstantTable` @ 0x830113D0, inside namespace
// CgsGraphics). Its storage currently lives in GameSource/World/WorldLinkStubs.cpp; every
// consumer (BrnRendererModule / BrnWorldModule / BrnShadowMap / CgsDispatcherCommands)
// declares it exactly like this.
namespace CgsGraphics { extern ShaderConstantTable mShaderConstantTable; }

namespace
{
    // Console word layout of ShaderConstantsInternal (5 words).
    struct SerialisedInternal
    {
        u32 muNumConstantsInstances;   // +0x00
        u32 muConstantsInstanceSize;   // +0x04  u32*
        u32 muConstantsInstanceData;   // +0x08  u32**
        u32 muNamesHash;               // +0x0C  u32*
        u32 muProgramStateHandles;     // +0x10  ProgramVariableHandle*
    };

    // Console word layout of ShaderConstantsExternal (4 words).
    struct SerialisedExternal
    {
        u32 muNumConstantsInstances;   // +0x00
        u32 muConstantsInstanceData;   // +0x04  u32*
        u32 muNames;                   // +0x08  const char**
        u32 muProgramStateHandles;     // +0x0C  ProgramVariableHandle*
    };

    // Console word layout of ShaderConstantsCPU (4 words).
    struct SerialisedCPU
    {
        u32 muCPUShader;               // +0x00  ICPUShader* (nulled by FixUp)
        u32 muNumConstantsInstances;   // +0x04
        u32 muConstantsInstanceData;   // +0x08  u32**
        u32 muNames;                   // +0x0C  const char**
    };

    inline u32* SlotArray(u32 luSlot)
    {
        return reinterpret_cast<u32*>(static_cast<uintptr_t>(luSlot));
    }
    inline const char* SlotString(u32 luSlot)
    {
        return reinterpret_cast<const char*>(static_cast<uintptr_t>(luSlot));
    }

    // ============================================================================================
    // [FLAG PC data gap] THE INSTANCING SUBSTITUTE TABLE.
    //
    // The PC programs are RE-COMPILED (tools/assets/shaders/convert_shaders_bundle.py) from the
    // TUB HLSL, and for 19 of the 110 techniques the TUB source is the NON-instanced variant of
    // the shader the console shipped. Their technique records still list the console's two
    // per-instance constants, so both lookups below fail and the object matrix never reaches the
    // program -- the "real programs bound, zero pixels" measurement the wheel bring-up recorded.
    //
    // The substitution is not a guess; both halves are decoded from the shipped data:
    //
    //   InstancingMatrixArray -> world
    //       The console wheel VS (X360 SHADERS.BNDL resource AB4DE44C, disassembled with
    //       tools/assets/shaders/xenos.py) builds its world position from
    //           c[a0+4..a0+7],  a0 = 4 * InstancingIndexArray[instance].w
    //       i.e. InstancingMatrixArray IS the per-draw object matrix, four registers of it.
    //       The PC twin does `mad r1.xyz, v0.x, c20, r1` ... `add r0.xyz, r1, c23` with
    //       `world` at c20, count 4 (fxc /dumpbin) -- the same four registers under the
    //       engine's other name for the same datum.
    //
    //   InstancingIndexArray  -> g_wheelConstants
    //       The console VS exports `c[a0+24].x` (a0 = instance) on interpolator 2 lane z, and
    //       Model::SetupShaderConstantsForInstancing fills those xyz lanes from the CALLER --
    //       for the wheels, RenderRaceCar's per-wheel spin-blur factor. The PC twin ends with
    //       `mov o3.z, c31.x`, c31 = `g_wheelConstants`, and its pixel shader consumes that lane
    //       as `lrp r4, v2.z, <blurred texture>, <sharp texture>`. Same value, same lane, same
    //       consumer; only the register moved.
    //
    // The SOURCE stays the console's own constant (table slot 6 / 7) -- only the destination
    // register moves -- so nothing about how the data is produced changes. For an expanded
    // console-instanced draw, DrawRenderable::Interpret points those two slots at the entry the
    // draw belongs to (see its "PER-INSTANCE CONSTANT CHANNELS" block).
    //
    // A technique that lists an instancing constant lists NOTHING ELSE in its vertex OBJECT
    // block -- measured over the whole shipped bundle: 19 blocks are exactly
    // [InstancingIndexArray, InstancingMatrixArray], 75 are [world], 16 are [.., world] -- so
    // this can never collide with a technique that binds `world` for real.
    //
    // DELETE-WHEN the PC program set is compiled from sources that implement the console
    // instancing scheme (they need manual vertex fetch, i.e. not D3D9 SM3).
    // ============================================================================================
    const char* PCInstancingSubstituteName(const char* lpcConsoleName)
    {
        if (lpcConsoleName == 0)
        {
            return 0;
        }
        if (std::strcmp(lpcConsoleName, "InstancingMatrixArray") == 0)
        {
            return "world";
        }
        if (std::strcmp(lpcConsoleName, "InstancingIndexArray") == 0)
        {
            return "g_wheelConstants";
        }
        return 0;
    }
}

// CgsShaderConstants.cpp:54-56
// File-static shadowing-policy flags. The X360 build compiles these as constant globals
// in this TU (used by the dispatch helpers reconstructed in other TUs).
namespace ShaderConstantShadowing
{
    const bool gbShadowingByValueIntoDispatchBin = false;
    const bool gbShadowingVertexConstantByValueIntoPushBuffer = false;
    const bool gbShadowingPixelConstantByValueIntoPushBuffer = false;
}

// CgsShaderConstants.cpp:1084
// Relocate the External block's internal pointers by the load base. word1 (instance data),
// word2 (name array) and word3 (program handles) each get the base added; additionally
// every non-null entry of the name array (word2) is relocated. The X360 returns int 0; the
// header signature is u32, matching the DWARF's u32 FixUp(u8*).
u32 ShaderConstantsExternal::FixUp(u8* lpBaseData)
{
    // SERIALISED-BLOCK SEAM (see the banner): walk the console word layout.
    SerialisedExternal* const lpBlock = reinterpret_cast<SerialisedExternal*>(this);
    const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lpBaseData));

    lpBlock->muConstantsInstanceData += luDelta;
    lpBlock->muProgramStateHandles   += luDelta;
    lpBlock->muNames                 += luDelta;

    if (lpBlock->muNumConstantsInstances != 0)
    {
        u32* const lpaNames = SlotArray(lpBlock->muNames);
        for (u32 luIndex = 0; luIndex < lpBlock->muNumConstantsInstances; ++luIndex)
        {
            if (lpaNames[luIndex] != 0)
            {
                lpaNames[luIndex] += luDelta;
            }
        }
    }

    return 0;
}

// =============================================================================================
// ShaderConstantsExternal::FixUp(const ProgramBuffer*)   X360 @ 0x827ED8D0 (sub_827ED8D0)
// DWARF home: CgsShaderConstants.h:810; the two assert sites below are CgsShaderConstants.cpp
// :1022 and :1036, which is what homes the body in this TU.
//
// The SECOND-STAGE fix-up of a streamed External constant block, run by
// CgsResource::ShaderTechniqueResourceType::PostFixUp once for each of the technique's four
// External blocks (vertex object @+28 / vertex global @+44 against the technique's VERTEX
// program, pixel object @+80 / pixel global @+96 against its PIXEL program). Three passes:
//
//  1. BIND. For every constant instance, look its NAME up in the global runtime
//     ShaderConstantTable (linear scan of mapConstantNames over mu8NumUsedConstants) and then
//     ask the program buffer for that name's register handle. On success the block's word-1
//     array -- serialised as "instance data", used by a ShaderTechnique as the CONSTANT-INDEX
//     array the dispatch walker reads (CgsDispatcherCommands AddShaderTechniqueConstantsToDispatchBin
//     / ShaderConstantsExternal_GatherFromContext) -- receives the table slot index, and the
//     handle's register COUNT is overridden with the table entry's declared array size in
//     quadwords whenever that entry is an array (mu8NumEntries > 1). That override is what makes
//     an array constant upload all of its registers: fxc/the Xenon compiler report only the
//     registers the shader actually READS (e.g. ShadowMap_WorldToLight comes back as 11 of its
//     12 registers), and the engine must push the whole declared array.
//     The running total of bound registers is the return value.
//  2. COMPACT. Drop every instance whose name slot is null, moving the survivors (name, index,
//     4-byte handle) down over them, and store the surviving count back into word 0.
//  3. HOIST. Move the instance bound to table slot 6 (InstancingMatrixArray) to position 0, then
//     the instance bound to table slot 0 (world) to position 0 -- the "WORLD or
//     INSTANCING_MATRIX_ARRAY must be the first constant" contract that
//     ShaderTechniqueResourceType::PostFixUp validates right after this runs.
//
// SERIALISED-BLOCK SEAM (see the banner at the top of this TU): the block is console word
// layout, so the walk goes through SerialisedExternal, and the handle array is the measured
// 4-byte renderengine::ProgramVariableHandle stride (FORMAT_MAP.md section 3).
//
// The declared parameter type is the DWARF's `const ProgramBuffer*`; renderengine::ProgramBuffer
// is a static-only class whose instance IS the ProgramBufferData image, which is what the X360
// hands straight to GetVariableHandleByName -- reinterpreted once here, named for clarity.
// =============================================================================================
u32 ShaderConstantsExternal::FixUp(const renderengine::ProgramBuffer* lpVertexProgramBuffer)
{
    SerialisedExternal* const lpBlock = reinterpret_cast<SerialisedExternal*>(this);
    const renderengine::ProgramBufferData* const lpProgramData =
        reinterpret_cast<const renderengine::ProgramBufferData*>(lpVertexProgramBuffer);

    u32* const lpaNames   = SlotArray(lpBlock->muNames);
    u32* const lpaIndices = SlotArray(lpBlock->muConstantsInstanceData);
    renderengine::ProgramVariableHandle* const lpaHandles =
        reinterpret_cast<renderengine::ProgramVariableHandle*>(
            static_cast<uintptr_t>(lpBlock->muProgramStateHandles));

    ShaderConstantTable& lrTable = CgsGraphics::mShaderConstantTable;

    // ---- pass 1: bind each named constant to a table slot + a program register --------------
    u32 luNumRegisters = 0;
    for (u32 luConstant = 0; luConstant < lpBlock->muNumConstantsInstances; ++luConstant)
    {
        bool lbBound = false;

        if (lrTable.mu8NumUsedConstants != 0)
        {
            u32  luIndex   = 0;
            bool lbMatched = false;
            while (luIndex < lrTable.mu8NumUsedConstants)
            {
                const char* const lpcOwnName = SlotString(lpaNames[luConstant]);
                if (lpcOwnName != 0)
                {
                    CGS_ASSERT(luIndex < lrTable.mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");

                    // Inline strcmp, X360 order: walk the TABLE name and stop on its terminator.
                    const char* lpcTableName = lrTable.mapConstantNames[luIndex];
                    const char* lpcOwn       = lpcOwnName;
                    int liDiff;
                    do
                    {
                        liDiff = static_cast<u8>(*lpcTableName) - static_cast<u8>(*lpcOwn);
                        if (*lpcTableName == 0)
                        {
                            break;
                        }
                        ++lpcTableName;
                        ++lpcOwn;
                    }
                    while (liDiff == 0);

                    if (liDiff == 0)
                    {
                        lbMatched = true;
                        break;
                    }
                }
                ++luIndex;
            }

            if (lbMatched)
            {
                CGS_ASSERT(luIndex < lrTable.mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");
                if (renderengine::ProgramBuffer::GetVariableHandleByName(
                        lpProgramData,
                        reinterpret_cast<const u8*>(lrTable.mapConstantNames[luIndex]),
                        &lpaHandles[luConstant]) != 0)
                {
                    u32 luRegisterCount = lpaHandles[luConstant].mu8RegisterCount;

                    const ShaderConstantTableElement& lrElement = lrTable.maConstants[luIndex];
                    if (lrElement.mu8NumEntries > 1u)
                    {
                        luRegisterCount = static_cast<u32>(lrElement.mu8SizeInBytes
                                                           * lrElement.mu8NumEntries) >> 4;
                        lpaHandles[luConstant].mu8RegisterCount = static_cast<u8>(luRegisterCount);
                    }

                    lbBound = true;
                    lpaIndices[luConstant] = luIndex;
                    luNumRegisters += luRegisterCount;
                }
                else if (const char* const lpcSubstitute =
                             PCInstancingSubstituteName(lrTable.mapConstantNames[luIndex]))
                {
                    // [FLAG PC data gap] see the substitute table's banner at the top of this TU.
                    // NOTE what is deliberately NOT done here: the array-size override above.
                    // That override exists because the console's InstancingMatrixArray really is
                    // a 5-entry (20-register) array; the PC program's `world` is a single
                    // 4-register matrix and `g_wheelConstants` a single register, so the count
                    // must stay the one the PROGRAM reports.
                    if (renderengine::ProgramBuffer::GetVariableHandleByName(
                            lpProgramData,
                            reinterpret_cast<const u8*>(lpcSubstitute),
                            &lpaHandles[luConstant]) != 0)
                    {
                        lbBound = true;
                        lpaIndices[luConstant] = luIndex;
                        luNumRegisters += lpaHandles[luConstant].mu8RegisterCount;

                        if (CgsDev::Message::gxMessageFilterFlags & 1)
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "Shader constant " << lrTable.mapConstantNames[luIndex]
                                << " is absent from the recompiled program; bound to its"
                                   " non-instanced twin " << lpcSubstitute << " at register "
                                << static_cast<u32>(lpaHandles[luConstant].mu8RegisterSet)
                                << " x" << static_cast<u32>(lpaHandles[luConstant].mu8RegisterCount)
                                << " [FLAG PC data gap]\n";
                        }
                    }
                }
            }
        }

        if (!lbBound)
        {
            // X360 CgsShaderConstants.cpp:1022 streams "Missing shader constant from table " +
            // name + "\n" into the assert buffer and FIRES. It cannot fire on the console: the
            // shipped microcode was compiled from the same .fx the technique was authored from,
            // so every constant a technique lists exists in its program's variable table.
            //
            // FLAG PC-platform leaf (DATA gap, not an engine deviation): the PC program buffers
            // are RE-COMPILED by tools/assets/shaders/convert_shaders_bundle.py from the TUB
            // HLSL, and two groups of constants are absent from that source:
            //   * InstancingMatrixArray / InstancingIndexArray on the 19 `*_Instanced`
            //     techniques -- the TUB `_Instanced.fx` files are the Remastered port, whose
            //     instancing was reworked; they never mention either constant. Harmless today:
            //     instanced world draws are asserted off upstream (WorldEntityModule
            //     "Instancing not supported") and DispatchAllMeshes traps on them.
            //   * whatever fallback_world.fx does not declare, for the two techniques with no
            //     TUB source at all (CarStudio / Godray).
            // A fired assert here would be a false alarm on every boot, so this is a quiet log.
            // DELETE (restore the console assert) when the PC shader set is compiled from
            // sources that declare the full constant list.
            const char* const lpcName = SlotString(lpaNames[luConstant]);
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint
                    << "Missing shader constant from table "
                    << (lpcName != 0 ? lpcName : "<NULLSTRING>")
                    << " [FLAG PC data gap: constant absent from the recompiled program]\n";
            }
        }
    }

    // ---- pass 2: compact out the null-named instances ---------------------------------------
    u32 luWrite = 0;
    for (u32 luConstant = 0; luConstant < lpBlock->muNumConstantsInstances; ++luConstant)
    {
        if (lpaNames[luConstant] == 0)
        {
            // X360 CgsShaderConstants.cpp:1036 -- same streamed form as above.
            CGS_ASSERT(false, "NULL shader constant name <NULLSTRING>\n");
            continue;
        }

        lpaNames[luWrite]   = lpaNames[luConstant];
        lpaIndices[luWrite] = lpaIndices[luConstant];
        lpaHandles[luWrite] = lpaHandles[luConstant];
        ++luWrite;
    }
    lpBlock->muNumConstantsInstances = luWrite;

    // ---- pass 3: hoist InstancingMatrixArray (slot 6), then world (slot 0), to position 0 ----
    // (Two independent searches, each a no-op when its slot is absent or already first.)
    for (u32 luPass = 0; luPass < 2u; ++luPass)
    {
        const u32 luWantedSlot = (luPass == 0) ? 6u : 0u;

        u32 luFound = 0;
        while (luFound < lpBlock->muNumConstantsInstances && lpaIndices[luFound] != luWantedSlot)
        {
            ++luFound;
        }
        if (luFound == 0 || luFound >= lpBlock->muNumConstantsInstances)
        {
            continue;
        }

        const u32 luName  = lpaNames[0];
        lpaNames[0]       = lpaNames[luFound];
        lpaNames[luFound] = luName;

        const u32 luIdx     = lpaIndices[0];
        lpaIndices[0]       = lpaIndices[luFound];
        lpaIndices[luFound] = luIdx;

        const renderengine::ProgramVariableHandle lHandle = lpaHandles[0];
        lpaHandles[0]       = lpaHandles[luFound];
        lpaHandles[luFound] = lHandle;
    }

    return luNumRegisters;
}

// CgsShaderConstants.cpp:1274
// Linear scan of the name array for an exact match of lpName, returning true if present.
// Faithful to the X360 inline strcmp: walk each candidate name and lpName in lock-step,
// comparing bytes, stopping at lpName's terminating NUL; a fully-matched candidate returns
// true, otherwise advance to the next name until muNumConstantsInstances is exhausted.
bool ShaderConstantsExternal::HasShaderConstant(const char* lpName) const
{
    // SERIALISED-BLOCK SEAM (see the banner): the callers (MaterialTechniqueResourceType::
    // PostFixUp) hand this a block inside a streamed technique blob.
    const SerialisedExternal* const lpBlock = reinterpret_cast<const SerialisedExternal*>(this);
    if (lpBlock->muNumConstantsInstances == 0)
    {
        return false;
    }

    u32 luIndex = 0;
    for (const u32* lppName = SlotArray(lpBlock->muNames); ; ++lppName)
    {
        const char* lpCandidate = SlotString(*lppName);
        const char* lpQuery = lpName;

        int liDiff;
        do
        {
            liDiff = static_cast<u8>(*lpQuery) - static_cast<u8>(*lpCandidate);
            if (*lpQuery == 0)
            {
                break;
            }
            ++lpQuery;
            ++lpCandidate;
        }
        while (liDiff == 0);

        if (liDiff == 0)
        {
            return true;
        }

        if (++luIndex >= lpBlock->muNumConstantsInstances)
        {
            return false;
        }
    }
}

// CgsShaderConstants.cpp:1125
// Relocate the Internal block's internal pointers by the load base. word1 (instance sizes),
// word2 (instance data), word3 (name hashes) and word4 (program handles) each get the base
// added; additionally every entry of the instance-data array (word2) is relocated. Returns
// int 0 on X360; u32 in the header per the DWARF.
u32 ShaderConstantsInternal::FixUp(u8* lpBaseData)
{
    // SERIALISED-BLOCK SEAM (see the banner): walk the console word layout.
    SerialisedInternal* const lpBlock = reinterpret_cast<SerialisedInternal*>(this);
    const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lpBaseData));

    lpBlock->muProgramStateHandles   += luDelta;
    lpBlock->muConstantsInstanceData += luDelta;
    lpBlock->muNamesHash             += luDelta;
    lpBlock->muConstantsInstanceSize += luDelta;

    if (lpBlock->muNumConstantsInstances != 0)
    {
        u32* const lpaInstanceData = SlotArray(lpBlock->muConstantsInstanceData);
        for (u32 luIndex = 0; luIndex < lpBlock->muNumConstantsInstances; ++luIndex)
        {
            lpaInstanceData[luIndex] += luDelta;
        }
    }

    return 0;
}

// CgsShaderConstants.cpp:1366
// Start-of-frame: record the active dispatch bin, reset the dirty count, then mark every
// in-use constant dirty. Faithful to the X360 form, which reads mu8NumDirtyConstants fresh
// as the write index each iteration (it counts up in lock-step with the loop counter), so
// mau8DirtyConstants[i] == i and the final dirty count equals mu8NumUsedConstants.
void ShaderConstantTable::BeginFrame(CgsGraphics::DispatchBin* lpDispatchBin)
{
    const u32 luNumUsed = mu8NumUsedConstants;

    mpDispatchBin = lpDispatchBin;
    mu8NumDirtyConstants = 0;

    if (luNumUsed != 0)
    {
        u32 luIndex = 0;
        do
        {
            mau8DirtyConstants[mu8NumDirtyConstants++] = static_cast<u8>(luIndex++);
        }
        while (luIndex < mu8NumUsedConstants);
    }
}

// CgsShaderConstants.cpp:1498
// Drain the dirty list into a dispatch-bin header block. The header (lpBinHeaderSpace,
// viewed as bytes) holds: byte 0 = dirty count, bytes 1..count = the dirty constant
// indices; a dword pointer array then follows, 4-byte aligned after the index bytes
// ((count + 4) rounded down to a multiple of 4). For each dirty constant we copy its
// index into the header and its latest dispatch-bin copy pointer into the pointer array.
// Finally the dirty list is cleared. Faithful to the X360: the +150 dword index in the
// pseudocode is 600/4 == the start of mapLatestCopyInDispatchBin, recovered here as named
// member access mapLatestCopyInDispatchBin[index].
void ShaderConstantTable::AddDirtyConstantsToDispatchBin(Vector4* lpBinHeaderSpace)
{
    u8* lpau8CountAndIndexArray = reinterpret_cast<u8*>(lpBinHeaderSpace);

    // Pointer array starts after the count byte + index bytes, aligned down to 4 bytes.
    const u16 lu16NumBytesForIndexArray = static_cast<u16>(mu8NumDirtyConstants + 4);
    const rw::math::vpu::Vector4** lpapPointerArray =
        reinterpret_cast<const rw::math::vpu::Vector4**>(lpau8CountAndIndexArray + (lu16NumBytesForIndexArray & 0xFFFC));

    lpau8CountAndIndexArray[0] = mu8NumDirtyConstants;

    if (mu8NumDirtyConstants != 0)
    {
        u32 luIndex = 0;
        do
        {
            const u8 lu8DirtyConstantIndex = mau8DirtyConstants[luIndex];
            lpau8CountAndIndexArray[luIndex + 1] = lu8DirtyConstantIndex;
            ++luIndex;
            *lpapPointerArray++ = mapLatestCopyInDispatchBin[lu8DirtyConstantIndex];
        }
        while (luIndex < mu8NumDirtyConstants);
    }

    mu8NumDirtyConstants = 0;
}

// CgsShaderConstants.cpp:141 (decl) / X360 @ 0x823F4170
// Set the per-entry size in bytes, then recompute the cached array size in quadwords.
// luNumQwInArray = (mu8NumEntries * mu8SizeInBytes) / 16 (1 qw == 16 bytes). The u8*u8
// product promotes to signed int, so the X360 emits a signed /16 (the v4<0 && (v4&0xF)
// round-toward-zero adjust in the pseudocode); plain `/ 16` reproduces it exactly. Assert
// the result fits the u16 cache field, then store it.
void ShaderConstantTableElement::SetSize(u8 lu8SizeInBytes)
{
    mu8SizeInBytes = lu8SizeInBytes;

    const int liNumQwInArray = (mu8NumEntries * mu8SizeInBytes) / 16;
    CGS_ASSERT(liNumQwInArray <= 0xFFFF, "luNumQwInArray <= 0xFFFF");
    mu16SizeOfArrayInQw = static_cast<u16>(liNumQwInArray);
}

// CgsShaderConstants.cpp:171 (decl) / X360 @ 0x823F41E8
// Set the entry count, then recompute the cached array size in quadwords. Identical body to
// SetSize except for which byte field is written first; same signed (mu8NumEntries *
// mu8SizeInBytes) / 16 idiom, same u16 fit assert, same store.
void ShaderConstantTableElement::SetNumEntries(u8 lu8NumEntries)
{
    mu8NumEntries = lu8NumEntries;

    const int liNumQwInArray = (mu8NumEntries * mu8SizeInBytes) / 16;
    CGS_ASSERT(liNumQwInArray <= 0xFFFF, "luNumQwInArray <= 0xFFFF");
    mu16SizeOfArrayInQw = static_cast<u16>(liNumQwInArray);
}

// ---- ShaderConstantsCPU -----------------------------------------------------------------
// The CPU-side ("material animation") shader-constant block. DWARF-homed at
// CgsShaderConstants.h (:1006/1028/1038/1043); the X360 bodies below come from
// CgsMaterialAnimation.cpp (@ 0x827E9800 / 0x827E98D8 / 0x827E9970 / 0x827E99F8).

// CgsShaderConstants.cpp / X360 @ 0x827E9800 (DWARF: CgsShaderConstants.h:1006)
// Relocate the streamed-in CPU shader-constant block by the load base. mpCPUShader (+0)
// is nulled, the instance-data array (+8) and name array (+0xC) each get the base added,
// then per constant instance: the name pointer is relocated only when non-null, while the
// instance-data pointer (u32**) is relocated unconditionally. Asserts the instance count
// fits a byte (< 256). Returns int 0 on X360 (u32 per the DWARF signature).
u32 ShaderConstantsCPU::FixUp(u8* lpBaseData)
{
    // SERIALISED-BLOCK SEAM (see the banner): walk the console word layout.
    SerialisedCPU* const lpBlock = reinterpret_cast<SerialisedCPU*>(this);

    CGS_ASSERT(lpBlock->muNumConstantsInstances < 256, "muNumConstantsInstances<256");

    const u32 luDelta = static_cast<u32>(reinterpret_cast<uintptr_t>(lpBaseData));

    lpBlock->muCPUShader              = 0;
    lpBlock->muConstantsInstanceData += luDelta;
    lpBlock->muNames                 += luDelta;

    if (lpBlock->muNumConstantsInstances != 0)
    {
        u32* const lpaNames        = SlotArray(lpBlock->muNames);
        u32* const lpaInstanceData = SlotArray(lpBlock->muConstantsInstanceData);

        u32 luIndex = 0;
        do
        {
            if (lpaNames[luIndex] != 0)
            {
                lpaNames[luIndex] += luDelta;
            }

            lpaInstanceData[luIndex] += luDelta;

            ++luIndex;
        }
        while (luIndex < lpBlock->muNumConstantsInstances);
    }

    return 0;
}

// CgsShaderConstants.cpp / X360 @ 0x827E98D8 (DWARF: CgsShaderConstants.h:1028)
// Linear name lookup. Walk mppacNames comparing each candidate against lpName with an inline
// strcmp (byte compare, stopping at lpName's terminating NUL). On the first exact match, copy
// that constant's Vector4 (16-byte aligned load/store on X360) out of the per-instance data
// array into lrOutValue and return true; otherwise return false. Mirrors the committed
// ShaderConstantsExternal::HasShaderConstant strcmp loop.
bool ShaderConstantsCPU::GetValue(const char* lpName, Vector4& lrOutValue) const
{
    // SERIALISED-BLOCK SEAM (see the banner): the block lives inside the streamed material.
    const SerialisedCPU* const lpBlock = reinterpret_cast<const SerialisedCPU*>(this);

    const u32 luNum = lpBlock->muNumConstantsInstances;
    if (luNum == 0)
    {
        return false;
    }

    u32 luIndex = 0;
    for (const u32* lppName = SlotArray(lpBlock->muNames); ; ++lppName)
    {
        const char* lpCandidate = SlotString(*lppName);
        const char* lpQuery = lpName;

        int liDiff;
        do
        {
            liDiff = static_cast<u8>(*lpQuery) - static_cast<u8>(*lpCandidate);
            if (*lpQuery == 0)
            {
                break;
            }
            ++lpQuery;
            ++lpCandidate;
        }
        while (liDiff == 0);

        if (liDiff == 0)
        {
            const u32* const lpaInstanceData = SlotArray(lpBlock->muConstantsInstanceData);
            lrOutValue = *reinterpret_cast<const Vector4*>(
                static_cast<uintptr_t>(lpaInstanceData[luIndex]));
            return true;
        }

        if (++luIndex >= luNum)
        {
            return false;
        }
    }
}

// CgsShaderConstants.cpp / X360 @ 0x827E9970 (DWARF: CgsShaderConstants.h:1038)
// Compute the serialised size contribution of this CPU constant block, given the running
// serialised size luCurrentSize (used to 16-align the header relative to the stream pos):
//   * header of (muNumConstantsInstances + 4) words, then 16-byte aligned;
//   * one Vector4 (16 bytes) of instance data per constant;
//   * one word (name pointer) per constant;
//   * each constant's NUL-terminated name, 4-byte aligned.
u32 ShaderConstantsCPU::GetSizeOf(u32 luCurrentSize) const
{
    const u32 luNum = muNumConstantsInstances;

    u32 luAlignedHeader = (((luNum + 4) * 4 + luCurrentSize + 15) & 0xFFFFFFF0u) - luCurrentSize;
    if (luNum != 0)
    {
        luAlignedHeader += 16 * luNum;
    }

    u32 luTotalSize = 4 * luNum + luAlignedHeader;

    if (luNum != 0)
    {
        const char* const* lppName = reinterpret_cast<const char* const*>(mppacNames);
        u32 luRemaining = luNum;
        do
        {
            const char* lpName = *lppName;
            const char* lpWalk = lpName;
            while (*lpWalk++ != 0)
            {
            }

            const u32 luNameLen = static_cast<u32>(lpWalk - lpName - 1);
            luTotalSize += (luNameLen + 4) & 0xFFFFFFFCu;

            --luRemaining;
            ++lppName;
        }
        while (luRemaining != 0);
    }

    return luTotalSize;
}

// CgsShaderConstants.cpp / X360 @ 0x827E99F8 (DWARF: CgsShaderConstants.h:1043)
// A CPU constant block is worth serialising iff it carries a non-zero "AnimDuration" constant:
// fetch that named Vector4 via GetValue; if it is absent, or its first component (the animation
// duration) is exactly 0.0, there is nothing to serialise.
bool ShaderConstantsCPU::ShouldSerialise() const
{
    Vector4 lValue;
    if (!GetValue("AnimDuration", lValue))
    {
        return false;
    }

    if (lValue.x == 0.0f)
    {
        return false;
    }

    return true;
}
