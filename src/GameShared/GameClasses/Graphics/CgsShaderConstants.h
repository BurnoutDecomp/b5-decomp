#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"               // Vector2/Vector3/Vector3Plus/Vector4/Matrix44/Matrix44Affine aliases
#include "rw/math/vpu/types.h"            // rw::math::vpu::Vector4 (qualified member type)

// CgsShaderConstants.h - the CGS shader-constant subsystem.
//
//   * ShaderConstantTable      : the runtime, per-frame register of named shader
//                                constants. Tracks the latest copy of each constant
//                                in the active push buffer / dispatch bin and a small
//                                dirty list that BeginFrame primes and
//                                AddDirtyConstantsToDispatchBin drains into a bin header.
//   * ShaderConstantsExternal  : a streamed-in (on-disk) block of "external" material
//                                shader constants - per-instance data, names, and the
//                                program-variable handles that bind them. FixUp relocates
//                                the block's internal pointers by the load base.
//   * ShaderConstantsInternal  : the equivalent block for engine-"internal" constants
//                                (hash-keyed instead of name-keyed); one extra pointer
//                                array (per-instance sizes) over the External layout.
//
// Reconstructed from the DecFIGS DWARF (CgsShaderConstants.{h,cpp}) and the X360
// pseudocode. Layouts are console-faithful: 32-bit X360 pointers, so each pointer /
// pointer-array member is 4 bytes, and the runtime table's raw byte offsets
// (maConstants @ 0, mapLatestCopyInDispatchBin @ 600, mu8NumUsedConstants @ 1400,
// mpDispatchBin @ 1404, mu8NumDirtyConstants @ 1408, mau8DirtyConstants @ 1409) fall
// straight out of the member order below.

// Opaque, pointer-only dependency: the per-program variable-binding handle lives in the
// render-engine SDK (SDKs/.../renderengine/programbuffer.h: renderengine::ProgramVariableHandle).
// These functions only store/relocate ProgramVariableHandle* (never dereference it), so a
// forward declaration is sufficient and avoids dragging in the whole SDK header tree.
namespace renderengine
{
    class ProgramBuffer;             // pointer-only (FixUp(const ProgramBuffer*) overload)
    struct ProgramVariableHandle;    // pointer-only member in External/Internal
}

// Global-scope dispatch handle, real home GameShared/GameClasses/Graphics/Dispatch/CgsMaterialTechnique.h
// (DWARF: top-level `struct ShaderConstantHandle`). Distinct from renderengine::ProgramVariableHandle.
// Pointer-only in the out-of-scope Internal::Dispatch{Pixel,Vertex}ShaderConstants declarations below.
struct ShaderConstantHandle;

namespace CgsGraphics
{
    // Pointer-only here (BeginFrame stores it, the bin functions take a separate header
    // pointer). Forward-declared to keep the heavy dispatch-pipeline header out of scope.
    class DispatchBin;

    struct ShaderConstantHashTable;  // pointer-only (Internal::FixUp(ProgramBuffer*, ShaderConstantHashTable*))

    // Pointer-only in the (out-of-scope) AddToDispatchBinFromStatePointers declaration.
    struct DispatchObjectContext;
}

// VecFloat / Matrix33 used by the (out-of-scope) SetShaderConstantData overloads are
// already aliased canonically in BrnCommonTypes.h (included above): the project-wide
// committed decision (flagged by the BrnPhysics-bodies group) is VecFloat == Vector4 and
// Matrix33 == rw::math::vpu::Matrix33. Re-typedef'ing them locally to a distinct
// forward-declared rw::math::vpu::VecFloat struct collided with that alias (C2371) and made
// the SetShaderConstantData(VecFloat) overload a duplicate of the Vector4 one (C2535), which
// broke compilation of this header for every includer. Reuse the committed aliases instead.

// CgsShaderConstants.h:97
const u32 KU_INSTANCING_MATRIX_ARRAY_MAX = 5;

