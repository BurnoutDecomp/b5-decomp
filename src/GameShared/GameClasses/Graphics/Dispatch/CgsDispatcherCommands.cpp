// =============================================================================
// CgsDispatcherCommands.cpp  (GameShared/GameClasses/Graphics/Dispatch)
//
// The render-dispatch command family: building the packed DispatchCommand stream
// (the *AddToBin entry points), interpreting it back into GPU draw calls (the
// static *Interpret interpreters + the DispatchList::DispatchAll* walkers), and
// the SPU/PPU "object -> mesh" shared-memory helpers.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (authoritative for behaviour and
// member offsets) gated on the X360 ledger, with declaration shape from the
// DecFIGS DWARF. The full recon notes (per-store asm decode of AddToBin word1 /
// the thread-info trailer, the sort-record decode `packet = m_pBinBase +
// (record & 0xFFFFF) * 16` settled from the DispatchAllMeshesZOnly asm, the
// pass/list map) live in the renderer wave log.
//
// SERIALISED-BLOB NOTE: a DispatchBin is a packed byte stream of 16-byte
// commands plus trailing variable-length sections, and a shared-memory
// DispatchFrame is the SPU job image; the *AddToBin builders, the *Interpret
// consumers and the shared-bin frame helpers necessarily poke those streams by
// quad-word offset. That is the documented external-serialised-data exception to
// the no-raw-offset rule (AGENTS.md). Accesses into real game *objects*
// (Renderable / RenderableMesh / MaterialAssembly) go through named members.
//
// ---------------------------------------------------------------------------
// [x64 command image] The command stream is built AND consumed on the PC (it
// never round-trips through serialised data), so on the LLP64 gate the stream
// carries host pointers, packed as follows (each a documented deviation from
// the 32-bit X360 image; the stream stays self-consistent):
//   * DRAWRENDERABLE       word2..3 = Renderable* (u64). The trailer pointer is
//     derived (cmd + 16 + dirtyQw*16), not stored. The trailer's last-mesh
//     pointer is a u64 at trailer+8.
//   * DRAWRENDERABLEMESH / ...ZONLY  word2..3 = RenderableMesh* (u64); the
//     shader-constant scratch pointer moves into the FIRST payload qword
//     (X360 kept it in word3); the SECOND..FIFTH payload qwords carry the
//     object's world-view-projection matrix -- a PC BRING-UP SHIM (loudly
//     flagged below) so the fallback-shader path can transform geometry before
//     the shader-constant machinery is fully online. The external-sampler
//     payload follows.
//   * The dirty-shader-constant block after a DRAWRENDERABLE header uses the
//     committed x64 AddDirtyConstantsToDispatchBin layout (8-byte pointers), so
//     the fixed worst-case reservation grows from the X360's 16 qwords to 29.
// ---------------------------------------------------------------------------
// =============================================================================

#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"        // DispatchBin / DispatchFrame / DispatchList
#include "GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"      // shadow::Device (the GPU flush leaf)
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"            // ShaderConstantTable
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // the one-shot bring-up gates
#include "BrnCommonTypes.h"                                                // Vector4 / Matrix44

#include <cstring>   // memcpy

namespace CgsGraphics
{

// The clip-space OOBB frustum test (CgsDrawRenderableFrustumTest.cpp @0x827EE478).
unsigned int FrustumTest(const rw::math::vpu::Matrix44& lrClipRows);

// -----------------------------------------------------------------------------
// Module dispatch state.
//
// The global ShaderConstantTable the producers drain their dirty constants from
// when stamping a DRAWRENDERABLE command; defined by the CgsShaderConstants TU
// (extern here). RECONCILE: the statics this TU previously carried
// (suDefaultExcludedMeshCount / suExcludedMeshCount / sauExcludedMeshList,
// byte_83011948/50/51) are NOT a separate "excluded mesh" list -- they are the
// table's own mu8NumUsedConstants (+1400) / mu8NumDirtyConstants (+1408) /
// mau8DirtyConstants (+1409) members (the addresses sit inside the table image
// @0x830113D0), so AddToBin now reaches them through the table by name.
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

// The per-walk technique/material cache (X360 dword_83010FA4 / dword_83010FA8):
// DispatchAll* reset these to -1, and the mesh walk re-binds programs/states/
// constants only when the technique actually changes.
static const MaterialTechniqueView* spLastTechnique = reinterpret_cast<const MaterialTechniqueView*>(~uintptr_t(0));
static uintptr_t                    suLastTechniqueAux = ~uintptr_t(0);   // dword_83010FA8 (companion cache word)

namespace
{
    // ---- x64 command-image helpers (see the header note) --------------------
    inline void WriteCommandPointer(u32* lpWords, const void* lpPointer)
    {
        const u64 lu64 = static_cast<u64>(reinterpret_cast<uintptr_t>(lpPointer));
        std::memcpy(lpWords, &lu64, sizeof(lu64));
    }
    inline void* ReadCommandPointer(const u32* lpWords)
    {
        u64 lu64 = 0;
        std::memcpy(&lu64, lpWords, sizeof(lu64));
        return reinterpret_cast<void*>(static_cast<uintptr_t>(lu64));
    }

    // Truncating pointer->u32 for the (PC-dead) SPU shared-memory image helpers;
    // X360 stored real 32-bit guest pointers here.
    inline u32 PtrToGuestU32(const void* lpPointer)
    {
        return static_cast<u32>(reinterpret_cast<uintptr_t>(lpPointer));
    }

    // The command id lives in word0's HIGH byte, not its low bits:
    // DispatchCommand::GetCommandID() == (word0 >> KU_SIZE_BITS(24)) & 0x7F, and every
    // AddToBin stamps word0 = packetLength | (id << 24) (e.g. DrawRenderable's
    // `| 0x01000000`). The six id checks in this TU had been reading `word0 & 0x7F`,
    // which is the low byte of the LENGTH -- so DispatchAllObjectToMesh asserted
    // "pCommand->GetCommandID() == DRAWRENDERABLE" on the first real world packet and
    // InterpretOcclusionQuery would have mis-routed zonly/mesh commands.
    inline u32 CommandIdOf(u32 luWord0)
    {
        return (luWord0 >> DispatchCommand::KU_SIZE_BITS) & 0x7Fu;
    }

    // Sorted/unsorted record decode (asm @0x827F7700): packet = binBase + offset*16.
    inline u32* PacketFromRecord(DispatchCommand* lpBinBase, u64 luRecord)
    {
        return reinterpret_cast<u32*>(
            reinterpret_cast<u8*>(lpBinBase) + (static_cast<u32>(luRecord) & 0xFFFFFu) * 16u);
    }

