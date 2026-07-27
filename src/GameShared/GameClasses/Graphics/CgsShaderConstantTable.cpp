#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h" // DispatchBin::AllocateMemoryFast
#include "rw/math/vpu/types.h"                             // rw::math::vpu::Vector4
#include <cstring>                                         // strlen / strcpy

// CgsShaderConstantTable.cpp - the runtime-side ShaderConstantTable methods that
// build and update the per-frame named-constant register:
//   * AddShaderConstantArray                          @ 0x823F44E8
//   * FastNonOverlappedVectorMemcpy                   @ 0x822A08F8
//   * SetShaderConstantArrayData (Vector4* overload)  @ 0x822B3458
//   * UpdateShaderChangeTableAndGetConstantDestination @ 0x822A0A20
//
// Reconstructed from the BURNOUT_X360_ARTIST.XEX disassembly (authoritative for member
// store order / offsets) cross-checked against the PS3 DWARF (CgsShaderConstants.h param
// names: luIndex / luSizeOfEachEntryInBytes / luNumEntries / luNumConstants) and the
// Feb-2007 leak body for AddShaderConstantArray (the new char[strlen+1] + strcpy name
// store). The ShaderConstantTableElement / ShaderConstantTable layout lives in the owning
// header CgsShaderConstants.h; SetSize / SetNumEntries are bodied in CgsShaderConstants.cpp
// (same logical TU, separate object). The byte offsets in the X360 pseudocode (e.g.
// 4*(index+200) for the name array @ 800, 12*index for maConstants[index], 4*(index+150)
// for mapLatestCopyInDispatchBin @ 600, 0x578/0x580/0x581 for the used-count / dirty-count
// / dirty-list) all resolve to the named members below.

// -----------------------------------------------------------------------------
// Honest reconstruction note for the name store in AddShaderConstantArray:
//
// The X360 inlines a CgsStringUtils bounded string-duplicate: it allocates a buffer sized
// to strlen(name)+1 (the call to the out-of-scope allocator sub_82C08C00, which the DWARF +
// Feb-2007 leak attest as `new char[strlen(lpName)+1]`), guards the copy against the buffer
// size (firing the assert at CgsStringUtils.h:65 - reported on the X360 via an inlined
// CgsDev::StrStream into the assert message buffer; modelled here as the equivalent
// CGS_ASSERT, since the observable effect is the same FireAssert call), then byte-copies the
// name (the do/while stbx loop @ 0x823F4710). The allocator itself is homed elsewhere; here
// we reproduce its documented semantics (allocate + bounded copy) so the member STORE of the
// duplicated pointer into mapConstantNames[luIndex] is faithful.
// -----------------------------------------------------------------------------

void ShaderConstantTable::AddShaderConstantArray(u32 luIndex, const char* lpcName, u8 lu8SizeOfEachEntryInBytes, u8 lu8NumEntries)
{
    CGS_ASSERT(luIndex < KU_MAX_SHADER_CONSTANTS, "luIndex < KU_MAX_SHADER_CONSTANTS");
    CGS_ASSERT(lu8NumEntries, "luNumEntries");

    // Allocate a heap buffer sized to the name (strlen+1) and bounded-copy into it. The
    // X360 computes the buffer size (luBufferSize == strlen+1) and the source length
    // (luStringLength == strlen) and asserts the string fits before copying.
    const u32 luBufferSize = static_cast<u32>(strlen(lpcName)) + 1u;
    const u32 luStringLength = static_cast<u32>(strlen(lpcName));
    char* lpHeapName = new char[luBufferSize];
    CGS_ASSERT(luStringLength < luBufferSize, "luStringLength < luBufferSize");
    strcpy(lpHeapName, lpcName);

    mapConstantNames[luIndex] = lpHeapName;
    maConstants[luIndex].SetSize(lu8SizeOfEachEntryInBytes);
    maConstants[luIndex].SetNumEntries(lu8NumEntries);

    ++mu8NumUsedConstants;
    CGS_ASSERT(mu8NumUsedConstants < KU_MAX_SHADER_CONSTANTS, "mu8NumUsedConstants < KU_MAX_SHADER_CONSTANTS");
}

// 16-byte (quad-word) bulk copy of luNumConstants vectors from lpSrc to lpDest. The X360
// emits a Duff's-device unrolled-by-8 lvx128/stvx128 loop (jump table @ 0x822A0960); the
// observable effect is luNumConstants whole-Vector4 copies, which we reproduce as a plain
// loop. Asserts luNumConstants > 0 first (CgsShaderConstants.h:233).
void ShaderConstantTable::FastNonOverlappedVectorMemcpy(Vector4* lpDest, const rw::math::vpu::Vector4* lpSrc, u32 luNumQw)
{
    CGS_ASSERT(luNumQw > 0, "luNumConstants > 0");

    for (u32 luIndex = 0; luIndex < luNumQw; ++luIndex)
    {
        lpDest[luIndex] = lpSrc[luIndex];
    }
}

