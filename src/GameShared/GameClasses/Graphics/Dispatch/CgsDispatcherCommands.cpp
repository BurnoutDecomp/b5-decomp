// =============================================================================
#include "pc/gcm/renderengine/FrameProfile.h"
#include "GameShared/Jobs/ObjectToMesh/ObjectToMeshJob.h"
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
#include "pc/gcm/renderengine/InstancedDraw.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"        // DispatchBin / DispatchFrame / DispatchList
#include "GameShared/GameClasses/Graphics/Dispatch/CgsOcclusionCullManager.h"
#include "GameShared/GameClasses/Graphics/Dispatch/Renderable.h"
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"      // shadow::Device (the GPU flush leaf)
#include "GameShared/GameClasses/Graphics/CgsMaterialAssembly.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsMaterialAnimation.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"            // ShaderConstantTable
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // the one-shot bring-up gates
#include "BrnCommonTypes.h"                                                // Vector4 / Matrix44
#include "rw/math/vpu/vector4_operation.h"                              // Splat / lane-wise * and +

#include <cstring>   // memcpy
#include <cmath>     // [DIAG] std::sqrt (BRN_OOBB_DIAG, issue #26)
#include "GameShared/GameClasses/Development/BrnDiagBoundSurfaces.h"  // [diag] BrnDiag::IsSceneColourPass
#include <cstdio>    // [diag] snprintf (the [vpcmp] view-projection probe)
#include <cstdlib>   // [diag] getenv  (BRN_VP_PROBE)
#include <xmmintrin.h>

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

    // FLAG PC-platform leaf: ARTIST 827F29A0..827F2A8C fetches the packet two
    // draws ahead and the next mesh's first two 128-byte cache lines. x64 uses
    // 64-byte cache lines and host-width command pointers. These are hints,
    // never data reads: a short variable-sized mesh may end within the span.
    inline void PrefetchDispatchSpanPC(const void* lpData, u32 luBytes)
    {
        const uintptr_t luAddress = reinterpret_cast<uintptr_t>(lpData);
        for (u32 luOffset = 0; luOffset < luBytes; luOffset += 64u)
            _mm_prefetch(reinterpret_cast<const char*>(luAddress + luOffset), _MM_HINT_T0);
    }

    inline bool MeshPrefetchEnabledPC()
    {
        static const bool sbEnabled = [] {
            const char* lpcValue = std::getenv("BRN_MESH_PREFETCH");
            return !lpcValue || lpcValue[0] != '0';
        }();
        return sbEnabled;
    }

    inline void PrefetchMeshCommandsPC(DispatchCommand* lpBinBase, const u64* lpuKeys,
                                       u32 luIndex, u32 luEnd)
    {
        if (luIndex >= luEnd || luEnd - luIndex <= 2u) return;
        const u32* lpCommand2 = PacketFromRecord(lpBinBase, lpuKeys[luIndex + 2u]);
        const u32* lpCommand1 = PacketFromRecord(lpBinBase, lpuKeys[luIndex + 1u]);
        CGS_ASSERT(CommandIdOf(lpCommand1[0]) == DispatchCommand::E_DRAWRENDERABLEMESH,
                   "lpCommand1->GetCommandID() == DRAWRENDERABLEMESH");
        CGS_ASSERT(CommandIdOf(lpCommand2[0]) == DispatchCommand::E_DRAWRENDERABLEMESH,
                   "lpCommand2->GetCommandID() == DRAWRENDERABLEMESH");
        PrefetchDispatchSpanPC(lpCommand2, 128u);
        PrefetchDispatchSpanPC(ReadCommandPointer(&lpCommand1[2]), 256u);
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

// Shared-bin methods: ARTIST827EE7C8/827EE970/827F72A8. The X360 local jobs
// already write directly to shared memory; the pointers are native-width here.
void DispatchFrame::ConstructWithSharedBinMemory(DispatchList* lpaDispatchListArray,
        u32 luDispatchListCount, uintptr_t luDispatchBinMasterAddress,
        uintptr_t luSharedMemoryStartAddress, u32* lpSharedMemoryBlockNextFreeAtomic,
        u32 luSharedMemoryBlockMax)
{
    m_Bin = DispatchBin{};
    m_Bin.mpSharedBinStart = reinterpret_cast<DispatchCommand*>(luSharedMemoryStartAddress);
    m_Bin.m_pSharedNextFreeAtomic = lpSharedMemoryBlockNextFreeAtomic;
    m_Bin.m_uSharedMemoryBlockMax = luSharedMemoryBlockMax;
    m_Bin.mpDispatchFrame = this;
    mpDispatchBinMasterAddress = luDispatchBinMasterAddress;
    mpDispatchBinOutputAddress = 0;
    m_paLists = lpaDispatchListArray;
    muNumDispatchLists = luDispatchListCount;
    for (u32 luList = 0; luList < luDispatchListCount; ++luList)
    {
        DispatchList& lrList = m_paLists[luList];
        lrList.muCount = 0;
        lrList.mpDispatchBin = &m_Bin;
        lrList.muChainBlockCount = 0;
        lrList.mpRelocatedChainTailPC = nullptr;
        lrList.muWord00 = 0;
        lrList.mpBlockListHead = lrList.mpBlockListTail = nullptr;
        lrList.mpSortedKeys = nullptr;
        lrList.m_pBinBase = m_Bin.GetBase();
    }
}

void DispatchFrame::RelocateForMainMemory(uintptr_t luBinBase, uintptr_t luBinOutput,
                                         uintptr_t luBinMaster)
{
    for (u32 luList = 0; luList < muNumDispatchLists; ++luList)
        m_paLists[luList].RelocateForMainMemory(luBinBase, luBinOutput, luBinMaster);
}

void DispatchFrame::FlushBlockToSharedMemory()
{
    if (mpDispatchBinOutputAddress)
    {
        const uintptr_t luSource = reinterpret_cast<uintptr_t>(m_Bin.GetBase());
        RelocateForMainMemory(luSource, mpDispatchBinOutputAddress, mpDispatchBinMasterAddress);
        if (mpDispatchBinOutputAddress != luSource)
            std::memcpy(reinterpret_cast<void*>(mpDispatchBinOutputAddress),
                        reinterpret_cast<void*>(luSource),
                        sizeof(DispatchCommand) * m_Bin.GetSizeQwords());
    }
    mpDispatchBinOutputAddress = 0;
}

// X360-native members used by 827FF380 and 827EE760. The PS3 DMA staging
// buffers are unnecessary for the shared-address-space X360/PC path.
struct DispatchObjectContext_JobState
{
    DispatchFrame lDispatchFrameLocal;
    DispatchList lInputDispatchList;
    ObjectToMeshJobInfo* lpObjectToMeshJobInfo;
    uintptr_t luOffsetToMainMemory;
};

} // namespace CgsGraphics