// CgsShaderConstants.h:641 / 642 / 701 - fixed table dimensions.
const u32 KU_MAX_SHADER_CONSTANTS = 50;
const u32 KU_MAX_SHADER_CONSTANTS_INTERNAL = 50;
const u32 KU_MAX_SHADER_CONSTANTS_DIRTY_LIST = 255;

// CgsShaderConstants.h:100
// File-static shadowing-policy flags (defined in CgsShaderConstants.cpp). The shadowing
// helpers (DivideExact / DivideRoundUp) are reconstructed by their own TUs; declared here
// only for completeness of the namespace.
namespace ShaderConstantShadowing
{
    u32 DivideExact(u32 luNumerator, u32 luDenominator);
    u32 DivideRoundUp(u32 luNumerator, u32 luDenominator);
}

// CgsShaderConstants.h:134
// One slot in the runtime ShaderConstantTable: the size metadata for a named constant
// plus, per shader stage (vertex=0/pixel=1), the address of its latest copy in the push
// buffer. Console-faithful: 12 bytes total (u16 @ 0, u8 @ 2, u8 @ 3, ShaderState[2] @ 4).
struct ShaderConstantTableElement
{
    // CgsShaderConstants.h:195
    // Per-shader-stage shadow record: the address where this constant's most recent value
    // was written into the push buffer. 4 bytes (one 32-bit pointer).
    struct ShaderState
    {
        // CgsShaderConstants.h:199
        const rw::math::vpu::Vector4* mpLatestCopyInPushBuffer;   // offset 0x00

        // CgsShaderConstants.h:206
        void Reset();
    };

    // CgsShaderConstants.h:137
    void Construct();

    // CgsShaderConstants.h:141
    void SetSize(u8 lu8SizeInBytes);

    // CgsShaderConstants.h:153
    const u32 GetSizeInBytes() const { return mu8SizeInBytes; }

    // CgsShaderConstants.h:159
    const u32 GetNumEntries() const { return mu8NumEntries; }

    // CgsShaderConstants.h:165
    const u16 GetSizeOfArrayInQw() const { return mu16SizeOfArrayInQw; }

    // CgsShaderConstants.h:171
    void SetNumEntries(u8 lu8NumEntries);

    // CgsShaderConstants.h:184
    u16 mu16SizeOfArrayInQw;        // offset 0x00
    // CgsShaderConstants.h:185
    u8  mu8SizeInBytes;             // offset 0x02
    // CgsShaderConstants.h:186
    u8  mu8NumEntries;              // offset 0x03
    // CgsShaderConstants.h:219
    ShaderState maShaderState[2];   // offset 0x04 (vertex/pixel) -> element size 12 bytes
};

// CgsShaderConstants.h:223
// The runtime shader-constant register. A fixed array of 50 named constants, parallel
// arrays of the latest dispatch-bin copy and debug name per constant, the per-internal-
// variable shadow state, a used-count, the active dispatch bin, and a dirty list that
// BeginFrame primes (all used constants) and AddDirtyConstantsToDispatchBin drains.
struct ShaderConstantTable
{
    // CgsShaderConstants.h:276
    void Construct();

    // CgsShaderConstants.h:283
    // First parameter is the table SLOT INDEX (asserted < KU_MAX_SHADER_CONSTANTS), not a
    // name hash: corrected from the DWARF prototype (AddShaderConstant(uint32_t luIndex,
    // const char*, uint8_t)) and the X360 asm, which uses a2 as the maConstants[] index.
    void AddShaderConstant(u32 luIndex, const char* lpcName, u8 lu8SizeInBytes);

    // CgsShaderConstants.h:304
    // luIndex is the table slot (DWARF: AddShaderConstantArray(uint32_t luIndex, const char*,
    // uint8_t luSizeOfEachEntryInBytes, uint8_t luNumEntries)). Defined in this TU.
    void AddShaderConstantArray(u32 luIndex, const char* lpcName, u8 lu8SizeOfEachEntryInBytes, u8 lu8NumEntries);