    // The dirty-constant block size in qwords for `count` constants under the
    // committed host layout ({count u8, idx bytes, aligned pointer array}).
    inline u32 DirtyBlockQwords(u32 luCount)
    {
        const u32 luPtrBytes = static_cast<u32>(sizeof(void*));
        return ((((luCount + 4u) & ~3u) + luPtrBytes * luCount) + 15u) / 16u;
    }

    // Worst-case dirty block (all 50 table constants) -- the X360 reserves this
    // on every DRAWRENDERABLE allocation (16 qwords there; 29 on the host).
    inline u32 DirtyBlockQwordsMax() { return DirtyBlockQwords(50u); }

    // PC mesh-command payload layout (qwords after the 16-byte header):
    //   [0]     scratch-pointer qword (u64 shader-constant scratch, 8B spare)
    //   [1..4]  the object WVP matrix  [PC bring-up shim -- see header note]
    //   [5..]   external-sampler payload (X360 layout, currently unconsumed)
    const u32 KU_MESH_PAYLOAD_FIXED_QW = 5u;
}

// AddShaderTechniqueConstantsToDispatchBin @ 0x827F9FB8 (CgsDispatcherCommands.cpp:709).
// Reserve bin scratch for the technique's shader-constant pointers and gather them
// from the object context's constant shadow. Bodied below.
Vector4** AddShaderTechniqueConstantsToDispatchBin(DispatchBin* lpBin,
                                                   DispatchObjectContext* lpContext,
                                                   const MaterialTechnique* lpTechnique,
                                                   const MaterialAssembly* lpAssembly,
                                                   bool lbZOnly);

// =============================================================================
// CgsGraphics::DispatchObjectContext::ResetShadowing  @ 0x827E9418
// Zero the 50-slot shader-constant shadow block (X360: 50 dwords; the x64 gate
// clears the same 50 named pointer slots).
// =============================================================================
void DispatchObjectContext::ResetShadowing()
{
    for (s32 li = 0; li < 50; ++li)
        mapConstantData[li] = 0;
}

// =============================================================================
// CgsGraphics::CallbackFn::Interpret  @ 0x827E92F0
// Re-invoke the callback stored in the command. word0's command id must be
// CALLBACKFN(0); the callback is fn(custom section, GetPacketLength()).
// [x64] the callback pointer spans words 2..3 of the command.
// =============================================================================
void CallbackFn::Interpret(DispatchCommand* lpCommand, DispatchFrame* /*lpFrame*/,
                           void* /*lpUserData*/, f32 /*lfTime*/)
{
    u32* lpWords = reinterpret_cast<u32*>(lpCommand);

    CGS_ASSERT(CommandIdOf(lpWords[0]) == DispatchCommand::E_CALLBACKFN,
               "lpCommand->GetCommandID() == CALLBACKFN");

    typedef void (*CallbackProc)(void*, u32);
    CallbackProc lpfnCallback = reinterpret_cast<CallbackProc>(ReadCommandPointer(&lpWords[2]));
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
// through the DispatchAllMeshes walk), so it is left NULL.
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
// DispatchFrame shared-memory output path (SPU job image; DEAD ON PC -- the PC
// takes the single-threaded object-to-mesh path). Kept for the record layout;
// the pointer stores truncate to the 32-bit guest image exactly as the X360
// stored 4-byte guest pointers.
// =============================================================================

// =============================================================================
// CgsGraphics::DispatchFrame::ConstructWithSharedBinMemory  @ 0x827EE7C8
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
    lpBin[0x30 / 4] = PtrToGuestU32(lpSharedMemoryBlockNextFreeAtomic); // 0xB0: next-free atomic ptr (a6, stw r8,0x30)
    lpBin[0x2C / 4] = luSharedMemoryBlockMax;          // 0xAC: shared-mem block max (a7, stw r9,0x2C)
    lpBin[0x08 / 4] = 0;                               // 0x88: m_pBin
    lpBin[0x0C / 4] = 0;                               // 0x8C: m_pNextWord
    lpBin[0x18 / 4] = 0;                               // 0x98: m_uSize
    lpBin[0x10 / 4] = 0;                               // 0x90
    lpBin[0x14 / 4] = 0;                               // 0x94
    lpBin[0x20 / 4] = 0;                               // 0xA0
    lpBin[0x24 / 4] = 0;                               // 0xA4

    lpFrame[0xB4 / 4]  = PtrToGuestU32(lpResult);            // shared-bin self-pointer (stw r3,0xB4)
    lpFrame[0x108 / 4] = luDispatchBinMasterAddress;         // dispatch-bin master address (a4, stw r6,0x108);
                                                            // FlushBlockToSharedMemory reads this for RelocateForMainMemory
    lpFrame[0x104 / 4] = 0;                                  // active-block address
    lpFrame[0]         = PtrToGuestU32(lpaDispatchListArray);  // m_paLists
    lpFrame[0x100 / 4] = luDispatchListCount;                // muNumDispatchLists

    if (luDispatchListCount != 0)
    {
        u32 luListByteOffset = 0;
        u32 luIndex          = 0;
        do
        {
            ++luIndex;
            u32* lpList = reinterpret_cast<u32*>(static_cast<uintptr_t>(lpFrame[0] + luListByteOffset));
            luListByteOffset += 384;                  // sizeof(DispatchList) (32-bit guest image)
            lpList[0x0C / 4]  = 0;
            lpList[0x04 / 4]  = PtrToGuestU32(lpBin);   // list -> embedded bin
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
                reinterpret_cast<DispatchList*>(static_cast<uintptr_t>(lpFrame[0] + luListByteOffset));
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
                memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(luDst)),
                       reinterpret_cast<void*>(static_cast<uintptr_t>(luSrc)),
                       16u * lpFrame[0x98 / 4]));      // 0x98: bin size in quad-words
        }
    }
    lpFrame[0x104 / 4] = 0;                            // clear active block
    return lpRet;
}

// =============================================================================
// ObjectToMeshJob::SharedMemoryChangeCallback  @ 0x827EE760  (SPU path; PC-dead)
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
            u32* lpList = reinterpret_cast<u32*>(static_cast<uintptr_t>(luListByteOffset + lpFrame[0]));
            luListByteOffset += 384;
            lpList[0x08 / 4] = luMainMemOffset;
        }
        while (luIndex < lpFrame[0x100 / 4]);
    }
    return lpResult;
}

// =============================================================================
// ShaderConstantsExternal::AddToDispatchBinFromStatePointers  @ 0x827E93B8
// Gather each listed constant's CURRENT pointer from the object context's
// constant shadow into the scratch pointer array, advancing the write cursor.
// The block is the serialised {muNumConstants @+0, mpaSourceIndices @+4} pair of
// a ShaderTechnique (32-bit LE image on PC after the flip port).
// =============================================================================
static Vector4** ShaderConstantsExternal_GatherFromContext(
        const u32* lpBlock, const DispatchObjectContext* lpContext, Vector4** lppCursor)
{
    const u32 luNumConstants = lpBlock[0];
    if (luNumConstants != 0)
    {
        // mpaSourceIndices: u32 slot in the 32-bit image (low-4GB fix-up convention).
        const u32* lpaSourceIndices =
            reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpBlock[1]));
        for (u32 luIndex = 0; luIndex < luNumConstants; ++luIndex)
        {
            lppCursor[luIndex] = const_cast<Vector4*>(reinterpret_cast<const Vector4*>(
                lpContext->mapConstantData[lpaSourceIndices[luIndex]]));
        }
    }
    return lppCursor + luNumConstants;
}