// =============================================================================
// ShaderConstantsExternal::AddToDispatchBinFromStatePointers
// Gather each listed constant's CURRENT pointer from the object context's
// constant shadow into the scratch pointer array and advance the caller's write
// cursor past them. lpDest (the end of the caller's reservation) is returned
// unchanged; the caller closes the allocation with it.
// The block is the serialised {muNumConstantsInstances @+0, instance-index array
// @+4} words of a ShaderTechnique (32-bit image, low-4GB fix-up convention).
// =============================================================================
Vector4* ShaderConstantsExternal::AddToDispatchBinFromStatePointers(
        CgsGraphics::DispatchObjectContext* lpContext, Vector4* lpDest,
        Vector4**& lrpStatePointers) const
{
    const u32* const lpBlock = reinterpret_cast<const u32*>(this);
    const u32* const lpaSourceIndices =
        reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpBlock[1]));
    for (u32 luIndex = 0; luIndex < lpBlock[0]; ++luIndex)
    {
        lrpStatePointers[luIndex] = const_cast<Vector4*>(reinterpret_cast<const Vector4*>(
            lpContext->mapConstantData[lpaSourceIndices[luIndex]]));
    }
    lrpStatePointers += lpBlock[0];
    return lpDest;
}

namespace CgsGraphics
{

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
    Vector4*  lpDest    = reinterpret_cast<Vector4*>(lppScratch) + luQwords;
    lpDest = reinterpret_cast<const ShaderConstantsExternal*>(lpBlkA)->AddToDispatchBinFromStatePointers(lpContext, lpDest, lppCursor);
    if (!lbZOnly)
        lpDest = reinterpret_cast<const ShaderConstantsExternal*>(lpBlkC)->AddToDispatchBinFromStatePointers(lpContext, lpDest, lppCursor);
    lpDest = reinterpret_cast<const ShaderConstantsExternal*>(lpBlkB)->AddToDispatchBinFromStatePointers(lpContext, lpDest, lppCursor);
    if (!lbZOnly)
        lpDest = reinterpret_cast<const ShaderConstantsExternal*>(lpBlkD)->AddToDispatchBinFromStatePointers(lpContext, lpDest, lppCursor);

    lpBin->EndAllocateMemory(static_cast<u32>(lpDest - reinterpret_cast<Vector4*>(lppScratch)));
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
//   * word1 = opaqueList<<24 | transparentList<<16 | technique<<8 | frustumEnable.
//     (CORRECTED 2026-07-31 -- see the note in Interpret: the X360 adds liListBase to the
//     >>24 AND >>16 bytes, so those are the two LIST ids and the >>8 byte is the technique.)
//   * trailer (cmd + 16 + dirtyQw*16): the DrawRenderableDispatchThreadInfo bytes
//     [0]=zOnly [1]=preZList [2]=instanceCount [3]=preZTechnique [4]=excludeBits,
//     and the renderable's LAST mesh pointer at trailer+8.
//   * the dirty-constant block is drained into cmd+16.
// =============================================================================
bool DrawRenderable::AddToBin(const Renderable* lpRenderable, DispatchFrame* lpFrame,
                              bool lbMarkAllConstantsDirty, s8 li8OpaqueList, s8 li8TransparentList,
                              u8 lu8FrustumEnable, u8 lu8Technique, bool lbZOnly,
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
    // X360 @0x827FA264..0x827FA294: r11 = (extsb(arg4) << 8) | extsb(arg5);
    //                                 insrwi arg7, r11, 24, 0;  insrwi arg6, arg7, 24, 0
    // i.e. arg4<<24 | arg5<<16 | arg7<<8 | arg6.
    lpCommand[1] = (static_cast<u32>(static_cast<u8>(li8OpaqueList)) << 24)
                 | (static_cast<u32>(static_cast<u8>(li8TransparentList)) << 16)
                 | (static_cast<u32>(lu8Technique) << 8)
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

namespace
{
    // The table slot of the world matrix (ShaderConstantTable::E_WORLD_MATRIX; the console
    // compares the technique's first object vertex constant against zero).
    const u32 KU_SHADER_CONSTANT_WORLD_MATRIX = 0;

    // One output row of the row-vector product lrRow * lrMatrix, accumulated the way the
    // console's vmulfp / vmaddfp chain does: x term first, then y, z and w added on.
    rw::math::vpu::Vector4 TransformRow(const rw::math::vpu::Vector4& lrRow,
                                        const rw::math::vpu::Matrix44& lrMatrix)
    {
        rw::math::vpu::Vector4 lvResult = rw::math::vpu::Splat(lrRow.x) * lrMatrix.xAxis;
        lvResult = rw::math::vpu::Splat(lrRow.y) * lrMatrix.yAxis + lvResult;
        lvResult = rw::math::vpu::Splat(lrRow.z) * lrMatrix.zAxis + lvResult;
        lvResult = rw::math::vpu::Splat(lrRow.w) * lrMatrix.wAxis + lvResult;
        return lvResult;
    }
}

// =============================================================================
// CgsGraphics::DrawRenderableMesh::InterpretOcclusionQuery
// The occlusion-query pass's mesh interpreter (DispatchList::DispatchAllMeshOcclusionQueries).
// An instanced mesh, or one under the index-count threshold, is accepted as visible without a
// query. Otherwise the mesh's world matrix -- the first object vertex constant of its
// technique, which the assert pins to the table's world-matrix slot -- is multiplied by the
// occlusion manager's view-projection, and the mesh's packed box is rendered as the occludee.
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
    // [x64] the shader-constant scratch pointer is payload qword 0 (console word 3).
    Vector4** lppConstScratch = reinterpret_cast<Vector4**>(ReadCommandPointer(&lpWords[4]));

    if (lpMesh->mu8InstanceCount != 0
        || lpMesh->mDrawIndexedParameters.muNumVertices < suOcclusionCullIndexCountThreshold)
    {
        spOcclusionCullManager->TrivialAcceptOccludeeBoundingBox();
        return;
    }

    // The command's technique, clamped to the last technique the mesh and its assembly share.
    const MaterialAssembly* lpAssembly = lpMesh->mpMaterialAssembly;
    s32 liLastTechnique = lpAssembly->GetLength();
    if (lpMesh->mu8NumVertexDescriptors < liLastTechnique)
        liLastTechnique = lpMesh->mu8NumVertexDescriptors;
    --liLastTechnique;
    s32 liTechnique = static_cast<s32>(lpWords[1]);
    if (liLastTechnique < liTechnique)
        liTechnique = liLastTechnique;

    const MaterialTechnique* lpMaterial = lpAssembly->GetMaterial(static_cast<u32>(liTechnique));
    CGS_ASSERT(lpMaterial, "lpMaterial");

    // *lpMaterial (+0x00 u32 slot) is the ShaderTechnique image; its external object vertex
    // constants are the serialised {count, instance-index array} block at +0x1C.
    const u32  luShaderTechnique = *reinterpret_cast<const u32*>(lpMaterial);
    const u32* lpObjectVertexConstants =
        reinterpret_cast<const u32*>(static_cast<uintptr_t>(luShaderTechnique) + 0x1C);
    const u32* lpaConstantsInstanceData =
        reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpObjectVertexConstants[1]));
    CGS_ASSERT(lpaConstantsInstanceData[0] == KU_SHADER_CONSTANT_WORLD_MATRIX,
               "lpMaterial->GetShaderTechnique()->GetExternalObjectVertexShaderConstants()"
               ".mppaConstantsInstanceData[0] == ShaderConstantTable::E_WORLD_MATRIX");

    const rw::math::vpu::Matrix44& lrWorld =
        *reinterpret_cast<const rw::math::vpu::Matrix44*>(lppConstScratch[0]);
    const rw::math::vpu::Matrix44& lrViewProjection = spOcclusionCullManager->mViewProjectionMatrix;

    rw::math::vpu::Matrix44 lWorldViewProjection;
    lWorldViewProjection.xAxis = TransformRow(lrWorld.xAxis, lrViewProjection);
    lWorldViewProjection.yAxis = TransformRow(lrWorld.yAxis, lrViewProjection);
    lWorldViewProjection.zAxis = TransformRow(lrWorld.zAxis, lrViewProjection);
    lWorldViewProjection.wAxis = TransformRow(lrWorld.wAxis, lrViewProjection);

    rw::math::vpu::Matrix44 lOccludeeBox;
    lpMesh->mPackedBoundingBox.ToMatrix(lOccludeeBox);
    spOcclusionCullManager->RenderOccludeeBoundingBox(&lWorldViewProjection, lOccludeeBox);
}

