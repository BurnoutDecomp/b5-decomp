#pragma once

// ===========================================================================
// XGRAPHICS -- the X360 XGRAPHICS GPU state-compiler helper library
// (BURNOUT_X360_ARTIST.XEX). This is the "CompilerToSSM" face of the SSM
// (Shader State Machine) micro-code compiler: a NAMESPACE of static helper
// functions that translate the attribute-state object (`pAS_Object`, the
// AS_Object below) into D3D/Xenos GPU register fields and "required render
// state" command-list entries.
//
// There is NO reference source and NO DWARF for this TU, so every shape and
// every numeric immediate below is reconstructed PURELY from the PowerPC asm
// of these 85 functions (the X360 ARTIST pseudocode + asm is the spine). The
// X360 SSMDebugPrint("Assertion failed: %s (%s:%u)", ...) idiom maps to the
// project CGS_ASSERT(cond,"msg") macro (the macro supplies __FILE__/__LINE__,
// so the original Xenon SDK file/line is dropped). `XGRAPHICS` is an X360
// graphics-SDK boundary, so its identifiers are preserved verbatim per the
// naming convention.
//
// THREE families live here:
//   * AS_* state accessors (the infra core the Compile* getters call):
//       AS_GetStateI / AS_GetStateF / AS_GetArrayStateI / AS_GetArrayStateF
//     read a scalar or array entry out of the AS_Object's flat state buffers.
//   * CompileGet*  register-field GETTERS  -- read AS state, pack a GPU
//     register field / float constant into the caller's out-buffer.
//   * CompileWith* register-field SETTERS  -- read AS state, branch, and emit
//     a CS_SetRequiredRenderState command (render-state id + stage + value).
//   * a few Memory* / OBJLIST_* / CS_SetRequiredRenderState / SetBaseTextureHeader
//     plumbing helpers.
//
// LAYOUT NOTE (AS_Object): only the offsets these functions actually touch are
// attested by the asm; everything else is opaque padding sized to place them.
// The Hex-Rays output renders the AS_Object handle as a bare `int` -- here it
// is given a partial struct so the accessors read named fields instead of raw
// offset hacks. FLAG: the AS_Object interior between the attested fields is
// opaque padding (no DWARF), and the ArrayStateMap entry shape is inferred
// from the 12-byte stride + the +0/+4/+8 reads in AS_GetArrayState*.
// ===========================================================================

#include "types.hpp"

