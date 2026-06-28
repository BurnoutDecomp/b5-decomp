// ===========================================================================
// EATech Apt -- AptValue value conversions + the c_string cast.
//
// DECOMPILED from the PS3 EXTERNAL ELF (the X360 ARTIST build inlined these, so
// it has no standalone bodies):
//     AptValue::c_string   @0x7E4E18   (full pseudocode)
//     AptValue::toBool     @0x7EF024   (full pseudocode + asm)
//     AptValue::toInteger  @0x7F2CF4   (asm; no pseudocode)
//     AptValue::toFloat    @0x7E8FE4   (asm; no pseudocode)
//
// All four dispatch on the value type tag (meValueType, the low 7 bits of the
// bitfield word -- here read endian-safely via getVtblIndex() rather than the
// console's `*((_DWORD*)this + 1) & 0x7F`). They read the concrete payload for
// bool/int/float values, parse the string for string values, and treat every
// other value as truthy/1 unless it is the global `undefined` singleton.
//
// toBool is a verbatim decompile (the export gives its complete body). For
// toInteger/toFloat the export TRUNCATES at the (IDA-flagged "invalid") jump
// table, so the four per-case handler bodies past it are not in the dump; what IS
// in the dump and kept verbatim is the structure -- the mbIsDefined guard, the
// exact jump-table case->type mapping (only types 1/0x21/5/6/7 have handlers; all
// else default) and the default body (`this != gpUndefinedValue ? 1 : 0`). The
// per-case bodies are reconstructed from toBool's verbatim value access (the
// payload offsets are identical across the three) and the standard primitive
// conversion; each such spot is FLAG'd below.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"
#include "SDKs/EATech/include/Apt/AptMovie.h"   // homes the AptValue_toInteger thunk

#include <cstdlib>   // strtol

// ---------------------------------------------------------------------------
// FLAG (wired by the Apt runtime / the loaded movie, not yet reconstructed):
//   gpUndefinedValue   -- the shared `undefined` value singleton (declared by
//                         AptArray.h; the conversions compare identity against it).
//   AptGetSwfVersion() -- the active movie's SWF version (string->bool differs for
//                         version 7); symbol _Z16AptGetSwfVersionv @0x7E29C0.
//   Apt_atoff()        -- Apt's string->float parse; symbol _Z9Apt_atoffPKc
//                         @0x7E2990. Bodied with the Apt number helpers (follow-on).
// These are declared (not defined) here; the per-TU gate is compile-only, and the
// Apt subsystem is wired into the game at AptInit (a later phase).
// ---------------------------------------------------------------------------
extern AptValue*    gpUndefinedValue;
extern unsigned int AptGetSwfVersion();
extern float        Apt_atoff(const char* pStr);

// ---------------------------------------------------------------------------
// c_string @0x7E4E18 -- as the AptString for a primitive string (type 1, the value
// IS the AptString), else the boxed-string's linked AptString pointer.
// ---------------------------------------------------------------------------
AptString* AptValue::c_string() const
{
    if (getVtblIndex() == AptVFT_StringValue)
        return reinterpret_cast<AptString*>(const_cast<AptValue*>(this));

    // FLAG: the console returns the linked AptString* a boxed-string value
    // (AptStringObject, type 0x21) keeps at a type-specific slot (PS3 +0x20). That
    // offset is layout-specific and its x64 owner (AptStringObject) is not
    // reconstructed yet, so return null until then rather than bake in the 32-bit
    // offset. No StringObject values exist before the VM/AptInit are live, so this
    // branch is dormant.
    return nullptr;
}