// =============================================================================
// AddShaderTechniqueConstantsToDispatchBin  @ 0x827F9FB8
// Reserve bin scratch for the technique's shader-constant pointer table and fill
// it from the object context. Block order (X360): vertex block A (@ST+28), then
// [pixel block C (@ST+80) unless z-only], vertex block B (@ST+44), then
// [pixel block D (@ST+96) unless z-only].
// =============================================================================
Vector4** AddShaderTechniqueConstantsToDispatchBin(DispatchBin* lpBin,
                                                   DispatchObjectContext* lpContext,
                                                   const MaterialTechnique* lpTechnique,
                                                   const MaterialAssembly* /*lpAssembly*/,
                                                   bool lbZOnly)
{
    // *technique (+0x00 u32 slot) = the ShaderTechnique image (SHADERS bundle).
    const u32 luShaderTechnique = *reinterpret_cast<const u32*>(lpTechnique);
    if (luShaderTechnique == 0)
    {
        // [PC bring-up gate] SHADERS.BNDL not yet loaded/fixed on this path: no
        // technique constant tables exist. Return a live empty scratch so the
        // command stream stays well-formed (the walk skips constant upload).
        // The reservation MUST go through the same Begin/EndAllocateMemory bracket
        // the real path uses: this runs inside the caller's open packet, and
        // AllocateMemoryFast asserts m_pPacketStart == NULL.
        Vector4** lppEmpty = reinterpret_cast<Vector4**>(lpBin->BeginAllocateMemory(1));
        lpBin->EndAllocateMemory(1);
        return lppEmpty;
    }

    const u8*  lpST    = reinterpret_cast<const u8*>(static_cast<uintptr_t>(luShaderTechnique));
    const u32* lpBlkA  = reinterpret_cast<const u32*>(lpST + 28);
    const u32* lpBlkB  = reinterpret_cast<const u32*>(lpST + 44);
    const u32* lpBlkC  = reinterpret_cast<const u32*>(lpST + 80);
    const u32* lpBlkD  = reinterpret_cast<const u32*>(lpST + 96);

    u32 luTotal = lpBlkA[0] + lpBlkB[0];
    if (!lbZOnly)
        luTotal += lpBlkC[0] + lpBlkD[0];

    // X360 reserves 4*total bytes (u32 pointers); the host uses pointer-width slots.
    const u32 luQwords = (static_cast<u32>(sizeof(void*)) * luTotal + 15u) >> 4;
    Vector4** lppScratch = reinterpret_cast<Vector4**>(lpBin->BeginAllocateMemory(luQwords));
    CGS_ASSERT(lppScratch != 0, "lMemoryForShaderConstantsVoid != NULL");

    Vector4** lppCursor = lppScratch;
    lppCursor = ShaderConstantsExternal_GatherFromContext(lpBlkA, lpContext, lppCursor);
    if (!lbZOnly)
        lppCursor = ShaderConstantsExternal_GatherFromContext(lpBlkC, lpContext, lppCursor);
    lppCursor = ShaderConstantsExternal_GatherFromContext(lpBlkB, lpContext, lppCursor);
    if (!lbZOnly)
        lppCursor = ShaderConstantsExternal_GatherFromContext(lpBlkD, lpContext, lppCursor);

    const u32 luUsedQwords = static_cast<u32>(
        (reinterpret_cast<u8*>(lppCursor) - reinterpret_cast<u8*>(lppScratch) + 15u) >> 4);
    lpBin->EndAllocateMemory(luUsedQwords);
    return lppScratch;
}

// =============================================================================
// CgsGraphics::DrawRenderable::AddToBin  @ 0x827FA0D0
// Stamp a DRAWRENDERABLE command into the frame's bin.
//
// ASM-TRUE routing (see renderer_wave_log.md for the per-store decode; the
// previous reconstruction mis-routed the trailer bytes and mis-named the
// "excluded mesh" statics -- they are the shader-constant dirty list):
//   * lbMarkAllConstantsDirty: rebuild the table's dirty list as the identity of
//     every used constant (the every-128-submissions "send everything" refresh).
//   * word1 = opaqueList<<24 | technique<<16 | transparentList<<8 | frustumEnable.
//   * trailer (cmd + 16 + dirtyQw*16): the DrawRenderableDispatchThreadInfo bytes
//     [0]=zOnly [1]=preZList [2]=instanceCount [3]=preZTechnique [4]=excludeBits,
//     and the renderable's LAST mesh pointer at trailer+8.
//   * the dirty-constant block is drained into cmd+16.
// =============================================================================
bool DrawRenderable::AddToBin(const Renderable* lpRenderable, DispatchFrame* lpFrame,
                              bool lbMarkAllConstantsDirty, s8 li8OpaqueList, s8 li8Technique,
                              u8 lu8FrustumEnable, u8 lu8TransparentList, bool lbZOnly,
                              u8 lu8PreZList, u8 lu8PreZTechnique,
                              s32 liInstanceCount, u8 lu8ExcludeMeshBits)
{
    CGS_ASSERT(lpRenderable != 0, "Adding null renderable pointer to bin");
    CGS_ASSERT(lpFrame != 0,      "Trying to use a NULL dispatch frame");

    // Refresh the dirty list to "every used constant" when requested.
    u8 lu8DirtyCount;
    if (lbMarkAllConstantsDirty)
    {
        const u8 lu8Used = mShaderConstantTable.mu8NumUsedConstants;
        for (u32 lu = 0; lu < lu8Used; ++lu)
            mShaderConstantTable.mau8DirtyConstants[lu] = static_cast<u8>(lu);
        mShaderConstantTable.mu8NumDirtyConstants = lu8Used;
        lu8DirtyCount = lu8Used;
    }
    else
    {
        lu8DirtyCount = mShaderConstantTable.mu8NumDirtyConstants;
    }

    // Dirty-constant block length in quad-words (host pointer width).
    const u16 lu16DirtyQw = static_cast<u16>(DirtyBlockQwords(lu8DirtyCount));

    // Allocate the command: header + dirty block + trailer + the worst-case
    // dirty-block headroom the X360 reserves on top (16 qwords there; the host
    // equivalent here).
    DispatchBin& lrBin = lpFrame->GetBin();
    u32* lpCommand = reinterpret_cast<u32*>(
        lrBin.AllocateCommand(lu16DirtyQw + 1u + DirtyBlockQwordsMax()));
    CGS_ASSERT(lpCommand != 0, "lpCmd");

    // The thread-info trailer sits after the dirty block.
    u8* lpTrailer = reinterpret_cast<u8*>(lpCommand) + (static_cast<u32>(lu16DirtyQw) << 4) + 16;
    lpTrailer[0] = lbZOnly ? 1u : 0u;                       // mbRenderZOnly
    lpTrailer[1] = lu8PreZList;                             // mu8PreZList (255 = none)
    lpTrailer[2] = static_cast<u8>(liInstanceCount);        // mi8InstanceCount
    lpTrailer[3] = lu8PreZTechnique;                        // mu8PreZTechniqueIndex
    lpTrailer[4] = lu8ExcludeMeshBits;                      // mu8ExcludeMeshBits
    // trailer+8: the renderable's last-mesh table entry (mppMeshes[nMeshes-1]).
    WriteCommandPointer(reinterpret_cast<u32*>(lpTrailer) + 2,
                        lpRenderable->mppMeshes[lpRenderable->mu16NumMeshes - 1]);

    // Drain the dirty shader constants into the block at cmd+16.
    mShaderConstantTable.AddDirtyConstantsToDispatchBin(
        reinterpret_cast<Vector4*>(lpCommand + (0x10 / 4)));

    WriteCommandPointer(&lpCommand[2], lpRenderable);       // [x64] words 2..3
    lpCommand[0] = (lu16DirtyQw + 1u) | 0x01000000u;        // DRAWRENDERABLE id + length
    lpCommand[1] = (static_cast<u32>(static_cast<u8>(li8OpaqueList)) << 24)
                 | (static_cast<u32>(static_cast<u8>(li8Technique)) << 16)
                 | (static_cast<u32>(lu8TransparentList) << 8)
                 | lu8FrustumEnable;
    return true;
}

