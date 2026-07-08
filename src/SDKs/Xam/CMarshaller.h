#ifndef SDKS_XAM_CMARSHALLER_H
#define SDKS_XAM_CMARSHALLER_H

#include "types.hpp"

#include "SDKs/Xam/CArgumentList.h"
#include "SDKs/Xam/CBaseEndianBuffer.h"
#include "SDKs/Xam/CConformanceList.h"
#include "SDKs/Xam/CSchemaAccess.h"
#include "SDKs/Xam/CSchemaData.h"

// ===========================================================================
// SDKs/Xam/CMarshaller.h
//
// CMarshaller -- the Xbox 360 XAM (Xbox Account Manager) RPC argument
// marshaller. It serialises a typed argument list (CArgumentList) into a raw
// wire buffer by walking the call's schema descriptor stream: for each schema
// field it reads the field descriptor, resolves any run-time conformance
// (array length / union selector) through a CConformanceList, pulls the source
// value from the bound argument list, and writes it (endian-swapped) into an
// output CBaseEndianBuffer. XOnlineMarshalArguments drives it (Initialize then
// Marshal). Sibling of CArgumentList / CSchemaData / CSchemaAccess /
// CConformanceList.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. There is no reference source and
// no DWARF for this TU; the SHAPE below is grounded purely on the X360 asm of
// the four functions. These are Microsoft-XDK-style C-prefixed marshalling
// classes, so the identifiers (CMarshaller, Initialize, Marshal, ...) are kept
// verbatim per the naming convention's external/platform-API carve-out.
//
// LAYOUT (X360-attested field offsets; the embedded objects' leading pointers
// widen on the 64-bit host, so the absolute offsets below are the X360 form and
// are NOT assertable on the PC target):
//   +0x000  mpSchema              CSchemaData*      bound schema (Initialize: stw r4,0)
//   +0x004  mpArgList             CArgumentList*    bound argument list (stw r7,4);
//                                                   GetArgumentFromSchema reads its
//                                                   muCount (+0x200) and element slots
//   +0x008  mBuffer               CBaseEndianBuffer output wire buffer (0x14 bytes)
//   +0x01C  mSchemaAccess         CSchemaAccess     schema read cursor (0x18 bytes)
//                                                   -- its bound schema lives at +0x030
//   +0x034  (opaque 4-byte gap on X360; never accessed)
//   +0x038  mConformance          CConformanceList  64-slot conformance table (0x400 bytes)
//   +0x438  muDefaultConformance  u32               default conformance count seeded into
//                                                   each GetConformingScopeAndValue call
// ===========================================================================

class CMarshaller
{
public:
    // @ 0x8297F4D8 -- bind the marshaller to a schema, an argument list and an
    // output span. Records the schema/argument-list pointers, binds the schema
    // read cursor (CSchemaAccess::BindToSchema, first (size,offset) pair) and the
    // output buffer (CBaseEndianBuffer::Bind), then zeroes the 64-slot conformance
    // table. On a schema-bind failure it resets the object and returns the failing
    // status. luBufferSize is passed as both the context and the size argument of
    // the buffer Bind (the X360 hands the same register for r5 and r6).
    s32 Initialize(CSchemaData* lpSchema, u32 luSchemaId, u32 luDefaultConformance,
                   CArgumentList* lpArgList, u8* lpBuffer, u32 luBufferSize,
                   u32 lbSwapEndian);

    // @ 0x8297FDE8 -- marshal the whole argument list: RecursiveMarshal from the
    // schema root (depth 0, no inbound data pointer).
    s32 Marshal();

    // @ 0x8297F620 -- recursively marshal one schema scope at `luDepth` (bounded to
    // 0x10). `lppData` is the in/out source-data cursor: on entry the source pointer
    // for this scope, on exit advanced past the scope's data. Walks each field of the
    // scope, resolving conformance, unions and nested scopes, writing each leaf value
    // into the output buffer.
    s32 RecursiveMarshal(u32 luDepth, u8** lppData);

    // @ 0x8297F598 -- fetch the argument slot the schema's next argument-info
    // descriptor names. Reads the 1-byte descriptor (CSchemaAccess::GetArgumentInfo);
    // its bit 0x20 is handed back through `lpIndirect` (when non-null) and its low 5
    // bits index the bound argument list. Hands back the slot in *lppSlot (null, and
    // E_FAIL, when the index is past the list). `luArgIndex` must be 0 or 1.
    s32 GetArgumentFromSchema(u32 luArgIndex, CArgumentList::Element** lppSlot,
                              u32* lpIndirect);

private:
    CSchemaData*      mpSchema;             // +0x000  bound schema
    CArgumentList*    mpArgList;            // +0x004  bound argument list
    CBaseEndianBuffer mBuffer;             // +0x008  output wire buffer
    CSchemaAccess     mSchemaAccess;       // +0x01C  schema read cursor
    // NOTE: the X360 layout has an unaccessed 4-byte gap at +0x034 between the schema
    // cursor and the conformance table; it is not modelled (never read or written).
    CConformanceList  mConformance;        // +0x038  conformance slot table
    u32               muDefaultConformance; // +0x438  default conformance count
};

#endif // SDKS_XAM_CMARSHALLER_H