    // CgsShaderConstants.h:325-441 - the SetShaderConstantData overload family.
    // (The leak's separate SetShaderConstantData(VecFloat) overload is omitted: the project
    // committed VecFloat == Vector4 in BrnCommonTypes.h, so it would duplicate the Vector4
    // overload below. The original VecFloat scalar-broadcast distinction is not modelled.)
    void SetShaderConstantData(u32 luIndex, const float& lfValue);
    void SetShaderConstantData(u32 luIndex, Vector2 lValue);
    void SetShaderConstantData(u32 luIndex, Vector3 lValue);
    void SetShaderConstantData(u32 luIndex, Vector3Plus lValue);
    void SetShaderConstantData(u32 luIndex, Vector4 lValue);
    void SetShaderConstantData(u32 luIndex, Matrix33 lValue);
    void SetShaderConstantData(u32 luIndex, Matrix44 lValue);
    void SetShaderConstantData(u32 luIndex, Matrix44Affine lValue);

    // CgsShaderConstants.h:454-524 - the SetShaderConstantArrayData overload family.
    void SetShaderConstantArrayData(u32 luIndex, const rw::math::vpu::Vector3* lpaValues);
    void SetShaderConstantArrayData(u32 luIndex, const rw::math::vpu::Vector3Plus* lpaValues);
    void SetShaderConstantArrayData(u32 luIndex, const rw::math::vpu::Vector4* lpaValues);
    void SetShaderConstantArrayData(u32 luIndex, const rw::math::vpu::Matrix44Affine* lpaValues);
    void SetShaderConstantArrayData(u32 luIndex, const rw::math::vpu::Matrix44* lpaValues);

    // CgsShaderConstants.h:542
    void SetShaderConstantDataInverseOrthonormal(u32 luIndex, Matrix44Affine lValue);

    // CgsShaderConstants.h:559 / 569
    u32 GetShaderConstantSizeInBytes(u32 luIndex);
    u32 GetShaderConstantSizeInBytes_SupportArrays(u32 luIndex);

    // CgsShaderConstants.h:578
    const char* GetShaderConstantName(u32 luIndex);

    // CgsShaderConstants.h:601
    ShaderConstantTable();

    // CgsShaderConstants.h:605 - defined in this TU.
    void BeginFrame(CgsGraphics::DispatchBin* lpDispatchBin);

    // CgsShaderConstants.h:608
    void EndFrame();

    // CgsShaderConstants.h:611 / 614
    void ResetShadowing();
    void ResetShadowingForDispatch();

    // CgsShaderConstants.h:618 / 621
    u8 GetNumDirtyConstants();
    u16 GetNumQWForDirtyHeaders();

    // CgsShaderConstants.h:625 - defined in this TU.
    void AddDirtyConstantsToDispatchBin(Vector4* lpBinHeaderSpace);

    // CgsShaderConstants.h:629
    void ReceiveDirtyConstantsFromDispatchBin(Vector4* lpBinHeaderSpace);

    // CgsShaderConstants.h:632 / 635 / 638
    void ClearDispatchThreadPointers();
    void ClearDirtyConstants();
    void SetAllConstantsDirty();

    // CgsShaderConstants.h:667
    Vector4* UpdateShaderChangeTableAndGetConstantDestination(u32 luIndex);

private:
    // CgsShaderConstants.h:229 / 256 - private helpers (other TUs).
    void FastNonOverlappedVectorMemcpy(Vector4* lpDest, const rw::math::vpu::Vector4* lpSrc, u32 luNumQw);
    void SetMatrixFromAffine(Matrix44& lrDest, const rw::math::vpu::Matrix44Affine& lrAffine);

public:
    // ---- console-faithful member layout (raw byte offsets in the comments) ----

