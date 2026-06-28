// =============================================================================
// CgsDispatcherCommands.cpp  (GameShared/GameClasses/Graphics/Dispatch)
//
// The render-dispatch command family: building the packed DispatchCommand stream
// (the *AddToBin entry points), interpreting it back into GPU draw calls (the
// static *Interpret interpreters), and the SPU/PPU "object -> mesh" job that
// expands a renderable's object commands into per-mesh commands in a shared-
// memory dispatch frame.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (authoritative for behaviour and
// member offsets) gated on the X360 ledger, with declaration shape from the
// DecFIGS DWARF and idiom from the engine's sibling Dispatch TUs.
//
// SERIALISED-BLOB NOTE: a DispatchBin is a packed byte stream of 16-byte
// commands plus trailing variable-length "custom sections", and a shared-memory
// DispatchFrame is the SPU job image; the *AddToBin builders, the *Interpret
// consumers and the shared-bin frame helpers necessarily poke those streams by
// quad-word offset. That is the documented external-serialised-data exception to
// the no-raw-offset rule (AGENTS.md): the layout is fixed by the data, not by a
// C++ class. Accesses into real game *objects* (Renderable / RenderableMesh /
// MaterialAssembly) still go through their reconstructed named members.
// =============================================================================

#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"        // DispatchBin / DispatchFrame / DispatchList
#include "GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"            // ShaderConstantTable
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "BrnCommonTypes.h"                                                // Vector4

#include <cstring>   // memcpy