// ---------------------------------------------------------------------------
// toBool @0x7EF024 -- verbatim decompile.
// ---------------------------------------------------------------------------
bool AptValue::toBool() const
{
    const AptVirtualFunctionTable_Indices eType = getVtblIndex();

    if (eType == AptVFT_StringValue || eType == AptVFT_StringObject)
    {
        AptString* pStr = c_string();
        if (pStr)
        {
            const EAStringC& s = *pStr->GetInternalString();

            // SWF v7: empty string is false, any non-empty string is true.
            if (AptGetSwfVersion() == 7)
                return !s.IsEmpty();

            // Otherwise numeric truthiness: a "0x.." literal parses as base-16 int,
            // everything else as a float; non-zero -> true.
            const char* buf = s.GetBuffer();
            if (s.GetLength() > 2 && buf[0] == '0' && buf[1] == 'x')
                return strtol(buf, 0, 16) != 0;
            return Apt_atoff(buf) != 0.0f;
        }
        // dormant boxed-string fall-through (see c_string FLAG): treat as object.
    }

    switch (eType)
    {
    case AptVFT_Boolean: return c_boolean()->GetBool();
    case AptVFT_Float:   return c_float()->GetFloat() != 0.0f;
    case AptVFT_Integer: return c_integer()->GetInt() != 0;
    default:             return this != gpUndefinedValue;
    }
}

// ---------------------------------------------------------------------------
// toInteger @0x7F2CF4 -- structure verbatim; per-case bodies reconstructed
// (export truncates past the jump table -- see file header).
// ---------------------------------------------------------------------------
int AptValue::toInteger() const
{
    // Verbatim: an undefined value is 0 regardless of type.
    if (!getIsDefined())
        return 0;

    const AptVirtualFunctionTable_Indices eType = getVtblIndex();

    if (eType == AptVFT_StringValue || eType == AptVFT_StringObject)
    {
        AptString* pStr = c_string();
        if (pStr)
        {
            // FLAG: string case body past the export's jump-table truncation;
            // reconstructed from toBool's verbatim string parse, yielding the
            // integer value (hex "0x.." -> base-16, else the numeric value).
            const EAStringC& s = *pStr->GetInternalString();
            const char* buf = s.GetBuffer();
            if (s.GetLength() > 2 && buf[0] == '0' && buf[1] == 'x')
                return static_cast<int>(strtol(buf, 0, 16));
            return static_cast<int>(Apt_atoff(buf));
        }
    }

    switch (eType)
    {
    // FLAG: bodies past the truncation; the payload access matches toBool verbatim.
    case AptVFT_Boolean: return c_boolean()->GetBool() ? 1 : 0;
    case AptVFT_Float:   return static_cast<int>(c_float()->GetFloat());
    case AptVFT_Integer: return c_integer()->GetInt();
    // Verbatim default: 1 for any other (object-like) value, 0 only for undefined.
    default:             return (this != gpUndefinedValue) ? 1 : 0;
    }
}

// ---------------------------------------------------------------------------
// toFloat @0x7E8FE4 -- structure verbatim; per-case bodies reconstructed
// (export truncates past the jump table -- see file header).
// ---------------------------------------------------------------------------
float AptValue::toFloat() const
{
    // Verbatim: an undefined value is 0.0 regardless of type.
    if (!getIsDefined())
        return 0.0f;

    const AptVirtualFunctionTable_Indices eType = getVtblIndex();

    if (eType == AptVFT_StringValue || eType == AptVFT_StringObject)
    {
        AptString* pStr = c_string();
        if (pStr)
        {
            // FLAG: string case body past the export's jump-table truncation;
            // reconstructed from toBool's verbatim string parse, yielding the float.
            const EAStringC& s = *pStr->GetInternalString();
            const char* buf = s.GetBuffer();
            if (s.GetLength() > 2 && buf[0] == '0' && buf[1] == 'x')
                return static_cast<float>(strtol(buf, 0, 16));
            return Apt_atoff(buf);
        }
    }

    switch (eType)
    {
    // FLAG: bodies past the truncation; the payload access matches toBool verbatim.
    case AptVFT_Boolean: return c_boolean()->GetBool() ? 1.0f : 0.0f;
    case AptVFT_Float:   return c_float()->GetFloat();
    case AptVFT_Integer: return static_cast<float>(c_integer()->GetInt());
    // Verbatim default: 1.0 for any other (object-like) value, 0.0 for undefined.
    default:             return (this != gpUndefinedValue) ? 1.0f : 0.0f;
    }
}

// ---------------------------------------------------------------------------
// AptValue_toInteger -- homes the thunk AptMovie::labelToFrame used while the
// AptValue conversion layer did not yet exist. Now a thin forward to toInteger().
// ---------------------------------------------------------------------------
int AptValue_toInteger(AptValue* pValue)
{
    return pValue ? pValue->toInteger() : 0;
}