namespace XGRAPHICS
{

// ---------------------------------------------------------------------------
// AS_Object -- the attribute-state object the SSM compiler reads from.
// Attested offsets only (PowerPC displacements from the AS_* accessors):
//   +0x08 mpStateFloats     -- base of the flat scalar-state array; element i
//                              is read as *(mpStateFloats + 4*i)  (AS_GetStateF/I).
//   +0x44 muNumStates       -- bound for AS_GetStateF/I (eState < uNumStates).
//   +0x48 muNumArrayStates  -- bound for AS_GetArrayState* (eArrayState < n).
//   +0x54 mpArrayStateMap   -- base of the 12-byte ArrayStateMap[] entries.
// The interior gaps are opaque padding.
// ---------------------------------------------------------------------------
struct ArrayStateMapEntry
{
    u32 muBaseStateIndex; // +0x00  first scalar-state index for this array
    u32 muStride;         // +0x04  per-element stride (in state-array slots)
    u32 muMaxCount;       // +0x08  uMaxCount bound for uIndex
};

struct AS_Object
{
    u8                  maPad00[0x08];     // +0x00 .. +0x07 opaque
    u32*                mpStateFloats;     // +0x08  flat scalar-state array base
    u8                  maPad0C[0x44 - 0x0C]; // +0x0C .. +0x43 opaque
    u32                 muNumStates;       // +0x44  AS_GetStateF/I bound
    u32                 muNumArrayStates;  // +0x48  AS_GetArrayState* bound
    u8                  maPad4C[0x54 - 0x4C]; // +0x4C .. +0x53 opaque
    ArrayStateMapEntry* mpArrayStateMap;   // +0x54  ArrayStateMap[] base
};

// The verbose-trace debug callback the Compile* functions optionally invoke:
//   void (*pfnPrint)(int liChannel, const char* lpcFmt, ...)
typedef void (*CompilePrintFn)(int liChannel, const char* lpcFmt, ...);

// ---------------------------------------------------------------------------
// Required-render-state command list. CS_SetRequiredRenderState allocates a
// 16-byte node {type=0, id, stage, value} via the dispatcher's allocator and
// appends it to the compiled-shader's object list (at owner+0x984/+2436).
// Modelled opaquely: the compiled-shader handle is a bare `int` (the asm calls
// through *(a1+4) function pointers and indexes *(a1+2436)), so it is left as
// the Hex-Rays `int` handle rather than a fabricated full type. FLAG.
// ---------------------------------------------------------------------------

// ===== infra core ==========================================================

// @ 0x82C21DA0 -- read scalar float state eState from mpStateFloats[eState].
f32  AS_GetStateF(AS_Object* apObject, u32 auState);

// @ 0x82C21A78 (declared; AS_GetStateI itself is NOT one of this TU's 85 funcs
// -- it is the integer sibling of AS_GetStateF and is reconstructed here as the
// trivial reinterpret of the same slot so the Compile* getters/setters that
// read integer state can be bodied faithfully). The asm of AS_GetArrayStateI
// attests it forwards to AS_GetStateI exactly as AS_GetArrayStateF forwards to
// AS_GetStateF, with the same +0x44/+0x08 reads). FLAG: bodied from the sibling
// accessor's attested shape, not from its own dedicated postmortem entry.
s32  AS_GetStateI(AS_Object* apObject, u32 auState);

// @ 0x82C21E10 -- AS_GetArrayStateF(eArrayState, uIndex): bounds-check then
// read mpStateFloats[ base + stride*uIndex ] as float.
f32  AS_GetArrayStateF(AS_Object* apObject, u32 auArrayState, u32 auIndex);

// @ 0x82C21A78 -- integer counterpart of AS_GetArrayStateF.
s32  AS_GetArrayStateI(AS_Object* apObject, u32 auArrayState, u32 auIndex);

// @ 0x82C21B28 -- allocate+append a Required-Renderstate node {0,id,stage,value}.
s32  CS_SetRequiredRenderState(int aiCompiledShader, int aiRenderStateId,
                              int aiStage, int aiValue);

// ===== plumbing helpers ====================================================

// @ 0x82C218B0 -- splice apItem before the sentinel of the object list.
int  OBJLIST_AppendItem(int aiObjList, int aiItem);
// @ 0x82C21978 -- allocate a new node from the pool and append it.
int  OBJLIST_AppendNewItem(u32* apObjList, int aiPayload);
// @ 0x82C21BE0 -- build a fixed-size free-list memory pool.
int (**MemoryInit(int aiCount, int aiNodeSize,
                  int (*apfnAlloc)(int, int), int (*apfnFree)(int, int),
                  int (*apfnUser)(int, int)))(int, int);
// @ 0x82C21CA0 -- pop a free node from the pool chain (growing it if needed).
u32* MemoryNewNode(u32* apHead, int aiTag);

// @ 0x82C21108 -- assemble a base-texture GPU header from the (a1..a42) params.
int  SetBaseTextureHeader(int a1, int a2, int a3, int a4, int a5, int a6,
                          int a7, int a8, int a9, int a10, int a11, int a12,
                          int a13, int a14, int a15, int a16, int a17, int a18,
                          int a19, int a20, int a21, int a22, int a23, int a24,
                          int a25, int a26, int a27, int a28, int a29, int a30,
                          int a31, int a32, int a33, int a34, int a35, int a36,
                          int a37, int a38, int a39, u32* a40, int a41,
                          u32* a42);

// @ 0x8295E070 -- DECL-ONLY + FLAG: a multi-stage VMX (lvx128/stvlx/stvrx/
// dcbz128) cache-line-aligned memcpy. The asm is a hand-tuled vector pipeline;
// per the hard rule a multi-stage VMX/hardware pipeline is NOT paraphrased to
// scalar -- declaration only.
u8*  CopyMemoryCachedDst(u8* apDst, u8* apSrc, u32 auBytes);

// ===== CompileGet* register-field getters ==================================
// Each: CGS_ASSERT(pAS_Object), read AS state, pack into the out-buffer,
// optional verbose-trace callback, return 1.

int CompileGetBorderColorR(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetBorderColorG(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetBorderColorB(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetBorderColorA(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetFogBias(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetFogColorR(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetFogColorG(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetFogDensity(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetFogEnd(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetFogScale(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetFogStart(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetHOSMode(AS_Object* apObject, int* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetLodBias(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetLodClampMax(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetMemExportConstant0(AS_Object* apObject, u32 auStage, u32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetMemExportConstant1(AS_Object* apObject, int aiStage, u32* apOut, CompilePrintFn apfnPrint, int aiChannel);
// @ 0x82C25CC0 -- DECL-ONLY + FLAG: indexes an un-homed rodata lookup table
// (unk_82FA5220) by AS_GetArrayStateI(91,..)/(92,..); that table's contents are
// not attested by this function's asm, so the packed result cannot be grounded.
int CompileGetMemExportConstant2(AS_Object* apObject, int aiStage, u32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetMemExportConstant3(AS_Object* apObject, int aiStage, int* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetPointSizeMax(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetPointSizeMin(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetShadowFailThreshold(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetTextureFormatData(AS_Object* apObject, u32 auStage, u32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetVertexStreamFrequencyDivideInterval(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetWincoordOffsetsForPolyStippleX(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetWincoordOffsetsForPolyStippleY(AS_Object* apObject, int aiStage, f32* apOut, CompilePrintFn apfnPrint, int aiChannel);
int CompileGetWincoordOffsetsForWincoordX(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC00(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC01(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC02(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC03(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC10(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC22(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC30(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC31(AS_Object* apObject, int aiStage, f32* apOut);
int CompileGetYUVConstantsC33(AS_Object* apObject, int aiStage, f32* apOut);

// ===== CompileWith* register-field setters =================================
// Each: optional CGS_ASSERT(pAS_Object), read AS state, branch, emit a
// CS_SetRequiredRenderState command, optional verbose-trace callback.

int CompileWithBorderColor(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithCXVUTextureFormat(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithColorClamp01(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithColorClampNeg11(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithColorClampNone(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithEdgeFlag(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithFogBlendInRB(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithFogIndexOFog(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithFogIndexZ(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithHOSMode(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithLineAA(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithLodClampEnable(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPixelFog(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPointAA(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPointModeShadowBuffering(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPointSizeClamp(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPointSpriteReplaceModeNone(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPointSpriteReplaceModeR(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPointSpriteReplaceModeS(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPointSpriteReplaceModeZero(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPointSprites(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithPolyStipple(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithShadowBuffering(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithShadowGEqualCompareFunc(AS_Object* apObject, u32 auStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithShadowLEqualCompareFunc(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithShadowUsageAlpha(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithShadowUsageIntensity(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithShadowUsageLuminance(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTableFogALU(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTableFogExp(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTableFogExp2(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTableFogLinear(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTexProjectedW(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTexProjectedY(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTexProjectedZ(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTextureDimensionCube(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTransformedVertices(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithTwoSidedLighting(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithVertexFog(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);
int CompileWithYUVConversion(AS_Object* apObject, int aiStage, int aiCompiledShader, CompilePrintFn apfnPrint, int aiChannel);

} // namespace XGRAPHICS