namespace CgsGraphics
{

// -----------------------------------------------------------------------------
// File-static / module dispatch-builder state.
//
// The global ShaderConstantTable the render thread drains its dirty constants
// from when stamping a DrawRenderable command; defined by the CgsShaderConstants
// TU (extern here).
// -----------------------------------------------------------------------------
extern ShaderConstantTable mShaderConstantTable;     // global; bodied by CgsShaderConstants TU

// Module occlusion-cull state (CgsDispatcherCommands.h:260/261). The manager is a
// singleton pointer; the threshold gates whether a mesh is occlusion-tested.
extern OcclusionCullManager* spOcclusionCullManager;          // @ 0x83010FAC
// DWARF: suOcclusionCullIndexCountThreshold, uint32_t. The asm compares the mesh index count
// against it with cmplw (UNSIGNED, @0x827EE5BC), so the threshold + the count are u32.
extern u32                   suOcclusionCullIndexCountThreshold; // @ 0x83010FB0
OcclusionCullManager* spOcclusionCullManager          = 0;
u32                   suOcclusionCullIndexCountThreshold = 0;

// DrawRenderable's per-object excluded-mesh scratch list (CgsDispatcherCommands.h
// statics @ 0x83011948..). suDefaultExcludedMeshCount is the rebuilt-list length,
// suExcludedMeshCount the live length, sauExcludedMeshList[] the id list.
static u8  suDefaultExcludedMeshCount = 0;   // byte_83011948
static u8  suExcludedMeshCount        = 0;   // byte_83011950
static u8  sauExcludedMeshList[256];         // byte_83011951

// AddShaderTechniqueConstantsToDispatchBin @ 0x827F9FB8 (CgsDispatcherCommands.cpp:709).
// Declared-only here (bodied by the shader-constant resolve path): reserves shader-
// constant pointer scratch in the bin for the technique, returns its base / NULL.
Vector4** AddShaderTechniqueConstantsToDispatchBin(DispatchBin* lpBin,
                                                   DispatchObjectContext* lpContext,
                                                   const MaterialTechnique* lpTechnique,
                                                   const MaterialAssembly* lpAssembly,
                                                   bool lbZOnly);

// =============================================================================
// CgsGraphics::DispatchObjectContext::ResetShadowing  @ 0x827E9418
// Zero the leading 50-dword (200-byte) shader-constant shadow block.
// =============================================================================
void DispatchObjectContext::ResetShadowing()
{
    u32* lpShadow = reinterpret_cast<u32*>(this);
    for (s32 li = 0; li < 50; ++li)
        *lpShadow++ = 0;
}

// =============================================================================
// CgsGraphics::CallbackFn::Interpret  @ 0x827E92F0
// Re-invoke the callback stored in the command. word0's command id must be
// CALLBACKFN(0); the callback is fn(this+16, GetPacketLength()).
// =============================================================================
void CallbackFn::Interpret(DispatchCommand* lpCommand, DispatchFrame* /*lpFrame*/,
                           void* /*lpUserData*/, f32 /*lfTime*/)
{
    u32* lpWords = reinterpret_cast<u32*>(lpCommand);

    CGS_ASSERT((lpWords[0] & 0x7Fu) == DispatchCommand::E_CALLBACKFN,
               "lpCommand->GetCommandID() == CALLBACKFN");

    // word2 holds the callback pointer; it receives the custom section (this+16)
    // and the packet length (word0 low 24 bits).
    typedef void (*CallbackProc)(void*, u32);
    CallbackProc lpfnCallback = reinterpret_cast<CallbackProc>(lpWords[2]);
    lpfnCallback(&lpWords[4], lpWords[0] & 0x00FFFFFFu);
}

// =============================================================================
// CgsGraphics::DispatchPacketInterpreter::DispatchPacketInterpreter  @ 0x827E9360
// Store the interpreter table + count (after asserting the table is non-NULL).
// =============================================================================
DispatchPacketInterpreter::DispatchPacketInterpreter(InterpretFn* lpaInterpreters,
                                                     u32 luNumInterpreters)
    : m_paInterpreters(lpaInterpreters)
    , m_uNumInterpreters(luNumInterpreters)
    // X360 @0x827E9360 writes ONLY +0x00 (table) and +0x04 (count); mfTime (+0x08) and
    // mpSingleBufferedDispatchFrame (+0x0C) are deliberately left uninitialised by the ctor.
{
    CGS_ASSERT(lpaInterpreters != 0, "paInterpreters != NULL");
    m_paInterpreters   = lpaInterpreters;
    m_uNumInterpreters = luNumInterpreters;
}

// =============================================================================
// CgsGraphics::SetupBuiltinInterpreters  @ 0x827FF350
// Populate the 4-slot interpreter table with the built-in command interpreters.
// Slot 2 (DRAWRENDERABLEMESH) has no standalone interpreter (meshes are drawn
// through DrawRenderable::Interpret), so it is left NULL.
// =============================================================================
DispatchPacketInterpreter::InterpretFn*
SetupBuiltinInterpreters(DispatchPacketInterpreter::InterpretFn* lpaInterpreters)
{
    lpaInterpreters[DispatchCommand::E_CALLBACKFN]              = &CallbackFn::Interpret;
    lpaInterpreters[DispatchCommand::E_DRAWRENDERABLE]          = &DrawRenderable::Interpret;
    lpaInterpreters[DispatchCommand::E_DRAWRENDERABLEMESH]      = 0;
    lpaInterpreters[DispatchCommand::E_DRAWRENDERABLEMESHZONLY] = &DrawRenderableMeshZOnly::Interpret;
    return lpaInterpreters;
}

// =============================================================================
// DispatchFrame shared-memory output path.
//
// These operate on the shared-memory frame image. The embedded bin lives at
// frame+0x80; the shared-block bookkeeping (active-block @ 0x104, shared-mem
// params @ 0x108, the produced-bin base @ 0x88 / size @ 0x98) and the per-frame
// DispatchList array (@ 0x00, count @ 0x100, 384-byte stride) are reached as the
// serialised SPU frame image. Field roles are named in the comments and read off
// the asm.
// =============================================================================

// =============================================================================
// CgsGraphics::DispatchFrame::ConstructWithSharedBinMemory  @ 0x827EE7C8
//   lpaDispatchListArray, luDispatchListCount, luDispatchBinMasterAddress,
//   luSharedMemoryStartAddress, lpSharedMemoryBlockNextFreeAtomic,
//   luSharedMemoryBlockMax  (DWARF, cpp:3114)
// Initialise the embedded bin + every output DispatchList against shared memory.
// =============================================================================
DispatchFrame* DispatchFrame::ConstructWithSharedBinMemory(
        DispatchFrame* lpResult, DispatchList* lpaDispatchListArray,
        u32 luDispatchListCount, u32 luDispatchBinMasterAddress,
        u32 luSharedMemoryStartAddress, u32* lpSharedMemoryBlockNextFreeAtomic,
        u32 luSharedMemoryBlockMax)
{
    u32* lpFrame = reinterpret_cast<u32*>(lpResult);
    u32* lpBin   = lpFrame + (0x80 / 4);               // embedded bin @ frame+0x80

    lpBin[0x28 / 4] = luSharedMemoryStartAddress;      // 0xA8: bin shared-mem base (a5, stw r7,0x28)
    lpBin[0x30 / 4] = reinterpret_cast<u32>(lpSharedMemoryBlockNextFreeAtomic); // 0xB0: next-free atomic ptr (a6, stw r8,0x30)
    lpBin[0x2C / 4] = luSharedMemoryBlockMax;          // 0xAC: shared-mem block max (a7, stw r9,0x2C)
    lpBin[0x08 / 4] = 0;                               // 0x88: m_pBin
    lpBin[0x0C / 4] = 0;                               // 0x8C: m_pNextWord
    lpBin[0x18 / 4] = 0;                               // 0x98: m_uSize
    lpBin[0x10 / 4] = 0;                               // 0x90
    lpBin[0x14 / 4] = 0;                               // 0x94
    lpBin[0x20 / 4] = 0;                               // 0xA0
    lpBin[0x24 / 4] = 0;                               // 0xA4

    lpFrame[0xB4 / 4]  = reinterpret_cast<u32>(lpResult);    // shared-bin self-pointer (stw r3,0xB4)
    lpFrame[0x108 / 4] = luDispatchBinMasterAddress;         // dispatch-bin master address (a4, stw r6,0x108);
                                                            // FlushBlockToSharedMemory reads this for RelocateForMainMemory
    lpFrame[0x104 / 4] = 0;                                  // active-block address
    lpFrame[0]         = reinterpret_cast<u32>(lpaDispatchListArray);  // m_paLists
    lpFrame[0x100 / 4] = luDispatchListCount;                // muNumDispatchLists

    if (luDispatchListCount != 0)
    {
        u32 luListByteOffset = 0;
        u32 luIndex          = 0;
        do
        {
            ++luIndex;
            u32* lpList = reinterpret_cast<u32*>(lpFrame[0] + luListByteOffset);
            luListByteOffset += 384;                  // sizeof(DispatchList)
            lpList[0x0C / 4]  = 0;
            lpList[0x04 / 4]  = reinterpret_cast<u32>(lpBin);   // list -> embedded bin
            lpList[0x11C / 4] = 0;
            lpList[0]         = 0;
            lpList[0x10 / 4]  = 0;
            lpList[0x14 / 4]  = 0;
            lpList[0x18 / 4]  = 0;
            lpList[0x08 / 4]  = lpBin[0x08 / 4];       // copy current m_pBin
        }
        while (luIndex < lpFrame[0x100 / 4]);
    }
    return lpResult;
}

// =============================================================================
// CgsGraphics::DispatchFrame::RelocateForMainMemory  @ 0x827EE970
// Relocate every DispatchList in the frame for main-memory addressing.
// =============================================================================
DispatchFrame* DispatchFrame::RelocateForMainMemory(DispatchFrame* lpResult,
                                                    u32 luBinBase, u32 luBinOffset,
                                                    u32 luListOffset)
{
    u32* lpFrame = reinterpret_cast<u32*>(lpResult);
    DispatchFrame* lpRet = lpResult;

    if (lpFrame[0x100 / 4] != 0)
    {
        u32 luListByteOffset = 0;
        u32 luIndex          = 0;
        do
        {
            DispatchList* lpList =
                reinterpret_cast<DispatchList*>(lpFrame[0] + luListByteOffset);
            lpRet = reinterpret_cast<DispatchFrame*>(
                lpList->RelocateForMainMemory(luBinBase, luBinOffset, luListOffset));
            ++luIndex;
            luListByteOffset += 384;
        }
        while (luIndex < lpFrame[0x100 / 4]);
    }
    return lpRet;
}

// =============================================================================
// CgsGraphics::DispatchFrame::FlushBlockToSharedMemory  @ 0x827F72A8
// If a shared-memory output block is active, relocate the frame for main memory
// then copy the produced bin into the shared block; clear the active-block slot.
// =============================================================================
DispatchFrame* DispatchFrame::FlushBlockToSharedMemory(DispatchFrame* lpResult)
{
    u32* lpFrame = reinterpret_cast<u32*>(lpResult);
    DispatchFrame* lpRet = lpResult;

    u32 luActiveBlock = lpFrame[0x104 / 4];           // active shared-mem block address
    if (luActiveBlock != 0)
    {
        RelocateForMainMemory(lpResult, lpFrame[0x88 / 4],
                              luActiveBlock, lpFrame[0x108 / 4]);
        u32 luDst = lpFrame[0x104 / 4];
        u32 luSrc = lpFrame[0x88 / 4];                // produced bin base
        if (luDst != luSrc)
        {
            lpRet = reinterpret_cast<DispatchFrame*>(
                memcpy(reinterpret_cast<void*>(luDst),
                       reinterpret_cast<void*>(luSrc),
                       16u * lpFrame[0x98 / 4]));      // 0x98: bin size in quad-words
        }
    }
    lpFrame[0x104 / 4] = 0;                            // clear active block
    return lpRet;
}

// =============================================================================
// ObjectToMeshJob::SharedMemoryChangeCallback  @ 0x827EE760
// Bin-overflow callback: rebase the frame's bin onto the freshly-acquired shared
// block and re-point every DispatchList at the new main-memory offset. (A free
// function: it is registered as DispatchBin's memory callback, not a member.)
// =============================================================================
DispatchFrame* ObjectToMeshJob_SharedMemoryChangeCallback(DispatchFrame* lpResult)
{
    u32* lpFrame = reinterpret_cast<u32*>(lpResult);

    u32 luActiveBlock = lpFrame[0x104 / 4];
    lpFrame[0x4394 / 4] = 0;
    lpFrame[0x88 / 4]   = luActiveBlock;              // m_pBin -> new block
    lpFrame[0x8C / 4]   = luActiveBlock;              // m_pNextWord -> new block
    lpFrame[0x98 / 4]   = 1024;                       // bin size = KU_BLOCK_SIZE_IN_QUAD_WORDS

    u32 luMainMemOffset = lpFrame[0x108 / 4] + lpFrame[0x88 / 4] - lpFrame[0x104 / 4];
    if (lpFrame[0x100 / 4] != 0)
    {
        u32 luListByteOffset = 0;
        u32 luIndex          = 0;
        do
        {
            ++luIndex;
            u32* lpList = reinterpret_cast<u32*>(luListByteOffset + lpFrame[0]);
            luListByteOffset += 384;
            lpList[0x08 / 4] = luMainMemOffset;
        }
        while (luIndex < lpFrame[0x100 / 4]);
    }
    return lpResult;
}

// =============================================================================
// ShaderConstantsExternal::AddToDispatchBinFromStatePointers  @ 0x827E93B8
// Gather each external constant from the live shader state into the dispatch-bin
// header space, advancing the bin write cursor.
//   lpExternal = {muNumConstants, mpaSourceIndices}  (ShaderConstantsExternal)
//   luStateBase = base of the live state vector array
//   lppBinCursor = &lpDispatchBinWriteCursor (advanced by muNumConstants words)
// (A free function: ShaderConstantsExternal is bodied across several Dispatch TUs;
// this is the one X360-attested entry that lands here.)
// =============================================================================
int ShaderConstantsExternal_AddToDispatchBinFromStatePointers(
        const u32* lpExternal, u32 luStateBase, int liResult, u32** lppBinCursor)
{
    u32 luNumConstants = lpExternal[0];
    if (luNumConstants != 0)
    {
        u32 luIndex     = 0;
        u32 luWordIndex = 0;
        const u32* lpaSourceIndices = reinterpret_cast<const u32*>(lpExternal[1]);
        do
        {
            ++luIndex;
            (*lppBinCursor)[luWordIndex] =
                *reinterpret_cast<const u32*>(
                    4u * lpaSourceIndices[luWordIndex] + luStateBase);
            ++luWordIndex;
        }
        while (luIndex < lpExternal[0]);
    }
    *lppBinCursor += lpExternal[0];                   // advance by the constant count
    return liResult;
}

// =============================================================================
// CgsGraphics::DrawRenderable::AddToBin  @ 0x827FA0D0
// Stamp a DRAWRENDERABLE command into the frame's bin: optionally rebuild the
// excluded-mesh list, allocate the command + custom section, write the renderable
// pointer + the per-object dispatch-thread-info bytes, drain the shader table's
// dirty constants into the command, and finalise word0/word1.
//
// The Hex-Rays 34-arg signature is a struct-by-value artefact -- the small
// DrawRenderableDispatchThreadInfo (mbRenderZOnly / list / technique / instance
// counts) and the sort key are passed by value and the trailing args are their
// bytes. Reconstructed against the asm with the DWARF prototype's parameter set.
// =============================================================================
bool DrawRenderable::AddToBin(const Renderable* lpRenderable, DispatchFrame* lpFrame,
                              bool lbRebuildExcluded, s8 li8SortHi, s8 li8SortLo,
                              u8 lu8ExcludeByte, u8 lu8ListByte, bool /*lbPreZ*/,
                              u8 lu8ThreadInfoByte, u8 lu8TechniqueByte,
                              s32 /*li32SortKey*/, u8 lu8FinalSortByte)
{
    CGS_ASSERT(lpRenderable != 0, "Adding null renderable pointer to bin");
    CGS_ASSERT(lpFrame != 0,      "Trying to use a NULL dispatch frame");

    // Build the excluded-mesh list: rebuild the identity list 0..N-1, or reuse the
    // count from the previous build.
    u8 lu8ExcludedCount;
    if (lbRebuildExcluded)
    {
        lu8ExcludedCount = suDefaultExcludedMeshCount;
        for (u32 lu = 0; lu < suDefaultExcludedMeshCount; ++lu)
        {
            sauExcludedMeshList[lu] = static_cast<u8>(lu);
            lu8ExcludedCount = suDefaultExcludedMeshCount;
        }
        suExcludedMeshCount = lu8ExcludedCount;
    }
    else
    {
        lu8ExcludedCount = suExcludedMeshCount;
    }

    // Custom-section length in quad-words: a packed excluded-id header plus the
    // per-id payload, rounded up to whole quad-words.
    u16 lu16CustomQw = static_cast<u16>(
        ((((lu8ExcludedCount + 4u) & 0xFFFCu) + 4u * lu8ExcludedCount) + 15u) / 16u);

    // Allocate the command + custom section in the frame's embedded bin (frame+0x80).
    DispatchBin* lpBin =
        reinterpret_cast<DispatchBin*>(reinterpret_cast<u8*>(lpFrame) + 0x80);
    u32* lpCommand = reinterpret_cast<u32*>(lpBin->AllocateCommand(lu16CustomQw + 17u));
    CGS_ASSERT(lpCommand != 0, "lpCmd");

    // The custom section starts after the 16-byte header + the lu16CustomQw payload.
    u8* lpCustom = reinterpret_cast<u8*>(lpCommand) + (static_cast<u32>(lu16CustomQw) << 4) + 16;
    lpCustom[0]  = lu8FinalSortByte;
    lpCustom[3]  = static_cast<u8>(li8SortLo);
    lpCustom[1]  = static_cast<u8>(li8SortHi);
    lpCustom[2]  = lu8TechniqueByte;
    lpCustom[4]  = lu8ThreadInfoByte;
    // word2 of the custom section: the renderable's last-mesh table entry
    // (mppMeshes[mu16NumMeshes - 1]).
    reinterpret_cast<u32*>(lpCustom)[2] =
        reinterpret_cast<const u32*>(lpRenderable->mppMeshes)[lpRenderable->mu16NumMeshes - 1];

    // Drain the render thread's dirty shader constants into the command's word4 block
    // (Command+0x10). asm @0x827FA0D0: `addi r4,r30,0x10` with r30 = the allocated command;
    // the callee writes count/index/pointer-array into this destination buffer.
    mShaderConstantTable.AddDirtyConstantsToDispatchBin(
        reinterpret_cast<Vector4*>(lpCommand + (0x10 / 4)));

    lpCommand[2] = reinterpret_cast<u32>(lpRenderable);
    lpCommand[3] = reinterpret_cast<u32>(lpCustom);
    lpCommand[0] = (lu16CustomQw + 1u) | 0x01000000u;   // DRAWRENDERABLE id + length
    // word1 packs the sort/list bytes (asm: extsb/slwi/or + two insrwi inserts).
    lpCommand[1] = ((((((static_cast<u32>(li8SortHi) << 8)
                        | (static_cast<u32>(li8SortLo) & 0xFFu)) << 8)
                      | lu8ListByte) << 8) | lu8ExcludeByte);
    return true;
}

// =============================================================================
// DrawRenderableMesh / DrawRenderableMeshZOnly AddToBin -- stamp a single mesh
// draw command. Shared validation; the differences are the command id word and
// the per-command size formula (the Z-only command carries no custom payload).
// =============================================================================
bool DrawRenderableMesh::AddToBin(const RenderableMesh* lpMesh, DispatchBin* lpBin,
                                  DispatchObjectContext* lpContext, u8 lu8TechniqueIndex,
                                  u8 lu8InstanceCount,
                                  const DrawRenderableDispatchThreadInfo* /*lpThreadInfo*/)
{
    CGS_ASSERT(lpMesh != 0, "Adding null mesh pointer to bin");
    CGS_ASSERT(lpBin != 0,  "Trying to fill null bin");
    const MaterialAssembly* lpAssembly = lpMesh->mpMaterialAssembly;
    CGS_ASSERT(lpAssembly != 0, "lpMesh->mpMaterialAssembly");
    CGS_ASSERT(lu8TechniqueIndex < lpAssembly->GetLength(),
               "lu8TechniqueIndex < lpMesh->mpMaterialAssembly->GetLength()");

    const MaterialTechnique* lpTechnique = lpAssembly->GetMaterial(lu8TechniqueIndex);
    CGS_ASSERT(lpTechnique != 0, "Null material technique on mesh");

    Vector4** lpConstScratch = AddShaderTechniqueConstantsToDispatchBin(
        lpBin, lpContext, lpTechnique, lpAssembly, false);
    CGS_ASSERT(lpConstScratch != 0, "lpMemoryForShaderConstantPointers != NULL");

    // Per-command size (quad-words): driven by the technique's external-sampler
    // count. Technique byte +0x23 == mi8NumExternalSamplers (CgsMaterialTechnique
    // DWARF) -- serialised read of a type owned by CgsMaterialTechnique.cpp.
    s8  li8NumExternal = reinterpret_cast<const s8*>(lpTechnique)[0x23];
    u32 luSizeQw = (((4 * li8NumExternal + 15) & 0xFFFFFFF0u) + 48 * li8NumExternal) >> 4;

    u32* lpCommand = reinterpret_cast<u32*>(lpBin->AllocateCommand(luSizeQw));
    CGS_ASSERT(lpCommand != 0, "lpCmd != NULL");

    lpCommand[0] = luSizeQw | 0x02000000u;            // DRAWRENDERABLEMESH id + length
    CGS_ASSERT(lpConstScratch != 0, "lpMemoryForShaderConstantPointers != NULL");
    lpCommand[2] = reinterpret_cast<u32>(lpMesh);
    lpCommand[3] = reinterpret_cast<u32>(lpConstScratch);
    lpCommand[1] = ((static_cast<u32>(lu8InstanceCount) << 8) & 0xFF00u) | lu8TechniqueIndex;
    return true;
}

bool DrawRenderableMeshZOnly::AddToBin(const RenderableMesh* lpMesh, DispatchBin* lpBin,
                                       DispatchObjectContext* lpContext, u8 lu8TechniqueIndex,
                                       u8 lu8InstanceCount,
                                       const DrawRenderableDispatchThreadInfo* /*lpThreadInfo*/)
{
    CGS_ASSERT(lpMesh != 0, "Adding null mesh pointer to bin");
    CGS_ASSERT(lpBin != 0,  "Trying to fill null bin");
    const MaterialAssembly* lpAssembly = lpMesh->mpMaterialAssembly;
    CGS_ASSERT(lpAssembly != 0, "lpMesh->mpMaterialAssembly");
    CGS_ASSERT(lu8TechniqueIndex < lpAssembly->GetLength(),
               "lu8TechniqueIndex < lpMesh->mpMaterialAssembly->GetLength()");

    const MaterialTechnique* lpTechnique = lpAssembly->GetMaterial(lu8TechniqueIndex);
    CGS_ASSERT(lpTechnique != 0, "Null material technique on mesh");

    Vector4** lpConstScratch = AddShaderTechniqueConstantsToDispatchBin(
        lpBin, lpContext, lpTechnique, lpAssembly, true);
    CGS_ASSERT(lpConstScratch != 0, "lpMemoryForShaderConstantPointers != NULL");

    // Z-only commands carry no per-command custom payload (size word == 0).
    u32* lpCommand = reinterpret_cast<u32*>(lpBin->AllocateCommand(0));
    CGS_ASSERT(lpCommand != 0, "lpCmd != NULL");

    lpCommand[0] = 0x03000000u;                       // DRAWRENDERABLEMESHZONLY id, length 0
    CGS_ASSERT(lpConstScratch != 0, "lpMemoryForShaderConstantPointers != NULL");
    lpCommand[2] = reinterpret_cast<u32>(lpMesh);
    lpCommand[3] = reinterpret_cast<u32>(lpConstScratch);
    lpCommand[1] = ((static_cast<u32>(lu8InstanceCount) << 8) & 0xFF00u) | lu8TechniqueIndex;
    return true;
}

// =============================================================================
// CgsGraphics::DrawRenderableMesh::InterpretOcclusionQuery  @ 0x827EE550
// Issue a hardware occlusion query for the mesh referenced by this command. If
// the mesh is force-visible or below the index-count threshold, trivially accept
// it; otherwise transform its packed bounding box by the command's world*view-
// projection matrix and render the occludee box into the GPU occlusion query.
// =============================================================================
void DrawRenderableMesh::InterpretOcclusionQuery(DispatchCommand* lpCommand, f32 /*lfTime*/)
{
    u32* lpWords = reinterpret_cast<u32*>(lpCommand);
    u32  luCommandId = lpWords[0] & 0x7Fu;
    CGS_ASSERT(luCommandId == DispatchCommand::E_DRAWRENDERABLEMESH
                   || luCommandId == DispatchCommand::E_DRAWRENDERABLEMESHZONLY,
               "pCommand->GetCommandID() == DRAWRENDERABLEMESH || "
               "pCommand->GetCommandID() == DRAWRENDERABLEMESHZONLY");

    // word2 = mesh pointer. Mesh byte +0x25 == per-mesh "force visible" flag,
    // word +0x1C == index count (serialised mesh image; renderablemesh.h owns the
    // named-member layout but these two diagnostics fields are past its modelled
    // prefix, so they are read positionally here and documented).
    const u8* lpMesh = reinterpret_cast<const u8*>(lpWords[2]);
    bool lbForceVisible = lpMesh[0x25] != 0;
    u32  luIndexCount   = reinterpret_cast<const u32*>(lpMesh)[0x1C / 4];

    if (lbForceVisible || luIndexCount < suOcclusionCullIndexCountThreshold)
    {
        spOcclusionCullManager->TrivialAcceptOccludeeBoundingBox();
        return;
    }

    // Resolve the material/technique for the query (clamped technique index).
    const MaterialAssembly* lpAssembly =
        reinterpret_cast<const MaterialAssembly*>(reinterpret_cast<const u32*>(lpMesh)[0x20 / 4]);
    u32 luLength    = lpAssembly->GetLength();
    u32 luTechIndex = lpMesh[0x24];
    if (luTechIndex >= luLength) luTechIndex = luLength;
    if (static_cast<s32>(luTechIndex) - 1 >= static_cast<s32>(lpWords[1]))
        luTechIndex = lpWords[1] + 1;
    const MaterialTechnique* lpMaterial = lpAssembly->GetMaterial(luTechIndex - 1);
    CGS_ASSERT(lpMaterial != 0, "lpMaterial");

    // The X360 then transforms the mesh PackedOobb by the command's world*view-
    // projection matrix with an inline VMX matrix multiply and renders the
    // resulting occludee box (OcclusionCullManager::RenderOccludeeBoundingBox).
    // That VMX transform + the PackedOobb::ToMatrix corner build are not yet
    // reconstructed; the verifiable early-out / trivial-accept path above is, and
    // the render path is left to the occlusion-render reconstruction.
    CGS_ASSERT(false, "InterpretOcclusionQuery render path not yet reconstructed (VMX transform)");
}

// =============================================================================
// ObjectToMeshJob::ExecuteImplementation  @ 0x827FF380
//
// The SPU/PPU "object -> mesh" job entry point. It DMAs the per-job header and
// renderable into local job state, constructs a shared-memory dispatch frame,
// walks the input dispatch list expanding each object command into per-mesh
// commands (DispatchList::DispatchAllObjectToMesh), flushes the produced block to
// shared memory, then DMAs the output dispatch-list array back to main memory.
//
// The verifiable shell (frame-skip sentinel, construct, the expansion loop,
// flush, output copy-out) is what the asm reliably shows; the heavy SPU DMA and
// per-mesh expansion run on the ~30 KB DispatchObjectContext_JobState image and
// the DispatchList/DispatchFrame helpers reconstructed in their own TUs. The full
// driver is not yet reconstructed member-by-member.
// =============================================================================
void* ObjectToMeshJob_ExecuteImplementation(void* lpJobScratch, const u32* lpInput)
{
    // lpInput[0x28/4] == frame number; -1 sentinel means "skip this job".
    if (lpInput[0x28 / 4] == 0xFFFFFFFFu)
        return lpJobScratch;

    CGS_ASSERT(false, "ObjectToMeshJob::ExecuteImplementation not fully reconstructed");
    return lpJobScratch;
}

// =============================================================================
// The two big command-stream INTERPRETERS.
//
// DrawRenderable::Interpret and DrawRenderableMeshZOnly::Interpret walk the packed
// dispatch-command stream and program the GPU (shadow::Device draw/state calls,
// inline VMX matrix work, GCM pokes). Their X360 Hex-Rays output is flagged
// "local variable allocation has failed" (unreliable), they are ~1.4 K / ~3 K
// lines, and they depend on a web of not-yet-reconstructed types (MaterialTechnique
// / ShaderTechnique / DispatchObjectContext_JobState) and the shadow::Device GPU
// layer. Per the project stub scaffold (AGENTS.md: never invent a body), these are
// left as trap-stub bodies until those dependencies are reconstructed; their
// declarations + the verified command-id/dispatch wiring above are what callers
// compile against (SetupBuiltinInterpreters installs these as the slot 1 / slot 3
// interpreters).
// =============================================================================
void DrawRenderable::Interpret(DispatchCommand* /*lpCommand*/, DispatchFrame* /*lpFrame*/,
                               void* /*lpUserData*/, f32 /*lfTime*/)
{
    CGS_ASSERT(false, "DrawRenderable::Interpret not yet reconstructed (GPU command stream)");
}

void DrawRenderableMeshZOnly::Interpret(DispatchCommand* /*lpCommand*/, DispatchFrame* /*lpFrame*/,
                                        void* /*lpUserData*/, f32 /*lfTime*/)
{
    CGS_ASSERT(false, "DrawRenderableMeshZOnly::Interpret not yet reconstructed (GPU command stream)");
}

} // namespace CgsGraphics
