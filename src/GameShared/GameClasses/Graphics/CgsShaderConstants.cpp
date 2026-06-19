#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (SetSize / SetNumEntries)

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
    const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

    mppaConstantsInstanceData = reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(mppaConstantsInstanceData) + luBase);
    mpaProgramStateHandles = reinterpret_cast<renderengine::ProgramVariableHandle*>(reinterpret_cast<uintptr_t>(mpaProgramStateHandles) + luBase);
    mppacNames = reinterpret_cast<const char**>(reinterpret_cast<uintptr_t>(mppacNames) + luBase);

    if (muNumConstantsInstances != 0)
    {
        for (u32 luIndex = 0; luIndex < muNumConstantsInstances; ++luIndex)
        {
            const uintptr_t luName = reinterpret_cast<uintptr_t>(mppacNames[luIndex]);
            if (luName != 0)
            {
                mppacNames[luIndex] = reinterpret_cast<const char*>(luName + luBase);
            }
        }
    }

    return 0;
}

// CgsShaderConstants.cpp:1274
// Linear scan of the name array for an exact match of lpName, returning true if present.
// Faithful to the X360 inline strcmp: walk each candidate name and lpName in lock-step,
// comparing bytes, stopping at lpName's terminating NUL; a fully-matched candidate returns
// true, otherwise advance to the next name until muNumConstantsInstances is exhausted.
bool ShaderConstantsExternal::HasShaderConstant(const char* lpName) const
{
    if (muNumConstantsInstances == 0)
    {
        return false;
    }

    u32 luIndex = 0;
    for (const char* const* lppName = mppacNames; ; ++lppName)
    {
        const char* lpCandidate = *lppName;
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

        if (++luIndex >= muNumConstantsInstances)
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
    const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpBaseData);

    mpaProgramStateHandles = reinterpret_cast<renderengine::ProgramVariableHandle*>(reinterpret_cast<uintptr_t>(mpaProgramStateHandles) + luBase);
    mppaConstantsInstanceData = reinterpret_cast<u32**>(reinterpret_cast<uintptr_t>(mppaConstantsInstanceData) + luBase);
    mpauNamesHash = reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(mpauNamesHash) + luBase);
    mpauConstantsInstanceSize = reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(mpauConstantsInstanceSize) + luBase);

    if (muNumConstantsInstances != 0)
    {
        for (u32 luIndex = 0; luIndex < muNumConstantsInstances; ++luIndex)
        {
            mppaConstantsInstanceData[luIndex] =
                reinterpret_cast<u32*>(reinterpret_cast<uintptr_t>(mppaConstantsInstanceData[luIndex]) + luBase);
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