// =============================================================================
// The per-WVP half of DrawRenderable::Interpret: the object bounding-sphere test
// and the per-mesh emit loop. Lifted verbatim out of Interpret so that ONE object
// command can be walked more than once with a different world matrix -- see the
// [PC bring-up shim] instancing note in Interpret below. With a single instance
// this is called exactly once and the code path is bit-for-bit what it was.
//
// lu8MeshInstanceCount is the instance byte stamped into each emitted mesh command
// (the console forwards the trailer's byte unchanged; the PC expansion forwards 1
// because it has already unrolled the instances).
// =============================================================================
// Inlined key construction in DrawRenderable::Interpret, ARTIST
// 0x827FD4CC..0x827FD5D4. Keys occupy up to 44 bits BEFORE Submit appends the
// 20-bit packet offset. The pseudocode loses the high half of the PPC registers.
static u64 MeshSortKey(const MaterialTechniqueView& lrTechnique, bool lbZOnly, f32 lfClipZ)
{
    const u64 luVertex = lrTechnique.mu16VertexProgramHash12 & 0xFFFu;
    const u64 luPixel = lrTechnique.mu16PixelProgramHash12 & 0xFFFu;
    const u64 luMaterial = lrTechnique.mu16MaterialHash16;
    const u64 luPriority = lrTechnique.mu16Flags2 & 7u;
    if (!lbZOnly)
        return (luPriority << 41) | (luPixel << 25) | (luMaterial << 9) | (luVertex >> 3);

    // unk_83011230: CRT @0x82C6C718..73C splats flt_820AD310 = 32767.0f.
    // fctiwz then signed clamp [0,32767]. Compare before the host conversion
    // to preserve PPC saturation for infinities and avoid an out-of-range cast.
    const f32 lfScaledDepth = lfClipZ * 32767.0f;
    const u64 luDepth = !(lfScaledDepth > 0.0f) ? 0u :
        lfScaledDepth >= 32767.0f ? 32767u : static_cast<u32>(lfScaledDepth);
    if (lrTechnique.mu16Flags & 8u)
    {
        // @0x827FD570 rlwinm r10,r9,12,4,15 keeps pixel-hash bits 4..11;
        // do not "fix" this to a full 12-bit hash. Alpha-tested draws prioritize
        // material over depth, unlike the opaque Z path below.
        return (u64(1) << 43) | ((luPixel & 0xFF0u) << 27) | (luMaterial << 15) | luDepth;
    }
    return (luPixel << 31) | (luDepth << 16) | luMaterial;
}

// ARTIST 0x827FD768..0x827FD794: three 12-bit fields, not a u32 key.
static u64 PreZSortKey(const MaterialTechniqueView& lrTechnique)
{
    return (static_cast<u64>(lrTechnique.mu16PixelProgramHash12 & 0xFFFu) << 24)
        | (static_cast<u64>(lrTechnique.mu16MaterialHash16 & 0xFFFu) << 12)
        | (lrTechnique.mu16VertexProgramHash12 & 0xFFFu);
}