    // CgsShaderConstants.h:645
    ShaderConstantTableElement maConstants[KU_MAX_SHADER_CONSTANTS];                         // offset 0    (600 bytes)
    // CgsShaderConstants.h:646
    const rw::math::vpu::Vector4* mapLatestCopyInDispatchBin[KU_MAX_SHADER_CONSTANTS];       // offset 600  (200 bytes)
    // CgsShaderConstants.h:647
    char* mapConstantNames[KU_MAX_SHADER_CONSTANTS];                                         // offset 800  (200 bytes)
    // CgsShaderConstants.h:649
    ShaderConstantTableElement::ShaderState maConstantsForInternalVariables[KU_MAX_SHADER_CONSTANTS][2]; // offset 1000 (400 bytes)
    // CgsShaderConstants.h:653
    u8 mu8NumUsedConstants;                                                                  // offset 1400
    // CgsShaderConstants.h:663  (3 bytes of pointer-alignment padding after mu8NumUsedConstants)
    CgsGraphics::DispatchBin* mpDispatchBin;                                                  // offset 1404
    // CgsShaderConstants.h:699
    u8 mu8NumDirtyConstants;                                                                  // offset 1408
    // CgsShaderConstants.h:702
    u8 mau8DirtyConstants[KU_MAX_SHADER_CONSTANTS_DIRTY_LIST];                                // offset 1409 (255 bytes)
};

// CgsShaderConstants.h:655 - the single global table instance.
extern ShaderConstantTable sShaderConstantTable;

// CgsShaderConstants.h:805
// On-disk "external" (named) shader-constant block, streamed in with a material/shader
// technique resource. FixUp relocates the three internal pointers (instance data, name
// array, program handles) by the load base and, for the name array, every name pointer.
// Console-faithful: 4 words (count @ 0, instance-data ptr @ 4, name-array ptr @ 8,
// program-handle ptr @ 12).
struct ShaderConstantsExternal
{
    // CgsShaderConstants.h:810 / 814 / 818
    u32 FixUp(const renderengine::ProgramBuffer* lpVertexProgramBuffer);
    u32 FixUp(u8* lpBaseData);
    u32 FixDown(u8* lpBaseData);

    // CgsShaderConstants.h:825
    Vector4* AddToDispatchBinFromStatePointers(CgsGraphics::DispatchObjectContext* lpContext, Vector4* lpDest, Vector4**& lrpStatePointers) const;

    // CgsShaderConstants.h:831 / 837
    void DispatchPixelShaderConstants(Vector4** lppConstants, bool lbShaderHasChanged, const renderengine::ProgramBuffer* lpProgramBuffer) const;
    void DispatchVertexShaderConstants(Vector4** lppConstants, bool lbShaderHasChanged, const renderengine::ProgramBuffer* lpProgramBuffer) const;

    // CgsShaderConstants.h:840
    u32 GetNumConstantInstances() const { return muNumConstantsInstances; }

    // CgsShaderConstants.h:843 / 844 / 845
    bool GetValue(const char* lpName, Vector4& lrOutValue) const;
    bool GetConstant(const char* lpName, renderengine::ProgramVariableHandle*& lrpOutHandle, u32& lrOutIndex) const;
    bool HasShaderConstant(const char* lpName) const;

    // ---- console-faithful member layout ----

    // CgsShaderConstants.h:849
    u32 muNumConstantsInstances;                                  // word 0 (offset 0x00) - element count
    // CgsShaderConstants.h:850
    u32* mppaConstantsInstanceData;                               // word 1 (offset 0x04) - per-instance constant data
    // CgsShaderConstants.h:851
    const char** mppacNames;                                      // word 2 (offset 0x08) - relocated; each name relocated too
    // CgsShaderConstants.h:856
    renderengine::ProgramVariableHandle* mpaProgramStateHandles;  // word 3 (offset 0x0C) - per-instance program handles
};