// Copy an array constant's source data into its freshly-allocated dispatch-bin destination.
// Bounds-checks the slot index, allocates/records the destination via
// UpdateShaderChangeTableAndGetConstantDestination, asserts it is non-null, then copies
// maConstants[luIndex].mu8NumEntries quad-words. (DWARF: the Vector4* overload @ 0x822B3458;
// the 4th memcpy arg is the per-element entry count read from maConstants[luIndex] offset 3.)
void ShaderConstantTable::SetShaderConstantArrayData(u32 luIndex, const rw::math::vpu::Vector4* lpaValues)
{
    CGS_ASSERT(luIndex < mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");

    Vector4* lpDest = UpdateShaderChangeTableAndGetConstantDestination(luIndex);
    CGS_ASSERT(lpDest, "lpDest");

    FastNonOverlappedVectorMemcpy(lpDest, lpaValues, maConstants[luIndex].GetNumEntries());
}

// The Matrix44* overload @ 0x827BAE60 (attested need: the shadow/frustum wave's cascade
// constant feed). Same slot bookkeeping as the Vector4* overload; the X360 loop copies
// 64 bytes (four quad-words == one Matrix44) per entry, mu8NumEntries entries, contiguous
// -- reproduced as a 4-qw-per-entry bulk copy. (No destination assert on the X360 here,
// unlike the Vector4* sibling -- matched.)
void ShaderConstantTable::SetShaderConstantArrayData(u32 luIndex, const rw::math::vpu::Matrix44* lpaValues)
{
    CGS_ASSERT(luIndex < mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");

    Vector4* lpDest = UpdateShaderChangeTableAndGetConstantDestination(luIndex);

    const u32 luNumEntries = maConstants[luIndex].GetNumEntries();
    if (luNumEntries > 0)
    {
        FastNonOverlappedVectorMemcpy(
            lpDest, reinterpret_cast<const rw::math::vpu::Vector4*>(lpaValues), luNumEntries * 4u);
    }
}

// Record that constant luIndex changed this frame and hand back a fresh destination for its
// value in the active dispatch bin. Appends luIndex to the dirty list, allocates
// mu16SizeOfArrayInQw quad-words from the bin, stores that pointer as the constant's latest
// dispatch-bin copy, and bumps the dirty count. (X360: dirty list @ 0x581 == mau8DirtyConstants,
// count @ 0x580, latest-copy array @ 600 == mapLatestCopyInDispatchBin, used-count @ 0x578.)
Vector4* ShaderConstantTable::UpdateShaderChangeTableAndGetConstantDestination(u32 luIndex)
{
    CGS_ASSERT(luIndex < mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");

    const u8 lu8NumDirtyConstants = mu8NumDirtyConstants;
    CGS_ASSERT(lu8NumDirtyConstants < KU_MAX_SHADER_CONSTANTS_DIRTY_LIST,
               "mu8NumDirtyConstants < KU_MAX_SHADER_CONSTANTS_DIRTY_LIST");

    const u32 luNumQuadWords = maConstants[luIndex].GetSizeOfArrayInQw();
    mau8DirtyConstants[lu8NumDirtyConstants] = static_cast<u8>(luIndex);

    Vector4* lpConstantsInDispatchBin =
        static_cast<Vector4*>(mpDispatchBin->AllocateMemoryFast(luNumQuadWords));
    mapLatestCopyInDispatchBin[luIndex] = lpConstantsInDispatchBin;
    mu8NumDirtyConstants = static_cast<u8>(lu8NumDirtyConstants + 1);

    return lpConstantsInDispatchBin;
}

// @ 0x827EDDC8 -- ShaderConstantTable::ShaderConstantTable.
//
// Clears all 50 element records (size 0, one entry, no cached array size) and their
// debug-name slots, zeroes the used/dirty counts, then registers the eight constants
// the ENGINE owns. The remaining 27 are registered by BrnRendererModule::Construct
// @0x8240A778 (the game-side set: view position, key light, fog, shadow map, the
// per-vehicle blocks and the two irradiance quadrics).
//
// This was an empty stub in WorldLinkStubs.cpp, which left mu8NumUsedConstants at 0 --
// so every SetShaderConstantData tripped "luIndex < mu8NumUsedConstants" and the
// dispatch interpreter found a NULL constant-0 (world transform) on the first world
// packet ("lpWorldMatrix != NULL").
ShaderConstantTable::ShaderConstantTable()
{
    mu8NumUsedConstants = 0;

    for (u32 luIndex = 0; luIndex < KU_MAX_SHADER_CONSTANTS; ++luIndex)
    {
        mapConstantNames[luIndex]                = 0;
        maConstants[luIndex].mu16SizeOfArrayInQw = 0;
        maConstants[luIndex].mu8SizeInBytes      = 0;
        maConstants[luIndex].mu8NumEntries       = 1;
    }

    AddShaderConstant(0u, "world",          64);
    AddShaderConstant(1u, "view",           64);
    AddShaderConstant(2u, "projection",     64);
    AddShaderConstant(3u, "viewProjection", 64);
    AddShaderConstant(4u, "worldViewProj",  64);
    AddShaderConstant(5u, "viewInverse",    64);
    AddShaderConstantArray(6u, "InstancingMatrixArray", 64, 5);
    AddShaderConstantArray(7u, "InstancingIndexArray",  16, 5);

    mu8NumDirtyConstants = 0;
}

// @ 0x823F4260 -- AddShaderConstant: the single-entry form of AddShaderConstantArray
// (SetSize(luSizeInBytes) + SetNumEntries(1)). Same heap-duplicated debug name.
void ShaderConstantTable::AddShaderConstant(u32 luIndex, const char* lpcName, u8 lu8SizeInBytes)
{
    AddShaderConstantArray(luIndex, lpcName, lu8SizeInBytes, 1);
}

// @ 0x822B32E8 -- SetShaderConstantData(u32, Vector4). Bounds-check the slot, take a
// fresh destination in the active dispatch bin, check the declared entry size matches
// the value being written, then store the 16-byte value.
// (CgsShaderConstants.h:396 / :400 / :401 assert sites.)
void ShaderConstantTable::SetShaderConstantData(u32 luIndex, Vector4 lValue)
{
    CGS_ASSERT(luIndex < mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");

    Vector4* lpDest = UpdateShaderChangeTableAndGetConstantDestination(luIndex);
    CGS_ASSERT(lpDest, "lpDest");
    CGS_ASSERT(maConstants[luIndex].GetSizeInBytes() == 16u,
               "sizeof(lData) == maConstants[luIndex].GetSizeInBytes()");

    *lpDest = lValue;
}

// The Vector3 overload: rw::math::vpu::Vector3 IS a 16-byte lane register with an
// unused w, so the store is the same single quad-word as the Vector4 form.
void ShaderConstantTable::SetShaderConstantData(u32 luIndex, Vector3 lValue)
{
    CGS_ASSERT(luIndex < mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");

    Vector4* lpDest = UpdateShaderChangeTableAndGetConstantDestination(luIndex);
    CGS_ASSERT(lpDest, "lpDest");
    CGS_ASSERT(maConstants[luIndex].GetSizeInBytes() == 16u,
               "sizeof(lData) == maConstants[luIndex].GetSizeInBytes()");

    lpDest->x = lValue.x;
    lpDest->y = lValue.y;
    lpDest->z = lValue.z;
    lpDest->w = lValue.w;
}

// The Matrix44 overload: four quad-words stored verbatim (declared size 64).
void ShaderConstantTable::SetShaderConstantData(u32 luIndex, Matrix44 lValue)
{
    CGS_ASSERT(luIndex < mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");

    Vector4* lpDest = UpdateShaderChangeTableAndGetConstantDestination(luIndex);
    CGS_ASSERT(lpDest, "lpDest");
    CGS_ASSERT(maConstants[luIndex].GetSizeInBytes() == 64u,
               "sizeof(lData) == maConstants[luIndex].GetSizeInBytes()");

    lpDest[0] = lValue.xAxis;
    lpDest[1] = lValue.yAxis;
    lpDest[2] = lValue.zAxis;
    lpDest[3] = lValue.wAxis;
}

// @ 0x822B33B8 (the X360's `sub_822B33B8`, called as the world transform setter from
// WorldEntityModule::RenderInstance) -- SetShaderConstantData(u32, Matrix44Affine).
// Widens the three-row affine into a full Matrix44: rows 0..2 get their w lane forced
// to 0 and row 3 (the translation) gets w = 1. That is exactly the asm's four
// vrlimi128 inserts (a zero vector into rows 0..2, vcfsx-produced 1.0f into row 3).
void ShaderConstantTable::SetShaderConstantData(u32 luIndex, Matrix44Affine lValue)
{
    CGS_ASSERT(luIndex < mu8NumUsedConstants, "luIndex < mu8NumUsedConstants");

    Vector4* lpDest = UpdateShaderChangeTableAndGetConstantDestination(luIndex);

    lpDest[0].x = lValue.xAxis.x; lpDest[0].y = lValue.xAxis.y; lpDest[0].z = lValue.xAxis.z; lpDest[0].w = 0.0f;
    lpDest[1].x = lValue.yAxis.x; lpDest[1].y = lValue.yAxis.y; lpDest[1].z = lValue.yAxis.z; lpDest[1].w = 0.0f;
    lpDest[2].x = lValue.zAxis.x; lpDest[2].y = lValue.zAxis.y; lpDest[2].z = lValue.zAxis.z; lpDest[2].w = 0.0f;
    lpDest[3].x = lValue.wAxis.x; lpDest[3].y = lValue.wAxis.y; lpDest[3].z = lValue.wAxis.z; lpDest[3].w = 1.0f;
}
