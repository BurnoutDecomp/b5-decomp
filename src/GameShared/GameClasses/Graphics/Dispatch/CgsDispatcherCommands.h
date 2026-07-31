#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"  // DispatchCommand base + DispatchFrame/DispatchBin/DispatchList

// =============================================================================
// CgsDispatcherCommands.h  (GameShared/GameClasses/Graphics/Dispatch)
//
// The render-dispatch *command* family. A frame's worth of draw work is encoded
// as a packed stream of 16-byte (quad-word) DispatchCommands inside a
// DispatchBin; the dispatcher walks that stream and "interprets" each command
// into GPU draw calls. Each concrete command (DrawRenderable / DrawRenderableMesh
// / DrawRenderableMeshZOnly / CallbackFn) derives from the common DispatchCommand
// header word and adds a fixed-size payload plus a trailing variable-length
// "custom section".
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (authoritative for behaviour and
// member offsets) gated on the X360 ledger, with declaration shape from the
// DecFIGS DWARF (.../Dispatch/CgsDispatcherCommands.h). Only methods attested in
// the X360 ledger are declared here; the DWARF also lists PS3-only setters
// (SetPreZMode/SetSortByDistance/Prefetch/SetMaterialShadowingAddress/
// InterpretOcclusionQuery_Bulk/...) that the X360 build folded inline and never
// emits as standalone functions, so they are intentionally omitted (X360 gate).
//
// CALLING-CONVENTION NOTE (read off the X360 prologues): the Interpret entry
// points are *static* command-stream interpreters of shape
//   void Interpret(DispatchCommand* lpCommand, DispatchFrame* lpFrame,
//                  void* lpUserData, float32_t lfTime)
// stored in the DispatchPacketInterpreter::m_paInterpreters table by
// SetupBuiltinInterpreters (slot 0 = CallbackFn, slot 1 = DrawRenderable,
// slot 3 = DrawRenderableMeshZOnly). The asm shows them taking the command in r3
// and never an implicit `this`, so they are declared static here.
// =============================================================================

// Global-scope dispatch objects (their full layouts live in Renderable.h /
// renderablemesh.h; pointer-only use here, so a forward declaration suffices).
struct Renderable;
struct RenderableMesh;

// The vpu vector type the constant shadow points at (pointer-only here).
namespace rw { namespace math { namespace vpu { struct Vector4; } } }

namespace CgsGraphics
{

// DispatchCommand / DispatchFrame / DispatchBin / DispatchList come from
// CgsDispatcher.h (included above).
struct OcclusionCullManager;  // CgsOcclusionCullManager.h
struct MaterialAssembly;      // CgsMaterialAssembly.h
struct MaterialTechnique;     // CgsMaterialTechnique.h (incomplete; pointer-only)

// -----------------------------------------------------------------------------
// DispatchObjectContext -- the per-pass "object -> mesh" expansion context the
// render thread builds on its stack each frame (BrnRendererModule::Render
// @0x8240BFA8 builds one 240-byte image and ConvertObjectsToMeshes memcpy's a
// fresh copy per pass). Layout recovered from the X360 byte offsets (comments);
// the x64 gate models it by named members (the constant shadow widens to host
// pointers). The heavier SPU job state (DispatchObjectContext_JobState) wraps
// this image and stays with the (PC-unused) job path.
//
//   +0x00  mapConstantData[50]      -- shader-constant pointer shadow; [0] = the
//                                     world matrix, [3] = the view-projection
//                                     (DrawRenderable::Interpret asserts both)
//   +0xC8  miListIdBase (word 50)   -- added to the command's list-id bytes
//   +0xD0  mbPreZEnabled            -- from BrnRendererModule::mbRenderPreZ
//   +0xD1  mbPreZAlphaEnabled       -- from mbRenderPreZAlpha
//   +0xE0  mvPreZDistanceThreshold  -- splat4(pre-Z distance), 16-aligned
// -----------------------------------------------------------------------------
struct DispatchObjectContext
{
    const rw::math::vpu::Vector4* mapConstantData[50]; // +0x00 (X360 u32 slots)
    s32   miListIdBase;                                // +0xC8
    u32   muPad0xCC;                                   // +0xCC (unrecovered word)
    bool  mbPreZEnabled;                               // +0xD0
    bool  mbPreZAlphaEnabled;                          // +0xD1
    u8    maPad0xD2[14];                               // +0xD2..0xDF (unrecovered)
    __declspec(align(16)) f32 mvPreZDistanceThreshold[4]; // +0xE0

    // X360 0x827E9418 -- zero the 50-slot shader-constant shadow block.
    void ResetShadowing();
};

// DispatchCommand (the 16-byte command header, CgsDispatcherCommands.h:63) is the
// shared base of every command and is homed in CgsDispatcher.h (where the bin's
// command pointers reference it). Its E_CommandID enum + GetCommandID/
// GetPacketLength accessors live there; this TU only consumes them.

// -----------------------------------------------------------------------------
// DrawRenderableDispatchThreadInfo (CgsDispatcherCommands.cpp:101) -- the small
// by-value packet that DrawRenderable::Interpret threads through the per-mesh
// DrawRenderableMesh[ZOnly]::AddToBin calls. Names/types from the DWARF.
// -----------------------------------------------------------------------------
struct DrawRenderableDispatchThreadInfo
{
    bool            mbRenderZOnly;          // +0x00
    u8              mu8PreZList;            // +0x01
    s8              mi8InstanceCount;       // +0x02
    u8              mu8PreZTechniqueIndex;  // +0x03
    u8              mu8ExcludeMeshBits;     // +0x04
    RenderableMesh* mpLastRenderableMesh;   // +0x08
};

// -----------------------------------------------------------------------------
// CallbackFn (CgsDispatcherCommands.h:165) -- a command that re-invokes an
// arbitrary callback over its custom section when interpreted.
//   custom section: [+0] padding, [+2] fn pointer void(*)(void*,u32) ...
// Interpret calls (word0+8 word) as fn(this+16, GetPacketLength()).
// -----------------------------------------------------------------------------
struct CallbackFn : public DispatchCommand
{
    // X360 0x827E92F0: assert id==CALLBACKFN, then invoke the stored callback
    // over the custom section. Static command-stream interpreter signature.
    static void Interpret(DispatchCommand* lpCommand, DispatchFrame* lpFrame,
                          void* lpUserData, f32 lfTime);
};

// -----------------------------------------------------------------------------
// DrawRenderable (CgsDispatcherCommands.h:187) -- a whole renderable (object)
// expanded into per-mesh draws at interpret time.
// -----------------------------------------------------------------------------
struct DrawRenderable : public DispatchCommand
{
    // X360 0x827FA0D0. The Hex-Rays 34-arg signature is a struct-by-value artefact;
    // the real source passes the per-object dispatch-thread-info + routing bytes by
    // value. ARGUMENT ROLES PINNED FROM THE ASM (word1 packing + the trailer stores
    // + DrawRenderable::Interpret's consumption; renderer_wave_log.md has the
    // per-store decode -- the earlier "sort/exclude/final" naming was wrong):
    //   word1 = opaqueList<<24 | technique<<16 | transparentList<<8 | frustumEnable
    //   trailer DrawRenderableDispatchThreadInfo:
    //     [0]=lbZOnly [1]=preZList [2]=instanceCount [3]=preZTechnique [4]=excludeMeshBits
    // Positions are unchanged from the previous declaration, so existing call sites
    // keep their (asm-pinned) argument VALUES.
    // ⭐ Parameters 5 and 7 were NAMED the wrong way round until 2026-07-31: word1 is
    // opaqueList<<24 | transparentList<<16 | technique<<8 | frustumEnable (Interpret
    // @0x827FCE94 adds the list base to the top TWO bytes), so the 5th argument is the
    // TRANSPARENT list and the 7th is the technique. See the note in Interpret.
    static bool AddToBin(const Renderable* lpRenderable, DispatchFrame* lpFrame,
                         bool lbMarkAllConstantsDirty, s8 li8OpaqueList, s8 li8TransparentList,
                         u8 lu8FrustumEnable, u8 lu8Technique, bool lbZOnly,
                         u8 lu8PreZList, u8 lu8PreZTechnique,
                         s32 liInstanceCount, u8 lu8ExcludeMeshBits);

    // X360 0x827FCDA0 -- the static command-stream interpreter (slot 1).
    static void Interpret(DispatchCommand* lpCommand, DispatchFrame* lpFrame,
                          void* lpUserData, f32 lfTime);
};

// -----------------------------------------------------------------------------
// MaterialTechniqueView -- the serialised-image view of CgsGraphics::
// MaterialTechnique the dispatch walkers read (the full type is owned by the
// CgsMaterialTechnique TU, not yet reconstructed; these offsets are the
// X360-attested reads in DrawRenderable::Interpret @0x827FCDA0 and
// DispatchList::DispatchAllMeshes @0x827F2718 -- documented serialised-blob
// reads, X360 32-bit image as ported by tools/assets/bundles).
//   +0x00  ShaderTechnique*   (import from SHADERS.BNDL, fixed by PostFixUp)
//   +0x04  render-state group {BlendState* @0, DepthStencilState* @4, RasterizerState* @8}
//   +0x08  u16 flags          (bit0 = transparent -> route to the transparent list;
//                              bit3 = alpha-tested -> pre-Z only when preZAlpha)
//   +0x0A/+0x0C/+0x0E/+0x10  u16 sort-key fields
//   +0x23  s8  numExternalSamplers
// -----------------------------------------------------------------------------
struct MaterialTechniqueView
{
    u32 muShaderTechnique;      // +0x00 (u32 slot in the 32-bit image)
    u32 muRenderStateGroup;     // +0x04 (u32 slot)
    u16 mu16Flags;              // +0x08
    u16 mu16SortKeyA;           // +0x0A
    u16 mu16SortKeyB;           // +0x0C
    u16 mu16SortKeyC;           // +0x0E
    u16 mu16SortKeyD;           // +0x10
};

// -----------------------------------------------------------------------------
// DrawRenderableMesh (CgsDispatcherCommands.h:257) -- a single mesh draw.
// -----------------------------------------------------------------------------
struct DrawRenderableMesh : public DispatchCommand
{
    // X360 0x827FA2A0.
    static bool AddToBin(const RenderableMesh* lpMesh, DispatchBin* lpBin,
                         DispatchObjectContext* lpContext, u8 lu8TechniqueIndex,
                         u8 lu8InstanceCount,
                         const DrawRenderableDispatchThreadInfo* lpThreadInfo);

    // X360 0x827EE550 -- issue a hardware occlusion query for this mesh.
    static void InterpretOcclusionQuery(DispatchCommand* lpCommand, f32 lfTime);
};

// -----------------------------------------------------------------------------
// DrawRenderableMeshZOnly (CgsDispatcherCommands.h:320) -- depth-only mesh draw.
// -----------------------------------------------------------------------------
struct DrawRenderableMeshZOnly : public DispatchCommand
{
    // X360 0x827FA4B0.
    static bool AddToBin(const RenderableMesh* lpMesh, DispatchBin* lpBin,
                         DispatchObjectContext* lpContext, u8 lu8TechniqueIndex,
                         u8 lu8InstanceCount,
                         const DrawRenderableDispatchThreadInfo* lpThreadInfo);

    // X360 0x827F5AC8 -- the static command-stream interpreter (slot 3).
    static void Interpret(DispatchCommand* lpCommand, DispatchFrame* lpFrame,
                          void* lpUserData, f32 lfTime);
};

// -----------------------------------------------------------------------------
// DispatchPacketInterpreter (CgsDispatcherCommands.h:363) -- holds the table of
// per-command interpreter function pointers and the mesh-only dispatch frame.
// Layout from the DWARF; ctor verified against X360 0x827E9360 (stores the
// table pointer at +0x00 and the count at +0x04 after asserting non-NULL).
// -----------------------------------------------------------------------------
struct DispatchPacketInterpreter
{
    // The static interpreter signature stored in the table.
    typedef void (*InterpretFn)(DispatchCommand*, DispatchFrame*, void*, f32);

    // X360 0x827E9360.
    DispatchPacketInterpreter(InterpretFn* lpaInterpreters, u32 luNumInterpreters);

    // PS3-only methods (Interpret/InterpretObjectToMesh/PrefetchMesh/...) are
    // folded inline / absent from the X360 ledger -> not declared (X360 gate).
    // The time/frame slots ARE poked inline by the X360 render loop
    // (BrnRendererModule::Render @0x8240BFA8: *(interp+12) = frame,
    // *(interp+8) = time), reproduced as these named inline accessors.
    void           SetTime(f32 lfTime)                       { mfTime = lfTime; }
    f32            GetTime() const                           { return mfTime; }
    void           SetSingleBufferedDispatchFrame(DispatchFrame* lpFrame)
                                                             { mpSingleBufferedDispatchFrame = lpFrame; }
    DispatchFrame* GetSingleBufferedDispatchFrame()          { return mpSingleBufferedDispatchFrame; }

private:
    InterpretFn*  m_paInterpreters;               // +0x00 (DWARF :412)
    u32           m_uNumInterpreters;             // +0x04 (DWARF :413)
    f32           mfTime;                          // +0x08 (DWARF :416)
    DispatchFrame* mpSingleBufferedDispatchFrame;  // +0x0C (DWARF :417)
};

// -----------------------------------------------------------------------------
// Module-level entry points (CgsDispatcherCommands.cpp).
// -----------------------------------------------------------------------------

// X360 0x827FF350 -- populate a 4-slot interpreter table with the built-in
// command interpreters (CallbackFn / DrawRenderable / <none> / ZOnly).
DispatchPacketInterpreter::InterpretFn*
SetupBuiltinInterpreters(DispatchPacketInterpreter::InterpretFn* lpaInterpreters);

} // namespace CgsGraphics