static void EmitObjectMeshCommands(const Renderable* lpRenderable, DispatchFrame* lpFrame,
                                   DispatchObjectContext* lpContext, const u8* lpTrailer,
                                   u32 luOpaqueListId, u32 luTransparentListId,
                                   u8 lu8Technique, bool lbFrustumTest, s32 liListBase,
                                   u8 lu8MeshInstanceCount,
                                   const rw::math::vpu::Matrix44& lWorldViewProjection,
                                   const renderengine::WorldInstanceDrawPC* lpInstances = nullptr)
{
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

    // =====================================================================
    // [DIAG] NOT IN THE X360 BINARY -- BRN_OOBB_DIAG=1 (b5-decomp issue #26).
    //
    // MEASURES THE PER-MESH CULL BOX, NOT "is something on screen". For the first
    // renderables of a boot it decodes every mesh's PackedOobb and compares the box it
    // yields with the renderable's OWN LOD0 bounding sphere -- which is independently
    // known good (the world entity module hands the identical sphere to the scene
    // manager and the coarse query answers correctly with it).
    //
    // POSITIVE CONTROL, and it is the whole point: a CORRECT decode puts every mesh's
    // box centre inside that sphere and its half-extents at or under the sphere radius,
    // for models of every size. A decode that reads the wrong bytes cannot do that --
    // it produces the same tiny box near the model origin whatever the model is, so
    // `posOverR` and `extOverR` come out ~0 on a kilometre-scale mesh and the line says
    // so without any judgement call. DELETE-WHEN issue #26 is closed.
    // =====================================================================
    {
        static const s32 siOobbDiag = [] {
            const char* lpcValue = std::getenv("BRN_OOBB_DIAG");
            return lpcValue && lpcValue[0] && lpcValue[0] != '0' ? 1 : 0;
        }();
        static s32 siOobbSamples = 0;
        if (!lpContext->mpJobState && siOobbDiag != 0 && siOobbSamples < 120 && CgsDev::Log::gpDebugPrint != 0)
        {
            const f32 lfSphereR = lpRenderable->mBoundingSphere.w;
            if (lfSphereR > 50.0f)      // only the big meshes -- a backdrop is hundreds of metres
            {
                ++siOobbSamples;
                for (u32 luM = 0; luM < luNumMeshes && luM < 8u; ++luM)
                {
                    rw::math::vpu::Matrix44 lBox;
                    lpRenderable->mppMeshes[luM]->mPackedBoundingBox.ToMatrix(lBox);
                    const f32 lfEx = std::sqrt(lBox.xAxis.x * lBox.xAxis.x + lBox.xAxis.y * lBox.xAxis.y + lBox.xAxis.z * lBox.xAxis.z);
                    const f32 lfEy = std::sqrt(lBox.yAxis.x * lBox.yAxis.x + lBox.yAxis.y * lBox.yAxis.y + lBox.yAxis.z * lBox.yAxis.z);
                    const f32 lfEz = std::sqrt(lBox.zAxis.x * lBox.zAxis.x + lBox.zAxis.y * lBox.zAxis.y + lBox.zAxis.z * lBox.zAxis.z);
                    const f32 lfDx = lBox.wAxis.x - lpRenderable->mBoundingSphere.x;
                    const f32 lfDy = lBox.wAxis.y - lpRenderable->mBoundingSphere.y;
                    const f32 lfDz = lBox.wAxis.z - lpRenderable->mBoundingSphere.z;
                    const f32 lfPos = std::sqrt(lfDx * lfDx + lfDy * lfDy + lfDz * lfDz);
                    const f32 lfExt = (lfEx > lfEy ? (lfEx > lfEz ? lfEx : lfEz) : (lfEy > lfEz ? lfEy : lfEz));
                    *CgsDev::Log::gpDebugPrint
                        << "[oobb] meshes=" << static_cast<s32>(luNumMeshes)
                        << " mesh=" << static_cast<s32>(luM)
                        << " sphC=(" << lpRenderable->mBoundingSphere.x << "," << lpRenderable->mBoundingSphere.y
                        << "," << lpRenderable->mBoundingSphere.z << ") sphR=" << lfSphereR
                        << " boxPos=(" << lBox.wAxis.x << "," << lBox.wAxis.y << "," << lBox.wAxis.z << ")"
                        << " boxExt=(" << lfEx << "," << lfEy << "," << lfEz << ")"
                        << " posOverR=" << (lfPos / lfSphereR)
                        << " extOverR=" << (lfExt / lfSphereR)
                        << "\n";
                }
            }
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

        // The sort/pre-Z depth is the transformed MESH centre (the output's
        // fourth row at stack+var_B0), not the object's WVP translation.
        // FLAG PC-platform leaf: also initialize it when the coarse frustum
        // test skips per-mesh tests. The console then reads an unwritten stack
        // matrix; a native depth sort must never consume indeterminate values.
        rw::math::vpu::Matrix44 lMeshClipBox;
        if (lbFrustumTest || lpTrailer[0] != 0 ||
            (lpContext->mbPreZEnabled && lpTrailer[1] != 255u))
        {
            lpMesh->mPackedBoundingBox.MultiplyByMatrix(lWorldViewProjection, lMeshClipBox);
            if (lbFrustumTest && FrustumTest(lMeshClipBox) != 0)
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
            if (!lpContext->mpJobState && !sbLoggedNullAssembly && CgsDev::Log::gpDebugPrint != 0)
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
                          static_cast<u8>(luTechnique), lu8MeshInstanceCount, lpThreadInfo);
        else
            lbAdded = DrawRenderableMesh::AddToBin(lpMesh, &lrBin, lpContext,
                          static_cast<u8>(luTechnique), lu8MeshInstanceCount, lpThreadInfo);
        // [PC shim] overwrite the identity WVP lane the AddToBin seeded with the
        // real object WVP (payload qwords 1..4 -- see the header note).
        {
            DispatchCommand* lpPeek = lrBin.EndPacket();
            CGS_ASSERT(lpPeek != 0, "Failed to add mesh to dispatch bin");
            u32* lpMeshCmd = reinterpret_cast<u32*>(lpPeek);
            std::memcpy(&lpMeshCmd[8], &lWorldViewProjection, 64);
            WriteCommandPointer(&lpMeshCmd[6], lpInstances);
            CGS_ASSERT(lbAdded, "Failed to add mesh to dispatch bin");

            const bool lbZOnly = lpTrailer[0] != 0;
            lpList->Submit(MeshSortKey(*lpTechnique, lbZOnly,
                lbZOnly ? lMeshClipBox.wAxis.z : 0.0f), lpPeek);
        }

        // Optional pre-Z re-emit.
        if (lpContext->mbPreZEnabled && lpTrailer[1] != 255u)
        {
            const u16 lu16Flags = lpTechnique->mu16Flags;
            if ((lu16Flags & 1u) == 0 && (((lu16Flags >> 3) & 1u) == 0 || lpContext->mbPreZAlphaEnabled))
            {
                u32 luPreZTechnique = luNumVd - 1u;
                if (luPreZTechnique > lpTrailer[3]) luPreZTechnique = lpTrailer[3];

                // Distance gate: the mesh centre's clip-space w against the
                // pre-Z distance threshold vector.
                const f32 lfDepth = lMeshClipBox.wAxis.w;
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
                        lu8MeshInstanceCount, lpThreadInfo);
                    DispatchCommand* lpPreZPacket = lrBin.EndPacket();
                    CGS_ASSERT(lbPreZAdded && lpPreZPacket != 0,
                               "Failed to add mesh to dispatch bin (pre-z)");
                    u32* lpPreZCmd = reinterpret_cast<u32*>(lpPreZPacket);
                    std::memcpy(&lpPreZCmd[8], &lWorldViewProjection, 64);
                    WriteCommandPointer(&lpPreZCmd[6], lpInstances);

                    lpPreZList->Submit(PreZSortKey(*lpPreZTech), lpPreZPacket);
                }
            }
        }
    }
}

