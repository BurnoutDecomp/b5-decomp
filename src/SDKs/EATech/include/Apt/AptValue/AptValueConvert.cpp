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
#include "SDKs/EATech/include/Apt/AptNativeHash.h"   // AptNativeHash / AptHashItem (urlEncode walk)
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"   // gAptActionInterpreter.getName (MC name path)
#include "SDKs/EATech/include/Apt/AptMovie.h"   // homes the AptValue_toInteger thunk

struct AptArray;   // AptArray.h -- the array join renderer (AptValue_AptArrayToString thunk)

#include <cstdio>    // snprintf (the "[Type=0x%X]" / "%d" / "%f" / "%p" renders)
#include <cstring>   // strcmp (the "_proto__" / "type" key compares)
#include <cmath>     // fmodf (the float "integral?" test)
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
// Apt_atoff -- HOMED 2026-07-02 (retiring the return-0 stub). The PS3 body
// @0x7E2990 (`_Z9Apt_atoffPKc`) is exactly `(float)_Stod(pStr, 0, 0)` -- the
// CRT string->double parse (strtod semantics: leading whitespace, sign,
// decimal/exponent, hex) rounded to single precision.
float Apt_atoff(const char* pStr)
{
    return static_cast<float>(strtod(pStr, nullptr));
}

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

// ===========================================================================
// AptValue string rendering -- the value-layer renderer the conversion/opcode
// layers call. DECOMPILED from the X360 ARTIST.XEX:
//     AptValue::toString              @0x82AF8FA0
//     AptValue::Append_ToString       @0x82AF9668
//     AptValue::urlEncodeCustomRender @0x82AF9410
//
// The toString jump table (X360 byte_82145338, keyed by meValueType-1, then a
// 0x40-entry computed-goto at loc_82AF903C) and every rodata string/format
// constant were read directly from the decrypted ARTIST XEX rodata; the per-type
// renders below are that table verbatim. EA SDK identifiers kept verbatim.
// ===========================================================================

// ---------------------------------------------------------------------------
// FLAG (wired by the Apt runtime / the loaded movie + the EAStringC subsystem,
// not yet reconstructed here):
//   gAptActionInterpreter  -- the shared AS interpreter (X360 dword_8324E760); the
//       MovieClip-instance-name path renders through its getName() path builder.
//   The EAStringC `true`/`false`/`undefined` runtime string singletons the X360
//       assigns from StaticStringHelperT statics (dword_8324E6C4 / dword_8324E620 /
//       dword_8324E6C8) are modelled here by assigning the literal text -- the same
//       observable string -- rather than referencing the private empty-string
//       sentinel s_EmptyInternalData. (FLAG per use below.)
//   AptArray::toString is private; the X360 calls it directly across the inlined
//       TU. Routed through this FLAG'd thunk (the array join renderer follow-on).
// ---------------------------------------------------------------------------
extern AptActionInterpreter gAptActionInterpreter;                 // dword_8324E760
extern void AptValue_AptArrayToString(AptArray* pArray, EAStringC* pOut,
                                      const char* pSeparator);     // FLAG: AptArray::toString @0x82AED040