// =============================================================================
// DrawRenderableMesh / DrawRenderableMeshZOnly AddToBin -- stamp a single mesh
// draw command. Shared validation; the differences are the command id word and
// the payload size (the X360 Z-only command carries no payload; the PC image
// carries the fixed scratch/WVP qwords on both -- see the header note).
// =============================================================================
static bool AddMeshCommandToBin(const RenderableMesh* lpMesh, DispatchBin* lpBin,
                                DispatchObjectContext* lpContext, u8 lu8TechniqueIndex,
                                u8 lu8InstanceCount, bool lbZOnly,
                                const rw::math::vpu::Matrix44* lpWorldViewProjection)
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
        lpBin, lpContext, lpTechnique, lpAssembly, lbZOnly);
    CGS_ASSERT(lpConstScratch != 0, "lpMemoryForShaderConstantPointers != NULL");

    // External-sampler payload (X360 formula): technique byte +0x23 ==
    // mi8NumExternalSamplers -- serialised read of the CgsMaterialTechnique image.
    const s8  li8NumExternal = reinterpret_cast<const s8*>(lpTechnique)[0x23];
    const u32 luExternQw = lbZOnly ? 0u
        : ((((4 * li8NumExternal + 15) & 0xFFFFFFF0u) + 48 * li8NumExternal) >> 4);

    const u32 luPayloadQw = KU_MESH_PAYLOAD_FIXED_QW + luExternQw;
    u32* lpCommand = reinterpret_cast<u32*>(lpBin->AllocateCommand(luPayloadQw));
    CGS_ASSERT(lpCommand != 0, "lpCmd != NULL");

    lpCommand[0] = luPayloadQw | (lbZOnly ? 0x03000000u : 0x02000000u);
    WriteCommandPointer(&lpCommand[2], lpMesh);            // [x64] words 2..3
    lpCommand[1] = ((static_cast<u32>(lu8InstanceCount) << 8) & 0xFF00u) | lu8TechniqueIndex;

    // Payload qword 0: the shader-constant scratch pointer ([x64]: X360 word3).
    WriteCommandPointer(&lpCommand[4], lpConstScratch);
    lpCommand[6] = 0;
    lpCommand[7] = 0;
    // Payload qwords 1..4: the object WVP. FLAG [PC bring-up shim]: carried in
    // the command so the fallback-shader dispatch can transform geometry before
    // the real shader-constant dispatch machinery is online. Remove when the
    // technique constant path is verified end-to-end.
    std::memcpy(&lpCommand[8], lpWorldViewProjection, 64);
    return true;
}

bool DrawRenderableMesh::AddToBin(const RenderableMesh* lpMesh, DispatchBin* lpBin,
                                  DispatchObjectContext* lpContext, u8 lu8TechniqueIndex,
                                  u8 lu8InstanceCount,
                                  const DrawRenderableDispatchThreadInfo* /*lpThreadInfo*/)
{
    // The WVP shim lane is filled by DrawRenderable::Interpret (the only caller
    // on the world path); direct callers pass identity via the overload below.
    static const rw::math::vpu::Matrix44 sIdentity =
        { { 1.f, 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f, 0.f }, { 0.f, 0.f, 1.f, 0.f }, { 0.f, 0.f, 0.f, 1.f } };
    return AddMeshCommandToBin(lpMesh, lpBin, lpContext, lu8TechniqueIndex,
                               lu8InstanceCount, false, &sIdentity);
}

bool DrawRenderableMeshZOnly::AddToBin(const RenderableMesh* lpMesh, DispatchBin* lpBin,
                                       DispatchObjectContext* lpContext, u8 lu8TechniqueIndex,
                                       u8 lu8InstanceCount,
                                       const DrawRenderableDispatchThreadInfo* /*lpThreadInfo*/)
{
    static const rw::math::vpu::Matrix44 sIdentity =
        { { 1.f, 0.f, 0.f, 0.f }, { 0.f, 1.f, 0.f, 0.f }, { 0.f, 0.f, 1.f, 0.f }, { 0.f, 0.f, 0.f, 1.f } };
    return AddMeshCommandToBin(lpMesh, lpBin, lpContext, lu8TechniqueIndex,
                               lu8InstanceCount, true, &sIdentity);
}