// =============================================================================
// CgsGraphics::DrawRenderable::Interpret  @ 0x827FCDA0
//
// The object -> mesh expansion: restore the command's dirty shader constants
// into the object context, build the object's WVP, frustum-cull, then emit one
// DRAWRENDERABLEMESH[ZONLY] command + sort record per surviving mesh into the
// mesh-only frame's lists (EmitObjectMeshCommands above).
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
    // ⭐ CORRECTED (race-car render wave 2026-07-31). Byte 2 and byte 1 were swapped here:
    // the X360 Interpret @0x827FCE94..0x827FCEDC adds the context's liListBase to BOTH the
    // >>24 and the >>16 bytes (`add r5, r5, r7` / `add r7, r6, r7`) and stores the >>8 byte
    // RAW. Two list ids and one technique -- so byte2 is the TRANSPARENT LIST and byte1 is
    // the TECHNIQUE, not the other way round. RenderInstance in BrnWorldEntityModule.cpp
    // passed AddToBin's arguments with the same swap, so the two errors cancelled and the
    // world still drew correctly; the race car passes (opaque 19, transparent 20, technique
    // 0..3) and would have been routed into list 19 with technique 20 (clamped to the last
    // material) and its transparent meshes into list 0..3 -- the SHADOW cascades.
    const u32 luWord1          = lpWords[1];
    const u8  lu8OpaqueList    = static_cast<u8>(luWord1 >> 24);
    const u8  lu8Transparent   = static_cast<u8>(luWord1 >> 16);
    const u8  lu8Technique     = static_cast<u8>(luWord1 >> 8);
    const bool lbFrustumTest   = (luWord1 & 0xFFu) != 0;
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

    // [DIAG] THE WORLD PASS'S VIEW-PROJECTION, PRINTED ONCE PER CALL SITE. NOT IN THE X360
    // BINARY, inert unless BRN_VP_PROBE names a value. DELETE-WHEN-STABLE.
    //
    // WHY: the tyre mark is rejected by the depth test with EVERY fragment strictly BEHIND what
    // the world wrote (measured: lt=0, eq=0, gt=coverage, over eight samples spread across a
    // drift), while the mark itself is laid 3 cm ABOVE the wheel contact point on a road the car
    // visibly sits on, and its own transform puts it on exactly the right PIXEL. Right x and y,
    // wrong z, is the signature of two passes projecting the same world point through different
    // matrices -- and the two passes do take their view-projection from different places: the
    // world from this dispatch command's shader constant 3, the trail from
    // ParticleRenderData::mCgsCamera. Nobody has compared them. This prints the world's side;
    // the trail's is already in [trailpass] xform and [ImVerts diag] vs c0..c3.
    {
        static const bool sbVpProbe = [] {
            const char* lpcValue = std::getenv("BRN_VP_PROBE");
            return lpcValue && lpcValue[0] && lpcValue[0] != '0';
        }();
        static u32 suVpSeen = 0;
        static u32 suVpPrinted = 0;
        // Sampled, not a prefix: this runs thousands of times a frame and the shadow cascades go
        // first, so a prefix budget would describe the cascade camera and never the scene one.
        // ⚠ GATED ON THE SCENE COLOUR PASS. Unfiltered, seven of the first eight samples
        // were the shadow cascades and an env-map face -- orthographic and 90-degree
        // cameras that have nothing to do with the depth the trail is tested against.
        if (!lpContext->mpJobState && sbVpProbe && lpViewProjection != 0 && suVpPrinted < 8u
            && ((++suVpSeen % 499u) == 0u) && BrnDiag::IsSceneColourPass())
        {
            ++suVpPrinted;
            const f32* const lpfM = reinterpret_cast<const f32*>(lpViewProjection);
            char lacMsg[400];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[vpcmp] world viewProj(const 3): m0=[%.6f %.6f %.6f %.6f] m1=[%.6f %.6f %.6f %.6f]"
                " m2=[%.6f %.6f %.6f %.6f] m3=[%.4f %.4f %.4f %.4f]\n",
                lpfM[0],  lpfM[1],  lpfM[2],  lpfM[3],
                lpfM[4],  lpfM[5],  lpfM[6],  lpfM[7],
                lpfM[8],  lpfM[9],  lpfM[10], lpfM[11],
                lpfM[12], lpfM[13], lpfM[14], lpfM[15]);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // ARTIST submits one instance group and leaves matrix selection to the GPU.
    // The native path carries constant6/7 snapshots to a D3D9 instance stream.
    // BRN_INSTANCING_EXPAND=1 retains the earlier scalar expansion for controlled
    // comparisons; unsupported native shaders are expanded at mesh submission.
    // Constants0/6/7 are world, InstancingMatrixArray and InstancingIndexArray.
    const u32 KU_CONSTANT_WORLD                   = 0u;
    const u32 KU_CONSTANT_INSTANCING_MATRIX_ARRAY = 6u;
    const u32 KU_CONSTANT_INSTANCING_INDEX_ARRAY  = 7u;

    const u8 lu8InstanceCount = lpTrailer[2];
    const rw::math::vpu::Matrix44* lpaInstanceWorlds = 0;
    const rw::math::vpu::Vector4*  lpaInstanceIndices = 0;
    u32 luNumInstanceDraws = 1;
    if (lu8InstanceCount > 1u)
    {
        lpaInstanceWorlds = reinterpret_cast<const rw::math::vpu::Matrix44*>(
            lpContext->mapConstantData[KU_CONSTANT_INSTANCING_MATRIX_ARRAY]);
        lpaInstanceIndices = lpContext->mapConstantData[KU_CONSTANT_INSTANCING_INDEX_ARRAY];
        if (lpaInstanceWorlds != 0)
        {
            luNumInstanceDraws = lu8InstanceCount;

            // ARTIST827FCDA0 emits one mesh command with the original instance
            // count. Carry its two constant arrays through the spare native
            // payload pointer; D3D9 consumes them as an instance stream.
            static const bool sbExpandInstances = [] {
                const char* value = std::getenv("BRN_INSTANCING_EXPAND");
                return value && value[0] && value[0] != '0';
            }();
            if (!sbExpandInstances && lu8InstanceCount <= 5)
            {
                auto* lpInstances = static_cast<renderengine::WorldInstanceDrawPC*>(
                    lpFrame->GetBin().AllocateMemoryFast(
                        (sizeof(renderengine::WorldInstanceDrawPC) + 15) / 16));
                ::new (lpInstances) renderengine::WorldInstanceDrawPC{
                    reinterpret_cast<const float*>(lpaInstanceWorlds),
                    reinterpret_cast<const float*>(lpaInstanceIndices),
                    reinterpret_cast<const float*>(lpViewProjection), lu8InstanceCount};
                rw::math::vpu::Matrix44 lWorldViewProjection;
                renderengine::WorldInstanceWvpPC(reinterpret_cast<const float*>(lpWorld),
                    reinterpret_cast<const float*>(lpViewProjection),
                    reinterpret_cast<float*>(&lWorldViewProjection));
                EmitObjectMeshCommands(lpRenderable, lpFrame, lpContext, lpTrailer,
                    luOpaqueListId, luTransparentListId, lu8Technique, lbFrustumTest,
                    liListBase, lu8InstanceCount, lWorldViewProjection, lpInstances);
                return;
            }

            // [DIAG] latched on the EXPANDED COUNT and on the spread of the instance
            // translations, not on a "ran once" bool -- an expansion that produced N
            // draws all at the same place would otherwise look identical to a correct
            // one in the log.
            static u32 suLoggedInstanceCount = 0;
            if (!lpContext->mpJobState && suLoggedInstanceCount != luNumInstanceDraws && CgsDev::Log::gpDebugPrint != 0)
            {
                suLoggedInstanceCount = luNumInstanceDraws;
                // [DIAG wheels] constant 0 -- the object's OWN world matrix, i.e. what a
                // non-instanced draw of this very packet would use -- and constant 3, the
                // view-projection every WVP in this packet is built from. Printed beside the
                // instance matrices so the two spaces can be compared directly.
                *CgsDev::Log::gpDebugPrint
                    << "[instancing] constant0 world translation ("
                    << lpWorld->wAxis.x << ", " << lpWorld->wAxis.y << ", "
                    << lpWorld->wAxis.z << ", " << lpWorld->wAxis.w << ")\n";
                *CgsDev::Log::gpDebugPrint
                    << "[instancing] constant3 viewProjection rows"
                    << " x(" << lpViewProjection->xAxis.x << "," << lpViewProjection->xAxis.y
                    << "," << lpViewProjection->xAxis.z << "," << lpViewProjection->xAxis.w << ")"
                    << " y(" << lpViewProjection->yAxis.x << "," << lpViewProjection->yAxis.y
                    << "," << lpViewProjection->yAxis.z << "," << lpViewProjection->yAxis.w << ")"
                    << " z(" << lpViewProjection->zAxis.x << "," << lpViewProjection->zAxis.y
                    << "," << lpViewProjection->zAxis.z << "," << lpViewProjection->zAxis.w << ")"
                    << " w(" << lpViewProjection->wAxis.x << "," << lpViewProjection->wAxis.y
                    << "," << lpViewProjection->wAxis.z << "," << lpViewProjection->wAxis.w << ")\n";
                *CgsDev::Log::gpDebugPrint
                    << "[instancing] DrawRenderable::Interpret expanding "
                    << luNumInstanceDraws << " instances; translations";
                for (u32 luLog = 0; luLog < luNumInstanceDraws; ++luLog)
                {
                    *CgsDev::Log::gpDebugPrint
                        << " (" << lpaInstanceWorlds[luLog].wAxis.x
                        << ", " << lpaInstanceWorlds[luLog].wAxis.y
                        << ", " << lpaInstanceWorlds[luLog].wAxis.z << ")";
                }
                *CgsDev::Log::gpDebugPrint << "\n";
            }
        }
        else
        {
            static bool sbLoggedMissingInstanceMatrices = false;
            if (!lpContext->mpJobState && !sbLoggedMissingInstanceMatrices && CgsDev::Log::gpDebugPrint != 0)
            {
                sbLoggedMissingInstanceMatrices = true;
                *CgsDev::Log::gpDebugPrint
                    << "DrawRenderable::Interpret: instanced object with no"
                       " InstancingMatrixArray (constant 6) -- drawing one instance"
                       " [PC bring-up shim]\n";
            }
        }
    }

    // Expanded comparison path: each mesh packet retains one array entry, as
    // required by the original non-instanced PC shader counterparts. Restore the
    // context afterward so later objects retain their own constant snapshots.
    const rw::math::vpu::Vector4* const lpSavedWorldConstant =
        lpContext->mapConstantData[KU_CONSTANT_WORLD];
    const rw::math::vpu::Vector4* const lpSavedInstancingMatrixConstant =
        lpContext->mapConstantData[KU_CONSTANT_INSTANCING_MATRIX_ARRAY];
    const rw::math::vpu::Vector4* const lpSavedInstancingIndexConstant =
        lpContext->mapConstantData[KU_CONSTANT_INSTANCING_INDEX_ARRAY];

    for (u32 luInstance = 0; luInstance < luNumInstanceDraws; ++luInstance)
    {
        const rw::math::vpu::Matrix44* lpWorldForDraw =
            (lpaInstanceWorlds != 0) ? &lpaInstanceWorlds[luInstance] : lpWorld;

        if (lpaInstanceWorlds != 0)
        {
            const rw::math::vpu::Vector4* const lpInstanceWorldRows =
                reinterpret_cast<const rw::math::vpu::Vector4*>(&lpaInstanceWorlds[luInstance]);
            lpContext->mapConstantData[KU_CONSTANT_WORLD]                   = lpInstanceWorldRows;
            lpContext->mapConstantData[KU_CONSTANT_INSTANCING_MATRIX_ARRAY] = lpInstanceWorldRows;
            if (lpaInstanceIndices != 0)
            {
                lpContext->mapConstantData[KU_CONSTANT_INSTANCING_INDEX_ARRAY] =
                    lpaInstanceIndices + luInstance;
            }
        }

        rw::math::vpu::Matrix44 lWorldViewProjection;
        {
            // Row-vector 4x4 multiply (the X360 inline VMX chain, de-vectorised;
            // the world matrix is affine -- its rows' w lanes are 0/0/0/1).
            const rw::math::vpu::Vector4* lapW[4] =
                { &lpWorldForDraw->xAxis, &lpWorldForDraw->yAxis,
                  &lpWorldForDraw->zAxis, &lpWorldForDraw->wAxis };
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

        EmitObjectMeshCommands(lpRenderable, lpFrame, lpContext, lpTrailer,
                               luOpaqueListId, luTransparentListId, lu8Technique,
                               lbFrustumTest, liListBase,
                               (lpaInstanceWorlds != 0) ? 1u : lu8InstanceCount,
                               lWorldViewProjection);
    }

    lpContext->mapConstantData[KU_CONSTANT_WORLD]                   = lpSavedWorldConstant;
    lpContext->mapConstantData[KU_CONSTANT_INSTANCING_MATRIX_ARRAY] =
        lpSavedInstancingMatrixConstant;
    lpContext->mapConstantData[KU_CONSTANT_INSTANCING_INDEX_ARRAY] =
        lpSavedInstancingIndexConstant;
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
s32 DispatchList::DispatchAllMeshes(DispatchPacketInterpreter* lpInterpreter,
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

    const bool lbPrefetch = MeshPrefetchEnabledPC();
    for (u32 luIndex = luFirst; luIndex < luEnd; ++luIndex)
    {
        if (lbPrefetch)
            PrefetchMeshCommandsPC(m_pBinBase, mpSortedKeys, luIndex, luEnd);
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
                lpTechnique, lpAssembly, reinterpret_cast<void* const*>(lppConstScratch), false);
        }

        // Per-mesh: the technique's OBJECT-scope external constant blocks (the X360 runs
        // these on every mesh, right after the technique-change block).
        shadow::Device::SetMeshObjectConstantsPC(
            lpTechnique, reinterpret_cast<void* const*>(lppConstScratch), false);

        // ARTIST 827F3A40..74: dispatch the CPU shader on EVERY mesh, even
        // when the material technique is still cached from the preceding draw.
        if (const ShaderConstantsCPU* lpCPU = lpAssembly->GetCPUShaderConstants())
            lpCPU->Dispatch(lpInterpreter->GetTime(), lpAssembly,
                            lpAssembly->GetMaterial(luTechnique));

        // [PC bring-up shim] the per-object WVP carried in the command
        // (payload qwords 1..4) feeds the fallback-shader transform.
        shadow::Device::SetObjectTransformPC(reinterpret_cast<const f32*>(&lpPacket[8]));

        // [DIAG wheels] the clip-space origin of a console-instanced mesh. Row 3 of the
        // per-object WVP IS the object origin transformed into clip space, so this says
        // whether the wheel lands on screen at all (|x|,|y| <= w and 0 <= z <= w) and which
        // technique/list it went to. DELETE with the wheel bring-up.
        // [DIAG wheels] SAMPLED ACROSS THE WHOLE RUN, not "the first N": the first samples of
        // the instanced and the plain streams come from different FRAMES (and therefore
        // different cameras), which makes comparing them meaningless. Every 4096th of each
        // stream puts the two side by side in the same stretch of the log.
        {
            const bool lbInstanced = (lpMesh->mu8InstanceCount > 1u);
            static u32 suWheelSeen = 0, suWheelLogged = 0;
            static u32 suPlainSeen = 0, suPlainLogged = 0;
            u32* lpuCounter = lbInstanced ? &suWheelSeen : &suPlainSeen;
            u32* lpuLogged  = lbInstanced ? &suWheelLogged : &suPlainLogged;
            const u32 luSeen   = (*lpuCounter)++;
            const u32 luStride = lbInstanced ? 65536u : 1048576u;
            if ((luSeen % luStride) == 0u && (*lpuLogged)++ < 12u && CgsDev::Log::gpDebugPrint != 0)
            {
                const f32* lpWvp = reinterpret_cast<const f32*>(&lpPacket[8]);
                *CgsDev::Log::gpDebugPrint
                    << (lbInstanced ? "[wheel-wvp] @" : "[plain-wvp] @") << luSeen
                    << " tech " << luTechnique << "/" << luLen
                    << " meshInstances " << static_cast<u32>(lpMesh->mu8InstanceCount)
                    << " clipOrigin (" << lpWvp[12] << ", " << lpWvp[13]
                    << ", " << lpWvp[14] << ", " << lpWvp[15] << ")\n";
            }
        }

        // Bind the mesh geometry + draw.
        shadow::Device::SetMeshBuffersPC(lpMesh, luTechnique);
        if (lu8Instances > 1u && lpMesh->mu8InstanceCount > 1u)
        {
            const auto* lpInstances = static_cast<const renderengine::WorldInstanceDrawPC*>(
                ReadCommandPointer(&lpPacket[6]));
            CGS_ASSERT(lpInstances && lpInstances->muCount == lu8Instances, "Missing native instance snapshots");
            if (lpInstances)
                shadow::Device::DrawInstancedMeshPC(lpMesh, lpInstances, lpTechnique,
                    reinterpret_cast<void* const*>(lppConstScratch), false);
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
// DrawRenderableMeshZOnly::Interpret @ 0x827F5AC8 -- the depth-only GPU path
// (the pre-Z pass and the shadow cascades).
//
// Same shape as the per-record body DispatchAllMeshes inlines, with the three
// differences the asm spells out:
//
//  1. STATES. The console reads the technique's MaterialState and binds its
//     DEPTH-STENCIL (+0x04) and RASTERISER (+0x08) objects exactly as the colour
//     walk does, but the BLEND object comes from one of two engine-wide Z-only
//     states -- `(technique->mu16Flags >> 3) & 1 ? dword_83010F90 :
//     dword_83010F8C` -- instead of the material's own. The PC selects the same
//     CgsBlendStateFactory slots 7/8; Construct fills them before dispatch.
//  2. PIXEL SIDE. Only an ALPHA-TESTED technique binds a pixel program and the
//     technique's samplers; otherwise the console binds `SetPixelProgram(0)` and
//     no textures at all. Neither pixel constant block runs -- which is exactly
//     why AddShaderTechniqueConstantsToDispatchBin skips the pixel blocks for a
//     z-only command, so the scratch table here is [A][B] rather than [A][C][B][D].
//  3. CACHE. The technique-change compare is against the companion cache word
//     (dword_83010FA8), not the colour walk's dword_83010FA4, so a pre-Z pass and
//     a colour pass over the same technique do not shadow each other's binds.
//
// The record decode, the technique clamp, the mesh bind and the draw are
// identical to the colour walk's.
// =============================================================================
void DrawRenderableMeshZOnly::Interpret(DispatchCommand* lpCommand, DispatchFrame* /*lpFrame*/,
                                        void* /*lpUserData*/, f32 lfTime)
{
    u32* const lpPacket = reinterpret_cast<u32*>(lpCommand);
    CGS_ASSERT(CommandIdOf(lpPacket[0]) == DispatchCommand::E_DRAWRENDERABLEMESHZONLY,
               "lpCommand->GetCommandID() == DRAWRENDERABLEMESHZONLY");

    RenderableMesh* lpMesh =
        reinterpret_cast<RenderableMesh*>(ReadCommandPointer(&lpPacket[2]));
    const u32 luWord1      = lpPacket[1];
    const u8  lu8Technique = static_cast<u8>(luWord1 & 0xFFu);
    const u8  lu8Instances = static_cast<u8>((luWord1 >> 8) & 0xFFu);

    const MaterialAssembly* lpAssembly = lpMesh->mpMaterialAssembly;

    // [FLAG PC boot gate] the same unresolved-Material guard DrawRenderable::Interpret
    // carries: a mesh whose Material import lives in a bundle the pool refused keeps a
    // null assembly, and the console's unguarded read would be an access violation here.
    if (lpAssembly == 0 || lpAssembly->GetLength() == 0)
        return;

    CGS_ASSERT(lpMesh->mu8NumVertexDescriptors == lpAssembly->GetLength(),
               "lpMesh->GetNumVertexDescriptors() == lpMesh->mpMaterialAssembly->GetLength()");

    u32 luTechnique = lu8Technique;
    const u32 luLen = lpAssembly->GetLength();
    if (luLen - 1u < luTechnique) luTechnique = luLen - 1u;
    CGS_ASSERT(luTechnique < luLen, "Material technique index out of range.");

    const MaterialTechniqueView* lpTechnique =
        reinterpret_cast<const MaterialTechniqueView*>(lpAssembly->GetMaterial(luTechnique));
    CGS_ASSERT(lpTechnique != 0, "lpMaterial");

    Vector4** lppConstScratch =
        reinterpret_cast<Vector4**>(ReadCommandPointer(&lpPacket[4]));

    if (reinterpret_cast<uintptr_t>(lpTechnique) != suLastTechniqueAux)
    {
        suLastTechniqueAux = reinterpret_cast<uintptr_t>(lpTechnique);
        shadow::Device::SetMeshTechniquePC(
            lpTechnique, lpAssembly, reinterpret_cast<void* const*>(lppConstScratch), true);
    }

    shadow::Device::SetMeshObjectConstantsPC(
        lpTechnique, reinterpret_cast<void* const*>(lppConstScratch), true);

    // ARTIST 827F68EC..6920: the same animation time/UV offset in depth passes.
    if (const ShaderConstantsCPU* lpCPU = lpAssembly->GetCPUShaderConstants())
        lpCPU->Dispatch(lfTime, lpAssembly, lpAssembly->GetMaterial(luTechnique));

    // [PC bring-up shim] the per-object WVP carried in the command (payload qwords 1..4).
    shadow::Device::SetObjectTransformPC(reinterpret_cast<const f32*>(&lpPacket[8]));

    shadow::Device::SetMeshBuffersPC(lpMesh, luTechnique);
    if (lu8Instances > 1u && lpMesh->mu8InstanceCount > 1u)
    {
        const auto* lpInstances = static_cast<const renderengine::WorldInstanceDrawPC*>(
            ReadCommandPointer(&lpPacket[6]));
        CGS_ASSERT(lpInstances && lpInstances->muCount == lu8Instances, "Missing native instance snapshots");
        if (lpInstances)
            shadow::Device::DrawInstancedMeshPC(lpMesh, lpInstances, lpTechnique,
                reinterpret_cast<void* const*>(lppConstScratch), true);
    }
    else
    {
        shadow::Device::DrawIndexedMeshPC(lpMesh);
    }
}

} // namespace CgsGraphics

// ARTIST827EE760. HandleMemoryOverflow has selected a fresh 16 KiB block.
// Keep each output key relative to the owner frame's master bin, not this block.
void ObjectToMeshJob::SharedMemoryChangeCallback(void* lpContext)
{
    auto* lpState = static_cast<CgsGraphics::DispatchObjectContext_JobState*>(lpContext);
    CgsGraphics::DispatchFrame& lrFrame = lpState->lDispatchFrameLocal;
    lpState->luOffsetToMainMemory = 0;
    auto* lpBlock = reinterpret_cast<CgsGraphics::DispatchCommand*>(lrFrame.GetActiveBlockInSharedMemory());
    lrFrame.GetBin().SetBinRange(lpBlock, lpBlock + CgsGraphics::DispatchBin::KU_BLOCK_SIZE_IN_QUAD_WORDS);
    const uintptr_t luMaster = lrFrame.GetDispatchBinMasterAddress()
        + reinterpret_cast<uintptr_t>(lrFrame.GetBin().GetBase()) - lrFrame.GetActiveBlockInSharedMemory();
    for (u32 luList = 0; luList < lrFrame.GetNumDispatchLists(); ++luList)
        lrFrame.GetList(luList)->SetDispatchBinMasterStart(reinterpret_cast<CgsGraphics::DispatchCommand*>(luMaster));
}

// ARTIST827FF380: private context/input view, bounded key-block walks, shared
// output blocks, then a final flush. r4 is the data argument; -1 is a start-index
// sentinel, not a frame number. The X360 path uses the caller's output-list array
// directly, so its conditional PS3 local-store copy is not taken.
void ObjectToMeshJob::ExecuteImplementation(ObjectToMeshJobInfo* lpData)
{
    using namespace CgsGraphics;
    if (lpData->miStartIndex == -1) return;

    DispatchObjectContext lContext = *lpData->mpDispatchObjectContext;
    DispatchObjectContext_JobState lState{};
    lContext.ResetShadowing();
    lContext.mpJobState = &lState;
    lState.lpObjectToMeshJobInfo = lpData;
    lState.lInputDispatchList = *lpData->mpDispatchListInput;
    CGS_ASSERT(lpData->muDispatchListOutputCount <= 26u, "Try increasing KU_MAX_OUTPUT_DISPATCH_LISTS");
    DispatchFrame& lrOutput = lState.lDispatchFrameLocal;
    lrOutput.ConstructWithSharedBinMemory(lpData->mpaDispatchListOutputArray,
        lpData->muDispatchListOutputCount, reinterpret_cast<uintptr_t>(lpData->mpDispatchBinMasterAddress),
        lpData->muSharedMemoryStartAddress, lpData->mpSharedMemoryBlockNextFreeAtomic,
        lpData->muSharedMemoryBlockMax);
    lrOutput.GetBin().SetMemoryCallback(&ObjectToMeshJob::SharedMemoryChangeCallback, &lState);
    lrOutput.GetBin().HandleMemoryOverflow(1);

    s32 liBlockBase = 0;
    for (DispatchList::KeyBlock* lpBlock = lpData->mpDispatchListInput->GetFirstKeyBlock();
         lpBlock; lpBlock = lpBlock->mpNext)
    {
        lState.lInputDispatchList.SetSingleKeyBlock(lpBlock);
        s32 liFirst = lpData->miStartIndex - liBlockBase;
        if (liFirst < 0) liFirst = 0;
        s32 liEnd = lpData->miEndIndex - liBlockBase;
        if (liEnd < 0) liEnd = 0;
        if (liEnd > static_cast<s32>(lpBlock->muCount)) liEnd = static_cast<s32>(lpBlock->muCount);
        lState.lInputDispatchList.DispatchAllObjectToMesh(lpData->mpDispatchInterpreter,
            &lrOutput, &lContext, liFirst, liEnd - liFirst);
        liBlockBase += lpBlock->muCount;
    }
    lrOutput.FlushBlockToSharedMemory();
}