// ---------------------------------------------------------------------------
// toString @0x82AF8FA0 -- render this value into *pOut as an ActionScript string.
//
// X360 control flow, verbatim:
//   * an undefined value (mbIsDefined == 0): in SWF v7 (dword_8324E530 == 7) assign
//     the "undefined" string, otherwise assign the empty string;
//   * a defined value dispatches on meValueType through byte_82145338 -- the cases
//     below are the exact table targets (string/bool/int/float/array/the bracketed
//     [Type] placeholders/the CIH recurse/the MovieClip instance-name path).
// ---------------------------------------------------------------------------
void AptValue::toString(EAStringC* pOut) const
{
    AptValue* const pThis = const_cast<AptValue*>(this);

    // X360 0x82AF8FC0: extrwi. r10,r11,1,4 -- the mbIsDefined bit (>>27 & 1).
    if (!getIsDefined())
    {
        // X360 0x82AF8FC8: undefined render. dword_8324E530 is the active SWF
        // version (same global AptGetSwfVersion() reads); v7 spells "undefined".
        if (AptGetSwfVersion() == 7)
            *pOut = "undefined";   // FLAG: console assigns the &dword_8324E6C8 "undefined" singleton
        else
            *pOut = "";            // X360: *a2 = &unk_82F72FF8 (the empty string)
        return;
    }

    const AptVirtualFunctionTable_Indices eType = getVtblIndex();

    switch (eType)
    {
    // --- STRING (loc_82AF903C): a primitive string IS the embedded EAStringC at
    //     +8; a boxed string (0x21) indirects through its linked value at +0x20.
    case AptVFT_StringValue:        // 1
    case AptVFT_StringObject:       // 0x21
    {
        AptString* pStr = (eType == AptVFT_StringValue)
            ? reinterpret_cast<AptString*>(pThis)
            // X360: r30 = *(r30+0x20); r4 = r30+8 -- the boxed string's linked value.
            : reinterpret_cast<AptString*>(*reinterpret_cast<AptValue**>(
                  reinterpret_cast<char*>(pThis) + 0x20));
        *pOut = *pStr->GetInternalString();
        return;
    }

    // --- BOOL (loc_82AF9058): the bool payload byte at +8 -> "true"/"false".
    case AptVFT_Boolean:            // 5
        // FLAG: console assigns the &dword_8324E6C4 / &dword_8324E620 runtime
        // singletons; modelled by the same literal text.
        *pOut = c_boolean()->GetBool() ? "true" : "false";
        return;

    // --- INT (loc_82AF9080): "%d" of the int payload at +8.
    case AptVFT_Integer:            // 7
    {
        char szBuf[136];           // X360 v7[136] scratch
        snprintf(szBuf, sizeof(szBuf), "%d", c_integer()->GetInt());   // unk_8214404C = "%d"
        *pOut = szBuf;
        return;
    }

    // --- FLOAT (loc_82AF9094): integral floats render "%d", others "%f".
    case AptVFT_Float:              // 6
    {
        const float fValue = c_float()->GetFloat();
        char szBuf[136];
        // X360: fmod(value, 1.0) == 0.0 -> the int form ("%d" of (int)value),
        // else the "%f" form.
        if (fmodf(fValue, 1.0f) == 0.0f)
            snprintf(szBuf, sizeof(szBuf), "%d", static_cast<int>(fValue));   // unk_82144050 = "%d"
        else
            snprintf(szBuf, sizeof(szBuf), "%f", static_cast<double>(fValue)); // unk_82144054 = "%f"
        *pOut = szBuf;
        return;
    }

    // --- ARRAY (loc_82AF90F8): join the elements with "," (AptArray::toString).
    case AptVFT_Array:              // 0xE
        AptValue_AptArrayToString(reinterpret_cast<AptArray*>(pThis), pOut, ",");   // unk_82144058 = ","
        return;

    // --- NATIVE FUNCTION (loc_82AF9114): "[native function 0x%p]" of the fn ptr@+0x20.
    case AptVFT_NativeFunction:     // 9
    {
        char szBuf[136];
        snprintf(szBuf, sizeof(szBuf), "[native function 0x%p]",
                 *reinterpret_cast<void**>(reinterpret_cast<char*>(pThis) + 0x20));
        *pOut = szBuf;
        return;
    }

    // --- the bracketed [Type] placeholder renders (loc_82AF9128..loc_82AF91D8) ---
    case AptVFT_ScriptFunction1:    // 0x22
    case AptVFT_ScriptFunction2:    // 0x23
    case AptVFT_ScriptFunctionByteCodeBlock: // 0x24
        *pOut = "[function]";       // aFunction_1
        return;

    case AptVFT_Key:                // 0x10
    case AptVFT_Global:             // 0x11
    case AptVFT_ScriptColour:       // 0x12
    case AptVFT_Object:             // 0x13
    case AptVFT_Mouse:              // 0x17
    case AptVFT_TextFormat:         // 0x1C
    case AptVFT_Stage:              // 0x1F
        *pOut = "[object Object]";  // aObjectObject
        return;

    case AptVFT_Xml:                // 0x19
    case AptVFT_XmlAttributes:      // 0x1A
        *pOut = "[object]";         // aObject
        return;

    case AptVFT_Prototype:          // 0x14
        *pOut = "[object (prototype)]";  // aObjectPrototyp
        return;

    case AptVFT_MovieClip:          // 0x16
        *pOut = "[MovieClip]";      // aMovieclip_0
        return;

    case AptVFT_Register:           // 4
        *pOut = "[Register]";       // aRegister
        return;

    case AptVFT_Lookup:             // 8
        *pOut = "[Lookup]";         // aLookup
        return;

    case AptVFT_Extern:             // 0xB
        *pOut = "[Extern]";         // aExtern
        return;

    case AptVFT_FrameStack:         // 0xA
        *pOut = "[FrameStack]";     // aFramestack
        return;

    case AptVFT_Extension:          // 0x1D
        *pOut = "[Extension]";      // aExtension
        return;

    case AptVFT_GlobalExtension:    // 0x1E
        *pOut = "[GlobalExtension]";// aGlobalextensio
        return;

    // --- CIH recurse (loc_82AF9168, table type 0x15 == AptVFT_Date): the X360
    //     re-enters AptValue::toString on the same receiver (the Date value's
    //     vtable resolves the recurse to its own render).
    case AptVFT_Date:               // 0x15
        // FLAG: the X360 `toString(this, out)` recurse relies on the Date vtable's
        // resolved render to terminate; the resolved-value indirection is the
        // value-formatting follow-on, so render the bracketed placeholder here.
        *pOut = "[object Object]";
        return;

    // --- the embedded-string path (loc_82AF91E8, table type 0x20 == AptVFT_Error):
    //     the embedded EAStringC sits at +0x20.
    case AptVFT_Error:              // 0x20 (32)
        *pOut = *reinterpret_cast<EAStringC*>(reinterpret_cast<char*>(pThis) + 0x20);
        return;

    // --- MovieClip instance name (loc_82AF91F0, table type 0xC ==
    //     AptVFT_CharacterInstHandle): handled below the switch (the X360 reads the
    //     display node at +0x20 and names it via the interpreter path builder).
    default:
        break;
    }

    // X360 loc_82AF91F0 (table type 0xC, AptVFT_CharacterInstHandle): the CIH's
    // display node at +0x20 names itself, with shape/unnamed fallbacks.
    if (eType == AptVFT_CharacterInstHandle)
    {
        AptValue* const pNode =
            *reinterpret_cast<AptValue**>(reinterpret_cast<char*>(pThis) + 0x20);
        if (pNode == 0)
        {
            *pOut = "";   // X360 beq loc_82AF9048 -> assign the empty embedded string
            return;
        }
        // X360 reads *(node+8)>>0x1A (the node's packed type tag) to classify it.
        const int nNodeType = *reinterpret_cast<const int*>(
                                  reinterpret_cast<char*>(pNode) + 8) >> 0x1A;
        const bool lbButtonOrSprite = (nNodeType == 5) || (nNodeType == 9);
        if (!lbButtonOrSprite
            && nNodeType != 4 && nNodeType != 2 && nNodeType != 0xA && nNodeType != 0xF)
        {
            // X360 0x82AF924C: cmpwi 1 -> "Shape Instance" else "No Instance Name".
            *pOut = (nNodeType == 1) ? "Shape Instance" : "No Instance Name";
            return;
        }
        // X360 loc_82AF9270: the named-instance path -> the interpreter builds the
        // node's slash/dot path. FLAG: getName needs the live interpreter + the
        // path-builder follow-on; dormant (empty) until they are up.
        gAptActionInterpreter.getName(pThis, pOut);   // FLAG: path-builder follow-on
        return;
    }

    // X360 default (loc_82AF9280): "[Type=0x%X]" of the raw type tag.
    {
        char szBuf[136];
        snprintf(szBuf, sizeof(szBuf), "[Type=0x%X]", static_cast<int>(eType));   // aType0xX
        *pOut = szBuf;
    }
}