// =============================================================================
// CgsGraphics::DrawRenderableMesh::InterpretOcclusionQuery  @ 0x827EE550
// =============================================================================
void DrawRenderableMesh::InterpretOcclusionQuery(DispatchCommand* lpCommand, f32 /*lfTime*/)
{
    u32* lpWords = reinterpret_cast<u32*>(lpCommand);
    u32  luCommandId = CommandIdOf(lpWords[0]);
    CGS_ASSERT(luCommandId == DispatchCommand::E_DRAWRENDERABLEMESH
                   || luCommandId == DispatchCommand::E_DRAWRENDERABLEMESHZONLY,
               "pCommand->GetCommandID() == DRAWRENDERABLEMESH || "
               "pCommand->GetCommandID() == DRAWRENDERABLEMESHZONLY");

    const RenderableMesh* lpMesh =
        reinterpret_cast<const RenderableMesh*>(ReadCommandPointer(&lpWords[2]));
    const bool lbForceVisible = (lpMesh->mu8Flags & 0x01u) != 0;   // X360 mesh byte +0x25 bit
    const u32  luIndexCount   = lpMesh->mDrawIndexedParameters.muNumVertices;

    if (lbForceVisible || luIndexCount < suOcclusionCullIndexCountThreshold)
    {
        spOcclusionCullManager->TrivialAcceptOccludeeBoundingBox();
        return;
    }

    // The X360 then transforms the mesh PackedOobb by the command's world*view-
    // projection matrix and renders the occludee box into the GPU occlusion
    // query (OcclusionCullManager::RenderOccludeeBoundingBox). The occlusion
    // pass is gated OFF on the PC bring-up (mbOcclusionCull* default false), so
    // the render path stays a loud trap until reconstructed.
    CGS_ASSERT(false, "InterpretOcclusionQuery render path not yet reconstructed (VMX transform)");
}

