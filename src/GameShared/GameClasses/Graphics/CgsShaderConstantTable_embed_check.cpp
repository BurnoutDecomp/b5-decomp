// Embed-check / compile tripwire for the ShaderConstantTable runtime methods bodied in
// CgsShaderConstantTable.cpp. Pins the X360 byte layout by offset and forces every body to
// instantiate. Not a unit test -- a compile/link guard for this TU.
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include <cstddef>

namespace
{
    // Layout pins. The X360 byte offsets in the header (element 12B, names @800,
    // used-count @0x578, dirty list @0x581, latest-copy @600) are the CONSOLE 32-bit-pointer
    // layout; on the host x64 the pointer-array members widen (8-byte pointers), so only the
    // narrow, pointer-free field offsets within ShaderConstantTableElement are width-stable
    // and asserted here. The functions address all members BY NAME, so console-faithful
    // behaviour does not depend on the host sizeof matching the console.
    static_assert(offsetof(ShaderConstantTableElement, mu16SizeOfArrayInQw) == 0x00, "mu16SizeOfArrayInQw @0x00");
    static_assert(offsetof(ShaderConstantTableElement, mu8SizeInBytes) == 0x02, "mu8SizeInBytes @0x02");
    static_assert(offsetof(ShaderConstantTableElement, mu8NumEntries) == 0x03, "mu8NumEntries @0x03");
    static_assert(offsetof(ShaderConstantTable, maConstants) == 0, "maConstants @0");
    // Member ordering (used-count precedes the dispatch bin precedes the dirty count/list).
    static_assert(offsetof(ShaderConstantTable, mu8NumUsedConstants) < offsetof(ShaderConstantTable, mpDispatchBin),
                  "mu8NumUsedConstants precedes mpDispatchBin");
    static_assert(offsetof(ShaderConstantTable, mpDispatchBin) < offsetof(ShaderConstantTable, mu8NumDirtyConstants),
                  "mpDispatchBin precedes mu8NumDirtyConstants");
    static_assert(offsetof(ShaderConstantTable, mu8NumDirtyConstants) < offsetof(ShaderConstantTable, mau8DirtyConstants),
                  "mu8NumDirtyConstants precedes mau8DirtyConstants");

    // Force the four bodies to be referenced (taking their addresses instantiates them).
    void (ShaderConstantTable::*pAddArray)(u32, const char*, u8, u8) =
        &ShaderConstantTable::AddShaderConstantArray;
    void (ShaderConstantTable::*pSetArray)(u32, const rw::math::vpu::Vector4*) =
        &ShaderConstantTable::SetShaderConstantArrayData;
    Vector4* (ShaderConstantTable::*pUpdate)(u32) =
        &ShaderConstantTable::UpdateShaderChangeTableAndGetConstantDestination;
}

int g_CgsShaderConstantTableEmbedCheck =
    (pAddArray != nullptr) + (pSetArray != nullptr) + (pUpdate != nullptr);