// CgsShaderConstants.h:861
// On-disk "internal" (engine, hash-keyed) shader-constant block. Same shape as External
// but with one extra pointer array (per-instance sizes), so FixUp relocates four internal
// pointers. Console-faithful: 5 words (count @ 0, sizes @ 4, instance-data @ 8,
// name-hashes @ 12, program-handles @ 16); the element walk is over word 2.
struct ShaderConstantsInternal
{
    // CgsShaderConstants.h:867 / 871 / 875
    u32 FixUp(const renderengine::ProgramBuffer* lpVertexProgramBuffer, const CgsGraphics::ShaderConstantHashTable* lpConstantHashTable);
    u32 FixUp(u8* lpBaseData);
    u32 FixDown(u8* lpBaseData);

    // CgsShaderConstants.h:882 / 889
    void DispatchPixelShaderConstants(s8 li8NumConstants, const ShaderConstantHandle* lpaHandles, bool lbShaderHasChanged, const renderengine::ProgramBuffer* lpProgramBuffer) const;
    void DispatchVertexShaderConstants(s8 li8NumConstants, const ShaderConstantHandle* lpaHandles, bool lbShaderHasChanged, const renderengine::ProgramBuffer* lpProgramBuffer) const;

    // CgsShaderConstants.h:894 / 900
    bool GetValue(const u32& lrConstantNameHash, Vector4& lrOutValue) const;
    bool GetConstant(const u32& lrConstantNameHash, renderengine::ProgramVariableHandle*& lrpOutHandle, u32& lrOutIndex) const;

    // ---- console-faithful member layout ----

    // CgsShaderConstants.h:904
    u32 muNumConstantsInstances;                                  // word 0 (offset 0x00) - element count
    // CgsShaderConstants.h:905
    u32* mpauConstantsInstanceSize;                               // word 1 (offset 0x04) - per-instance sizes
    // CgsShaderConstants.h:906
    u32** mppaConstantsInstanceData;                              // word 2 (offset 0x08) - relocated; each element relocated too
    // CgsShaderConstants.h:907
    u32* mpauNamesHash;                                           // word 3 (offset 0x0C) - per-instance name hashes
    // CgsShaderConstants.h:912
    renderengine::ProgramVariableHandle* mpaProgramStateHandles;  // word 4 (offset 0x10) - per-instance program handles
};

// CgsShaderConstants.h:1044 (DWARF)
// The CPU-side ("material animation") constant block. Unlike External/Internal this is
// name-keyed and carries a back-pointer to the ICPUShader that produced it (nulled by
// FixUp; only ever stored/relocated, never dereferenced by these functions, so a forward
// declaration suffices). FixUp relocates the instance-data and name arrays by the load
// base; GetSizeOf computes the serialised size; GetValue does the linear name lookup;
// ShouldSerialise gates on a non-zero "AnimDuration" constant.
class ICPUShader;   // pointer-only member (nulled by FixUp); homed in the shader SDK.

struct ShaderConstantsCPU
{
    // CgsShaderConstants.h:1006 / 1028 / 1038 / 1043
    u32  FixUp(u8* lpBaseData);
    bool GetValue(const char* lpName, Vector4& lrOutValue) const;
    u32  GetSizeOf(u32 luCurrentSize) const;
    bool ShouldSerialise() const;

    // ---- console-faithful member layout (DWARF CgsShaderConstants.h:1047-1050) ----
    // CgsShaderConstants.h:1047
    ICPUShader*  mpCPUShader;                 // word 0 (offset 0x00) - producing CPU shader (nulled by FixUp)
    // CgsShaderConstants.h:1048
    u32          muNumConstantsInstances;     // word 1 (offset 0x04) - element count
    // CgsShaderConstants.h:1049
    u32**        mppaConstantsInstanceData;   // word 2 (offset 0x08) - per-instance data (relocated; each element relocated too)
    // CgsShaderConstants.h:1050
    const char** mppacNames;                  // word 3 (offset 0x0C) - name array (relocated; each name relocated too)
};