// =============================================================================
// CgsGraphics::DrawRenderable::Interpret  @ 0x827FCDA0
//
// The object -> mesh expansion: restore the command's dirty shader constants
// into the object context, build the object's WVP, frustum-cull, then emit one
// DRAWRENDERABLEMESH[ZONLY] command + sort record per surviving mesh into the
// mesh-only frame's lists.
// =============================================================================
void DrawRenderable::Interpret(DispatchCommand* lpCommand, DispatchFrame* lpFrame,
                               void* lpUserData, f32 /*lfTime*/)
{
    u32* lpWords = reinterpret_cast<u32*>(lpCommand);
    DispatchObjectContext* lpContext = static_cast<DispatchObjectContext*>(lpUserData);

    CGS_ASSERT(lpCommand != 0, "lpCommand");
    CGS_ASSERT(lpFrame != 0,   "lpMeshOnlyDispatchFrame");
    CGS_ASSERT(lpContext != 0, "lpDispatchObjectContextVoid");
    CGS_ASSERT(CommandIdOf(lpWords[0]) == DispatchCommand::E_DRAWRENDERABLE,
               "lpCommand->GetCommandID() == DRAWRENDERABLE");

    const Renderable* lpRenderable =
        reinterpret_cast<const Renderable*>(ReadCommandPointer(&lpWords[2]));
    CGS_ASSERT(lpRenderable != 0, "lpRenderable");

    // word1 routing bytes.
    const u32 luWord1          = lpWords[1];
    const u8  lu8OpaqueList    = static_cast<u8>(luWord1 >> 24);
    const u8  lu8Technique     = static_cast<u8>(luWord1 >> 16);
    const u8  lu8Transparent   = static_cast<u8>(luWord1 >> 8);
    bool      lbFrustumTest    = (luWord1 & 0xFFu) != 0;
    const s32 liListBase       = lpContext->miListIdBase;
    const u32 luOpaqueListId   = static_cast<u32>(lu8OpaqueList + liListBase);
    const u32 luTransparentListId = static_cast<u32>(lu8Transparent + liListBase);

    // The thread-info trailer + the dirty-constant restore. GetPacketLength() =
    // dirtyQw + 1 (trailer), so the trailer is the packet's LAST qword.
    const u32 luDirtyQw = (lpWords[0] & 0x00FFFFFFu) - 1u;
    const u8* lpTrailer = reinterpret_cast<const u8*>(lpCommand) + 16u + (luDirtyQw << 4);

    // Restore: {count u8, idx bytes, aligned host-pointer array} at cmd+16.
    {
        const u8* lpBlock  = reinterpret_cast<const u8*>(lpCommand) + 16;
        const u32 luCount  = lpBlock[0];
        const u8* lpaIdx   = lpBlock + 1;
        const u8* lpaPtrs  = lpBlock + ((luCount + 4u) & ~3u);
        for (u32 lu = 0; lu < luCount; ++lu)
        {
            const rw::math::vpu::Vector4* lpData;
            std::memcpy(&lpData, lpaPtrs + lu * sizeof(void*), sizeof(void*));
            lpContext->mapConstantData[lpaIdx[lu]] = lpData;
        }
    }

    // WVP = worldMatrix (constant 0) * viewProjection (constant 3).
    const rw::math::vpu::Matrix44* lpWorld =
        reinterpret_cast<const rw::math::vpu::Matrix44*>(lpContext->mapConstantData[0]);
    const rw::math::vpu::Matrix44* lpViewProjection =
        reinterpret_cast<const rw::math::vpu::Matrix44*>(lpContext->mapConstantData[3]);
    CGS_ASSERT(lpWorld != 0,          "lpWorldMatrix != NULL");
    CGS_ASSERT(lpViewProjection != 0, "lpViewProjectionMatrix != NULL");

    rw::math::vpu::Matrix44 lWorldViewProjection;
    {
        // Row-vector 4x4 multiply (the X360 inline VMX chain, de-vectorised;
        // the world matrix is affine -- its rows' w lanes are 0/0/0/1).
        const rw::math::vpu::Vector4* lapW[4] =
            { &lpWorld->xAxis, &lpWorld->yAxis, &lpWorld->zAxis, &lpWorld->wAxis };
        const rw::math::vpu::Vector4* lapV[4] =
            { &lpViewProjection->xAxis, &lpViewProjection->yAxis,
              &lpViewProjection->zAxis, &lpViewProjection->wAxis };
        rw::math::vpu::Vector4* lapO[4] =
            { &lWorldViewProjection.xAxis, &lWorldViewProjection.yAxis,
              &lWorldViewProjection.zAxis, &lWorldViewProjection.wAxis };
        for (int liRow = 0; liRow < 4; ++liRow)
        {
            const rw::math::vpu::Vector4& lrW = *lapW[liRow];
            rw::math::vpu::Vector4&       lrO = *lapO[liRow];
            const f32 lfWLane = (liRow == 3) ? 1.0f : lrW.w;
            lrO.x = lrW.x * lapV[0]->x + lrW.y * lapV[1]->x + lrW.z * lapV[2]->x + lfWLane * lapV[3]->x;
            lrO.y = lrW.x * lapV[0]->y + lrW.y * lapV[1]->y + lrW.z * lapV[2]->y + lfWLane * lapV[3]->y;
            lrO.z = lrW.x * lapV[0]->z + lrW.y * lapV[1]->z + lrW.z * lapV[2]->z + lfWLane * lapV[3]->z;
            lrO.w = lrW.x * lapV[0]->w + lrW.y * lapV[1]->w + lrW.z * lapV[2]->w + lfWLane * lapV[3]->w;
        }
    }

    const u32 luNumMeshes = lpRenderable->mu16NumMeshes;

    // Object-level bounding-sphere frustum test (only worth it for >= 3 meshes):
    // when the whole object survives trivially, the per-mesh box tests are
    // skipped. The X360 tests the sphere's clip-space box: centre transformed by
    // WVP, extent = radius scaled rows.
    if (lbFrustumTest && luNumMeshes >= 3)
    {
        const f32 lfX = lpRenderable->mBoundingSphere.x;
        const f32 lfY = lpRenderable->mBoundingSphere.y;
        const f32 lfZ = lpRenderable->mBoundingSphere.z;
        const f32 lfR = lpRenderable->mBoundingSphere.w;   // Vector3Plus "plus" lane = the radius

        rw::math::vpu::Matrix44 lClipRows;
        const rw::math::vpu::Matrix44& lrM = lWorldViewProjection;
        // axis rows scaled by the radius; translation row = transformed centre.
        lClipRows.xAxis = { lrM.xAxis.x * lfR, lrM.xAxis.y * lfR, lrM.xAxis.z * lfR, lrM.xAxis.w * lfR };
        lClipRows.yAxis = { lrM.yAxis.x * lfR, lrM.yAxis.y * lfR, lrM.yAxis.z * lfR, lrM.yAxis.w * lfR };
        lClipRows.zAxis = { lrM.zAxis.x * lfR, lrM.zAxis.y * lfR, lrM.zAxis.z * lfR, lrM.zAxis.w * lfR };
        lClipRows.wAxis.x = lfX * lrM.xAxis.x + lfY * lrM.yAxis.x + lfZ * lrM.zAxis.x + lrM.wAxis.x;
        lClipRows.wAxis.y = lfX * lrM.xAxis.y + lfY * lrM.yAxis.y + lfZ * lrM.zAxis.y + lrM.wAxis.y;
        lClipRows.wAxis.z = lfX * lrM.xAxis.z + lfY * lrM.yAxis.z + lfZ * lrM.zAxis.z + lrM.wAxis.z;
        lClipRows.wAxis.w = lfX * lrM.xAxis.w + lfY * lrM.yAxis.w + lfZ * lrM.zAxis.w + lrM.wAxis.w;

        if (FrustumTest(lClipRows) == 0)
        {
            // Fully visible -- skip the per-mesh tests.
            lbFrustumTest = false;
        }
    }

    const DrawRenderableDispatchThreadInfo* lpThreadInfo =
        reinterpret_cast<const DrawRenderableDispatchThreadInfo*>(lpTrailer);
    DispatchBin& lrBin = lpFrame->GetBin();

    for (u32 luMesh = 0; luMesh < luNumMeshes; ++luMesh)
    {
        RenderableMesh* lpMesh = lpRenderable->mppMeshes[luMesh];

        // Thread-info exclusion: skip meshes whose flags intersect the exclude set.
        if ((lpMesh->mu8Flags & lpTrailer[4]) != 0)
            continue;

        // Per-mesh packed-OOBB frustum cull.
        if (lbFrustumTest)
        {
            rw::math::vpu::Matrix44 lMeshClipBox;
            lpMesh->mPackedBoundingBox.MultiplyByMatrix(lWorldViewProjection, lMeshClipBox);
            if (FrustumTest(lMeshClipBox) != 0)
                continue;
        }

        // Clamp the technique to the mesh's descriptor count.
        const u32 luNumVd = lpMesh->mu8NumVertexDescriptors;
        const MaterialAssembly* lpAssembly = lpMesh->mpMaterialAssembly;

        // [FLAG PC boot gate] The console always has every material resident, so it
        // dereferences the assembly unguarded. Here the world bundles are still being
        // brought up one at a time: a mesh whose Material import lives in a bundle that
        // is not staged (or that the pool refused when its heap filled) keeps the null
        // Pool::ResolveImportForEntry wrote, and the read below is an access violation
        // inside the per-frame render walk. Skip such a mesh and name it once.
        // DELETE when every world bundle resolves (the streamer's unload leg + the
        // remaining COMMONDATA/SHADERS staging).
        if (lpAssembly == 0 || lpAssembly->GetLength() == 0)
        {
            static bool sbLoggedNullAssembly = false;
            if (!sbLoggedNullAssembly && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedNullAssembly = true;
                *CgsDev::Log::gpDebugPrint
                    << "DrawRenderable::Interpret: mesh has no material assembly"
                       " (unresolved Material import) -- mesh skipped [FLAG PC boot gate]\n";
            }
            continue;
        }

        CGS_ASSERT(luNumVd == lpAssembly->GetLength(),
                   "lpRenderableMesh->GetNumVertexDescriptors() == lpMaterialAssembly->GetLength()");
        u32 luTechnique = lu8Technique;
        if (luNumVd - 1u < luTechnique) luTechnique = luNumVd - 1u;
        // FLAG [PC bring-up]: the console relies on the tripwire above holding, so its
        // clamp is against the descriptor count alone. Re-clamp into the assembly's real
        // range so a mismatch reported by that assert cannot walk off the technique table.
        if (luTechnique >= lpAssembly->GetLength())
            luTechnique = lpAssembly->GetLength() - 1u;

        const MaterialTechniqueView* lpTechnique =
            reinterpret_cast<const MaterialTechniqueView*>(lpAssembly->GetMaterial(luTechnique));

        // Transparent techniques (flags bit 0) route to the transparent list.
        const u32 luListId = (lpTechnique->mu16Flags & 1u) ? luTransparentListId : luOpaqueListId;
        DispatchList* lpList = lpFrame->GetList(luListId);
        lpList->ReserveKey();

        // Bin headroom check before the packet (X360: nextWord-used + 256 >= size).
        if (lrBin.GetUsedQwords() + 256u >= lrBin.GetSizeQwords())
            lrBin.HandleMemoryOverflow(0x100u);

        lrBin.BeginPacket();
        bool lbAdded;
        if (lpTrailer[0] != 0)
            lbAdded = DrawRenderableMeshZOnly::AddToBin(lpMesh, &lrBin, lpContext,
                          static_cast<u8>(luTechnique), lpTrailer[2], lpThreadInfo);
        else
            lbAdded = DrawRenderableMesh::AddToBin(lpMesh, &lrBin, lpContext,
                          static_cast<u8>(luTechnique), lpTrailer[2], lpThreadInfo);
        // [PC shim] overwrite the identity WVP lane the AddToBin seeded with the
        // real object WVP (payload qwords 1..4 -- see the header note).
        {
            DispatchCommand* lpPeek = lrBin.EndPacket();
            CGS_ASSERT(lpPeek != 0, "Failed to add mesh to dispatch bin");
            u32* lpMeshCmd = reinterpret_cast<u32*>(lpPeek);
            std::memcpy(&lpMeshCmd[8], &lWorldViewProjection, 64);
            CGS_ASSERT(lbAdded, "Failed to add mesh to dispatch bin");

            // Sort key from the technique's sort fields (X360 bit-packing across
            // +0x0A/+0x0C/+0x0E/+0x10; z-only folds a 15-bit quantised distance).
            // FLAG: the exact bit packing is Hex-Rays-fused in the export; the
            // key preserves the primary field order (postmortem: pin vs raw asm).
            s32 liSortKey;
            if (lpTrailer[0] != 0)
            {
                liSortKey = static_cast<s32>(
                    ((lpTechnique->mu16SortKeyB & 0xFFFu) << 16) | lpTechnique->mu16SortKeyC);
            }
            else
            {
                liSortKey = static_cast<s32>(
                    ((lpTechnique->mu16SortKeyB & 0xFFFu) << 19)
                  | ((lpTechnique->mu16SortKeyD & 0x7u) << 16)
                  | (lpTechnique->mu16SortKeyA & 0xFFFu));
            }
            lpList->Submit(liSortKey, lpPeek);
        }

        // Optional pre-Z re-emit.
        if (lpContext->mbPreZEnabled && lpTrailer[1] != 255u)
        {
            const u16 lu16Flags = lpTechnique->mu16Flags;
            if ((lu16Flags & 1u) == 0 && (((lu16Flags >> 3) & 1u) == 0 || lpContext->mbPreZAlphaEnabled))
            {
                u32 luPreZTechnique = luNumVd - 1u;
                if (luPreZTechnique > lpTrailer[3]) luPreZTechnique = lpTrailer[3];

                // Distance gate: the object's clip-space w (depth) against the
                // pre-Z distance threshold vector.
                const f32 lfDepth = lWorldViewProjection.wAxis.w;
                if (!(lfDepth > lpContext->mvPreZDistanceThreshold[0]))
                {
                    const MaterialTechniqueView* lpPreZTech =
                        reinterpret_cast<const MaterialTechniqueView*>(
                            lpAssembly->GetMaterial(luPreZTechnique));
                    DispatchList* lpPreZList =
                        lpFrame->GetList(static_cast<u32>(lpTrailer[1] + liListBase));
                    lpPreZList->ReserveKey();

                    if (lrBin.GetUsedQwords() + 256u >= lrBin.GetSizeQwords())
                        lrBin.HandleMemoryOverflow(0x100u);

                    lrBin.BeginPacket();
                    CGS_ASSERT(lpTrailer[0] == 0,
                               "Why would you ever want to do a preZ pass for Z only rendering?!");
                    const bool lbPreZAdded = DrawRenderableMeshZOnly::AddToBin(
                        lpMesh, &lrBin, lpContext, static_cast<u8>(luPreZTechnique),
                        lpTrailer[2], lpThreadInfo);
                    DispatchCommand* lpPreZPacket = lrBin.EndPacket();
                    CGS_ASSERT(lbPreZAdded && lpPreZPacket != 0,
                               "Failed to add mesh to dispatch bin (pre-z)");
                    u32* lpPreZCmd = reinterpret_cast<u32*>(lpPreZPacket);
                    std::memcpy(&lpPreZCmd[8], &lWorldViewProjection, 64);

                    // Pre-Z sort key packs the technique's B/D/A sort fields
                    // (Material[6]/[7]/[5] in the export). Same FLAG as above.
                    const s32 liPreZKey = static_cast<s32>(
                        ((((lpPreZTech->mu16SortKeyB << 12) & 0xFFF000u)
                          | (lpPreZTech->mu16SortKeyC & 0xFFFu)) << 12)
                        | (lpPreZTech->mu16SortKeyA & 0xFFFu));
                    lpPreZList->Submit(liPreZKey, lpPreZPacket);
                }
            }
        }
    }
}

