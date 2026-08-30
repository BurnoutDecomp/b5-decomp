#pragma once

// ===========================================================================
// Csis::ClassHandle -- a handle into the "Csis" class/interface registry, a
// vendor library boundary in BURNOUT_X360_ARTIST.XEX (the Csis::*, CsisDef::*
// family used by the AEMS audio voice/factory code). This header is the
// canonical OWNING home for:
//
//     Csis::ClassHandle::ClassHandle  @ 0x82B0F150   (default ctor)
//     Csis::ClassHandle::SetFast      @ 0x82B0FB20   (bind to an InterfaceId)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Csis` is a vendor
// library boundary, so its identifiers (Csis, ClassHandle, SetFast, SetHandle,
// InterfaceId, Result, CsisDef::FunctionDesc) are preserved verbatim per the
// naming convention.
//
// LAYOUT (from asm):
//   The ctor (`li r11,-3 ; stw r11,4(r3)`) writes the sentinel -3 to the 32-bit
//   word at byte offset +4 and touches nothing else, so:
//     +0x00  (handle payload word; left to the binding machinery -- the ctor
//             does not initialise it, SetFast/SetHandle populate it)
//     +0x04  miIndex -- the registry slot index; -3 is the "unbound/invalid"
//             sentinel the default ctor stamps. (Total object size 8 bytes; the
//             24/12 size hints SetFast passes to SetHandle confirm an 8-byte
//             handle plus the 24-byte InterfaceId / 12-byte FunctionDesc records
//             it indexes.)
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * Csis::SetHandle<ClassHandle, InterfaceId const, CsisDef::FunctionDesc> --
//     the templated binding free function SetFast tail-calls. Its body lives in
//     another (vendor) Csis TU; only this one explicit specialisation is reached
//     from this TU, declared below for compile/link. The three trailing integer
//     arguments are (kind=0, interfaceRecordSize=24, functionDescSize=12).
//   * Csis::InterfaceId / CsisDef::FunctionDesc -- vendor registry record types
//     SetHandle indexes. Their full layout is not defined in this TU; forward
//     declarations are sufficient because SetFast only forms a typed call.
// ===========================================================================

#include "types.hpp" // s32

namespace Csis
{

// Result of a handle-binding operation (mangled `W4Result@Csis`). Modelled as an
// enum-shaped status; the concrete enumerators live in the vendor Csis headers.
// Only the type identity is load-bearing here (SetFast forwards SetHandle's
// return verbatim), so a single placeholder enumerator stands in.
enum class Result : int
{
    E_OK = 0
};

// Native-64 interface selector materialised by the AEMS relocation table.
// Xbox One sub_14096C510 builds exactly {name pointer, system id, interface id}.
struct InterfaceId
{
    const char* mpName;
    u16 muSystemId;
    u16 muInterfaceId;
};

} // namespace Csis

namespace CsisDef
{
struct FunctionDesc;
} // namespace CsisDef

namespace Csis
{

// ---------------------------------------------------------------------------
// Csis::SetHandle -- the vendor templated handle-binding free function. Defined
// in another Csis TU; this is the single explicit specialisation SetFast uses.
// Signature reconstructed from the demangled tail-call target:
//   Result Csis::SetHandle<ClassHandle, InterfaceId const, CsisDef::FunctionDesc>(
//       ClassHandle* pHandle, const InterfaceId* pId, int kind,
//       int nInterfaceRecordSize, int nFunctionDescSize);
// (FLAGGED vendor extern -- declared, not defined, in this TU.)
// ---------------------------------------------------------------------------
class ClassHandle;

template <class THandle, class TId, class TFunctionDesc>
Result SetHandle(THandle* pHandle, const TId* pId, int kind,
                 int nInterfaceRecordSize, int nFunctionDescSize);

// ---------------------------------------------------------------------------
// Csis::ClassHandle
// ---------------------------------------------------------------------------
class ClassHandle
{
public:
    // @ 0x82B0F150 -- default ctor: stamp the "unbound" sentinel into miIndex.
    ClassHandle();

    // @ 0x82B0FB20 -- bind this handle to an InterfaceId via SetHandle, tail-call
    // forwarding the binding-record sizes (kind=0, idSize=24, fnDescSize=12).
    Result SetFast(const InterfaceId* pId);

    // Native-64 handle: descriptor pointer + 32-bit generation token.
    union { void* mpDescriptor; uintptr_t miPayload; };
    s32 miIndex;
    u32 muPadding;
};

static_assert(sizeof(ClassHandle) == 0x10, "native CSIS class handle");

} // namespace Csis