// ---------------------------------------------------------------------------
// Append_ToString @0x82AF9668 -- append this value's string form to *pOut.
//
// X360: a defined string-typed value (meValueType 1, or the boxed 0x21) appends
// its own embedded EAStringC directly (operator+=); any other value is rendered
// via toString into a scratch and that scratch is appended. The scratch fast-path
// (when *pOut is already the empty sentinel) renders straight into *pOut.
// ---------------------------------------------------------------------------
void AptValue::Append_ToString(EAStringC* pOut) const
{
    AptValue* const pThis = const_cast<AptValue*>(this);

    // X360 0x82AF9678: v4 = (type<<25)>>25; the value is an appendable string when
    // it is type 1 or 0x21 AND mbIsDefined ((v2>>27)&1).
    const AptVirtualFunctionTable_Indices eType = getVtblIndex();
    const bool lbAppendableString =
        (eType == AptVFT_StringValue || eType == AptVFT_StringObject) && getIsDefined();

    if (lbAppendableString)
    {
        // X360: a primitive string appends *(this+8); a boxed string indirects
        // through *(this+0x20) first. FLAG (x64 offset fix): those are CONSOLE byte
        // offsets -- the embedded EAStringC lives at the console's +8 only because
        // AptValue is 8 bytes there (4-byte vtbl + 4-byte mnValueData). On x64 the base
        // is 16 bytes (8-byte vtbl + mnValueData + pad), so the string is at +0x10; the
        // literal +8 read {mnValueData, pad} as an EAStringC -> m_pData = pad(0xBAADF00D
        // uninit)<<32 | mnValueData -> AV in operator+=/IncreaseInternalRefCount. Use
        // the named accessor (c_string()->GetInternalString()) which resolves the right
        // member for both the primitive value and the boxed StringObject.
        AptString* const pStr = pThis->c_string();
        if (pStr != nullptr)
            *pOut += *pStr->GetInternalString();
        return;
    }

    // X360 0x82AF96CC: when *pOut already holds the empty-string sentinel, render
    // straight into it; otherwise render into a scratch and append that.
    if (pOut->IsEmpty())
    {
        // X360: *a2 == &unk_82F72FF8 -> toString(this, a2).
        toString(pOut);
        return;
    }

    EAStringC strScratch;       // X360 v8[0] = &unk_82F72FF8 (empty)
    toString(&strScratch);      // X360 AptValue::toString(this, v8)
    *pOut += strScratch;        // X360 EAStringC::operator+=(a2, v8)
}