// =============================================================================
// CgsGraphics::DispatchList::DispatchAllObjectToMesh  @ 0x827FD7D0
// Walk the (unsorted) key-block chain and expand every DRAWRENDERABLE object
// command into per-mesh commands in lpMeshOnlyFrame via DrawRenderable::Interpret.
// =============================================================================
DispatchCommand* DispatchList::DispatchAllObjectToMesh(DispatchPacketInterpreter* /*lpInterpreter*/,
                                                       DispatchFrame* lpMeshOnlyFrame,
                                                       DispatchObjectContext* lpContext,
                                                       s32 liFirst, s32 liCount)
{
    s32 liEnd = static_cast<s32>(muCount);
    if (liCount >= 0)
    {
        liEnd = liFirst + liCount;
        if (liEnd >= static_cast<s32>(muCount))
            liEnd = static_cast<s32>(muCount);
    }

    DispatchCommand* lpLast = 0;
    if (liFirst < liEnd)
    {
        CGS_ASSERT(mpBlockListHead != 0, "mpBlockListHead != NULL");

        s32 liBlockBase = 0;
        for (KeyBlock* lpBlock = mpBlockListHead; lpBlock != 0; lpBlock = lpBlock->mpNext)
        {
            const s32 liRemaining = liEnd - liBlockBase;
            if (liRemaining <= 0)
                break;

            s32 liFrom = liFirst - liBlockBase;
            if (liFrom < static_cast<s32>(lpBlock->muCount))
            {
                if (liFrom < 0) liFrom = 0;
                s32 liTo = liRemaining;
                if (liTo > static_cast<s32>(lpBlock->muCount))
                    liTo = static_cast<s32>(lpBlock->muCount);

                for (s32 liKey = liFrom; liKey < liTo; ++liKey)
                {
                    u32* lpPacket = PacketFromRecord(m_pBinBase, lpBlock->mpKeys[liKey]);
                    CGS_ASSERT(lpPacket != 0, "pPacket");
                    CGS_ASSERT(CommandIdOf(lpPacket[0]) == DispatchCommand::E_DRAWRENDERABLE,
                               "pCommand->GetCommandID() == DRAWRENDERABLE");
                    DrawRenderable::Interpret(reinterpret_cast<DispatchCommand*>(lpPacket),
                                              lpMeshOnlyFrame, lpContext, 0.0f);
                    lpLast = reinterpret_cast<DispatchCommand*>(lpPacket);
                }
            }
            liBlockBase += static_cast<s32>(lpBlock->muCount);
        }
    }
    return lpLast;
}

