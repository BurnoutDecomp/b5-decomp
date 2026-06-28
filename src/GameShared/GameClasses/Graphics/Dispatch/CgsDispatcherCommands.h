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

namespace CgsGraphics
{

// DispatchCommand / DispatchFrame / DispatchBin / DispatchList come from
// CgsDispatcher.h (included above).
struct OcclusionCullManager;  // CgsOcclusionCullManager.h
struct MaterialAssembly;      // CgsMaterialAssembly.h
struct MaterialTechnique;     // CgsMaterialTechnique.h (incomplete; pointer-only)

// -----------------------------------------------------------------------------
// DispatchObjectContext -- the SPU/PPU "object -> mesh" job context. The full
// per-job state (DispatchObjectContext_JobState, ~30 KB of local scratch DMA'd in
// from main memory) is owned by the dispatcher job and is reached as the SPU job
// image; only the leading shader-constant shadow block is cleared here.
// ResetShadowing is the one X360-attested method on the context type.
// -----------------------------------------------------------------------------
struct DispatchObjectContext
{
    // X360 0x827E9418 -- zero the leading 50-dword shader-constant shadow block.
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
    // the real source passes the per-object dispatch-thread-info + sort key bytes
    // by value. These are the leading scalar args the asm actually consumes
    // (lbRebuildExcluded selects the excluded-mesh list; li8SortHi/Lo + the list/
    // exclude/threadinfo/technique/final bytes are packed into the command words).
    // The DWARF prototype's full bitfield argument set is approximated by these
    // recovered bytes -- see the TU postmortem for the exact bit roles.
    static bool AddToBin(const Renderable* lpRenderable, DispatchFrame* lpFrame,
                         bool lbRebuildExcluded, s8 li8SortHi, s8 li8SortLo,
                         u8 lu8ExcludeByte, u8 lu8ListByte, bool lbPreZ,
                         u8 lu8ThreadInfoByte, u8 lu8TechniqueByte,
                         s32 li32SortKey, u8 lu8FinalSortByte);

    // X360 0x827FCDA0 -- the static command-stream interpreter (slot 1).
    static void Interpret(DispatchCommand* lpCommand, DispatchFrame* lpFrame,
                          void* lpUserData, f32 lfTime);
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

    // PS3-only methods (Interpret/InterpretObjectToMesh/PrefetchMesh/SetTime/...)
    // are folded inline / absent from the X360 ledger -> not declared (X360 gate).

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
