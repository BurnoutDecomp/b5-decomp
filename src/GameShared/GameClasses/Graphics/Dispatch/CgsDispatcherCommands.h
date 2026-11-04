#pragma once

#include "types.hpp"
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"
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
// the X360 ledger are declared here, along with the Pre-Z setters inlined in
// BrnRendererModule::Render.
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
struct DispatchObjectContext_JobState;

// -----------------------------------------------------------------------------
// DispatchObjectContext -- the per-pass "object -> mesh" expansion context the
// render thread builds on its stack each frame (BrnRendererModule::Render
// @0x8240BFA8 builds one 240-byte image and ConvertObjectsToMeshes memcpy's a
// fresh copy per pass). Layout recovered from the X360 byte offsets (comments);
// the x64 gate models it by named members (the constant shadow widens to host
// pointers). Each conversion job owns its DispatchObjectContext_JobState.
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
    DispatchObjectContext_JobState* mpJobState;         // +0xCC (DWARF CgsShaderConstants.h:749)
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
CGS_STATIC_ASSERT((sizeof(DrawRenderableDispatchThreadInfo) == 12), "ARTIST object command trailer");
CGS_STATIC_ASSERT((offsetof(DrawRenderableDispatchThreadInfo, mpLastRenderableMesh) == 8),
    "ARTIST object command trailer mesh");

// -----------------------------------------------------------------------------
// CallbackFn (CgsDispatcherCommands.h:165) -- a command that re-invokes an
// arbitrary callback over its custom section when interpreted.
// Interpret calls the native function pointer in header word 2 with the
// custom section and packet length.
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
    // ARTIST 8240C060, 8240C06C and 8240C0D4, inlined in Render.
    static void SetPreZMode(bool enabled) { sbPreZEnabled = enabled; }
    static void SetPreZAlpha(bool enabled) { sbPreZAlpha = enabled; }
    static void SetPreZNearThreshold(f32 distance)
    {
        sPreZNearThreshold.mV = __vspltw(__lvlx(&distance, 0), 0);
    }

    // ARTIST 827FA0D0. Header word 1 packs opaque list, transparent list,
    // technique and frustum enable. Words 2 and 3 point to the renderable and trailer.
    static bool AddToBin(const Renderable* lpRenderable, DispatchFrame* lpFrame,
                         bool lbMarkAllConstantsDirty, s8 li8OpaqueList, s8 li8TransparentList,
                         u8 lu8FrustumEnable, u8 lu8Technique, bool lbZOnly,
                         u8 lu8PreZList, u8 lu8PreZTechnique,
                         s32 liInstanceCount, u8 lu8ExcludeMeshBits);

    // X360 0x827FCDA0 -- the static command-stream interpreter (slot 1).
    static void Interpret(DispatchCommand* lpCommand, DispatchFrame* lpFrame,
                          void* lpUserData, f32 lfTime);

private:
    static bool sbPreZEnabled;
    static bool sbPreZAlpha;
    static rw::math::vpu::VecFloat sPreZNearThreshold;
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
    u16 mu16VertexProgramHash12; // +0x0A (DecFIGS CgsMaterialTechnique.h:199)
    u16 mu16PixelProgramHash12;  // +0x0C
    u16 mu16MaterialHash16;      // +0x0E
    u16 mu16Flags2;             // +0x10 (low 3 bits: technique priority)
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