// ---------------------------------------------------------------------------
// urlEncodeCustomRender @0x82AF9410 -- render this value to its URL-encoded form
// ("key=value&key=value..."). X360: only a defined value that exposes a native
// property hash (vtbl GetNativeHashVirtual, slot +8) produces a body; it walks the
// hash, skips the "_proto__"/"type" magic keys and native-function values, and
// appends "<key-render>=<value-render>&" per remaining entry, trimming the
// trailing "&". Every other value renders the empty string.
// ---------------------------------------------------------------------------
EAStringC AptValue::urlEncodeCustomRender() const
{
    AptValue* const pThis = const_cast<AptValue*>(this);

    EAStringC strOut;           // X360 v29 = &unk_82F72FF8 (empty)

    // X360 0x82AF9430: only a defined value with a property hash has a body.
    AptNativeHash* pHash = getIsDefined()
        ? const_cast<AptValue*>(this)->GetNativeHashVirtual()   // vtbl slot +8
        : 0;
    if (pHash == 0)
        return strOut;          // X360: *a1 = &unk_82F72FF8 (empty), return

    EAStringC strValue;         // X360 v30 = &unk_82F72FF8 (the per-value scratch)

    // X360 loc_82AF947C..loc_82AF95A0: iterate every occupied bucket.
    for (AptHashItem* pItem = pHash->GetFirstItem(); pItem != 0;
         pItem = pHash->GetNextItem(pItem))
    {
        // X360: read the key (the bucket EAStringC). _R3 = item->mKey raw pointer;
        // the lwarx/+0x10000/stwcx. block is EAStringC IncreaseInternalRefCount on a
        // non-empty key (modelled by holding the key by value).
        const EAStringC& strKey = pItem->mKey;
        const char* const pKeyBuf = strKey.GetBuffer();

        // X360 0x82AF94A8: *(key+8)==95 -> the key starts with '_'; only then can it
        // be the "_proto__" magic key. The "_proto__"/"type" keys are skipped.
        bool lbSkip = false;
        if (pKeyBuf[0] == '_')
        {
            // X360 inline strcmp vs "_proto__" then vs "type".
            if (strcmp(pKeyBuf, "_proto__") == 0 || strcmp(pKeyBuf, "type") == 0)
                lbSkip = true;
        }

        if (!lbSkip)
        {
            AptValue* const pValue = pItem->mpValue;   // X360 v19 = v21[1]

            // X360 0x82AF9520: a native-function value is skipped (its type tag is
            // 9 and mbIsDefined).
            const bool lbNativeFn =
                (pValue->getVtblIndex() == AptVFT_NativeFunction) && pValue->getIsDefined();
            if (!lbNativeFn)
            {
                // X360 0x82AF954C: render value into v30, then build "key=value&".
                pValue->toString(&strValue);
                strOut += strValue;     // X360 operator+=(&v29, &v30)
                strOut += "=";          // X360 sub_82AE8520(&v29, "=")
                strOut += strKey;       // X360 operator+=(&v29, &v48 == the key)
                strOut += "&";          // X360 sub_82AE8520(&v29, "&")
            }
        }
        // X360 0x82AF958C: DecreaseInternalRefCount(key) -- the held key copy drops
        // when strKey goes out of scope each iteration.
    }

    // X360 0x82AF95A4: trim the trailing "&".
    strOut.EndWithRemove("&");
    return strOut;              // X360 *a1 = v29 (with the IncreaseInternalRefCount)
}