// =============================================================================
// CgsGraphics::DispatchList::DispatchAllMeshes  @ 0x827F2718
//
// Walk the sorted records and issue the GPU draw for every DRAWRENDERABLEMESH
// command. The X360 body programs the Xenos through the shadow device (state
// triple + vertex/pixel microcode programs + direct constant-memory writes +
// material samplers). The PC leaf keeps the walk, the technique-change cache and
// the mesh draw exactly, and routes the GPU programming through the same
// shadow::Device seam; the constant/sampler upload machinery rides the PC leaf
// in shadow::Device::SetMeshTechniquePC (see shadowingdevice.cpp) -- states and
// shaders fall back to the flagged bring-up defaults until the converted
// SHADERS bundle + material states are live. Occlusion-query interleaving
// (BeginQueries/conditional render) stays with the occlusion reconstruction --
// the PC occlusion switches default OFF.
// =============================================================================
s32 DispatchList::DispatchAllMeshes(DispatchPacketInterpreter* /*lpInterpreter*/,
                                    DispatchObjectContext* /*lpContext*/,
                                    u32 luFirst, s32 liCount)
{
    // Per-walk cache reset (X360 dword_83010FA4/FA8 + the program shadows).
    spLastTechnique    = reinterpret_cast<const MaterialTechniqueView*>(~uintptr_t(0));
    suLastTechniqueAux = ~uintptr_t(0);
    shadow::Device::ResetProgramShadows();

    u32 luEnd = muCount;
    if (liCount >= 0)
    {
        const u32 luRequested = luFirst + static_cast<u32>(liCount);
        if (luRequested < luEnd)
            luEnd = luRequested;
    }
    if (luFirst >= luEnd)
        return -1;

    CGS_ASSERT(mpSortedKeys != 0, "mpSortedKeys != NULL (PrepareSortJobInfo/SortForDispatch must run first)");

    for (u32 luIndex = luFirst; luIndex < luEnd; ++luIndex)
    {
        CGS_ASSERT(luIndex < muCount, "luIndex < muTotalKeyCount");
        u32* lpPacket = PacketFromRecord(m_pBinBase, mpSortedKeys[luIndex]);
        CGS_ASSERT(CommandIdOf(lpPacket[0]) == DispatchCommand::E_DRAWRENDERABLEMESH,
                   "lpCommand->GetCommandID() == DRAWRENDERABLEMESH");

        RenderableMesh* lpMesh =
            reinterpret_cast<RenderableMesh*>(ReadCommandPointer(&lpPacket[2]));
        const u32 luWord1        = lpPacket[1];
        const u8  lu8Technique   = static_cast<u8>(luWord1 & 0xFFu);
        const u8  lu8Instances   = static_cast<u8>((luWord1 >> 8) & 0xFFu);

        const MaterialAssembly* lpAssembly = lpMesh->mpMaterialAssembly;
        CGS_ASSERT(lpMesh->mu8NumVertexDescriptors == lpAssembly->GetLength(),
                   "lpMesh->GetNumVertexDescriptors() == lpMaterialAssembly->GetLength()");

        u32 luTechnique = lu8Technique;
        const u32 luLen = lpAssembly->GetLength();
        if (luLen - 1u < luTechnique) luTechnique = luLen - 1u;
        CGS_ASSERT(luTechnique < luLen, "Material technique index out of range.");

        const MaterialTechniqueView* lpTechnique =
            reinterpret_cast<const MaterialTechniqueView*>(lpAssembly->GetMaterial(luTechnique));
        CGS_ASSERT(lpTechnique != 0, "lpMaterial");

        Vector4** lppConstScratch =
            reinterpret_cast<Vector4**>(ReadCommandPointer(&lpPacket[4]));

        // Technique-change path: states + programs + technique constants.
        if (lpTechnique != spLastTechnique)
        {
            spLastTechnique = lpTechnique;
            shadow::Device::SetMeshTechniquePC(
                lpTechnique, reinterpret_cast<void* const*>(lppConstScratch), false);
        }

        // [PC bring-up shim] the per-object WVP carried in the command
        // (payload qwords 1..4) feeds the fallback-shader transform.
        shadow::Device::SetObjectTransformPC(reinterpret_cast<const f32*>(&lpPacket[8]));

        // Bind the mesh geometry + draw.
        shadow::Device::SetMeshBuffersPC(lpMesh, luTechnique);
        if (lu8Instances > 1u && lpMesh->mu8InstanceCount > 1u)
        {
            // Instanced world models are asserted off upstream
            // (WorldEntityModule: "Instancing not supported"); loud trap.
            CGS_ASSERT(false, "DrawInstancedIndexedPrimitive_Custom not yet reconstructed on PC");
        }
        else
        {
            shadow::Device::DrawIndexedMeshPC(lpMesh);
        }
    }
    return -1;
}

// =============================================================================
// CgsGraphics::DispatchList::DispatchAllMeshesZOnly  @ 0x827F7660
// Walk the sorted records and interpret every Z-only command. The Z-only
// interpreter itself (@0x827F5AC8, the shadow-map GPU path) is still a loud
// trap, and every PC caller (shadow cascades / pre-Z) is gated off; the walk is
// reconstructed so those passes light up with the interpreter.
// =============================================================================
void DispatchList::DispatchAllMeshesZOnly(DispatchPacketInterpreter* lpInterpreter,
                                          DispatchObjectContext* lpContext)
{
    spLastTechnique    = reinterpret_cast<const MaterialTechniqueView*>(~uintptr_t(0));
    suLastTechniqueAux = ~uintptr_t(0);
    shadow::Device::ResetProgramShadows();

    if (muCount == 0)
        return;

    CGS_ASSERT(mpSortedKeys != 0, "mpSortedKeys != NULL (PrepareSortJobInfo/SortForDispatch must run first)");

    for (u32 luIndex = 0; luIndex < muCount; ++luIndex)
    {
        CGS_ASSERT(luIndex < muCount, "luIndex < muTotalKeyCount");
        u32* lpPacket = PacketFromRecord(m_pBinBase, mpSortedKeys[luIndex]);
        CGS_ASSERT(lpPacket != 0, "pPacket");
        CGS_ASSERT(CommandIdOf(lpPacket[0]) == DispatchCommand::E_DRAWRENDERABLEMESHZONLY,
                   "pCommand->GetCommandID() == DRAWRENDERABLEMESHZONLY");
        DrawRenderableMeshZOnly::Interpret(reinterpret_cast<DispatchCommand*>(lpPacket),
                                           0, lpContext, lpInterpreter->GetTime());
    }
}

// =============================================================================
// ObjectToMeshJob::ExecuteImplementation  @ 0x827FF380  (SPU job path; PC-dead)
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
// DrawRenderableMeshZOnly::Interpret @ 0x827F5AC8 -- the depth-only GPU path
// (shadow cascades / pre-Z). ~3K lines of Xenos state programming; its PC
// callers are all gated off (shadow + pre-Z switches), so it stays a loud trap
// until the shadow-map pass is brought up.
// =============================================================================
void DrawRenderableMeshZOnly::Interpret(DispatchCommand* /*lpCommand*/, DispatchFrame* /*lpFrame*/,
                                        void* /*lpUserData*/, f32 /*lfTime*/)
{
    CGS_ASSERT(false, "DrawRenderableMeshZOnly::Interpret not yet reconstructed (GPU command stream)");
}

} // namespace CgsGraphics
