#ifndef DIRTYSDK_LOBBYTAGFIELD_H
#define DIRTYSDK_LOBBYTAGFIELD_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/lobbytagfield.h
// The "tagfield" record is the lobby wire format: a flat text record of NAME=value
// fields separated by a divider character. The CGS DirtySock components serialise
// their parameter/info structs into (and parse them out of) such a record through
// these helpers. Bodies: ../src/lobbytagfield.cpp. Every entry point the game build
// carries is declared here with the DirtySDK C signature (C linkage, global scope).

// Pattern characters understood by TagFieldSetStructure / TagFieldGetStructure.
enum TagFieldPatternE
{
    TAGFIELD_PATTERN_INT8  = 'b',
    TAGFIELD_PATTERN_INT16 = 'w',
    TAGFIELD_PATTERN_INT32 = 'l',
    TAGFIELD_PATTERN_STR   = 's',
};

#ifdef __cplusplus
extern "C" {
#endif

// Copy the record pData into pRecord (at most iReclen-1 chars + terminator); returns
// the number of bytes consumed.
s32 TagFieldDupl(char* pRecord, s32 iReclen, const char* pData);

// Merge the fields of pMerge into pRecord (fields present in pMerge replace the
// record's own). Returns 1 on success, 0 when there is nothing to merge, -1 on overflow.
s32 TagFieldMerge(char* pRecord, s32 iReclen, const char* pMerge);

// Remove the named field from the record. Returns 0, or -1 when the field is absent.
s32 TagFieldDelete(char* pRecord, const char* pName);

// Locate the value text for a named field within a record (NULL if absent). The name
// match is case-insensitive.
const char* TagFieldFind(const char* pRecord, const char* pName);

// TagFieldFind of the field named "<pName><iIdx>".
const char* TagFieldFindIdx(const char* pRecord, const char* pName, s32 iIdx);

// Store a raw (already-encoded) value under pName; a NULL pData deletes the field.
s32 TagFieldSetRaw(char* pRecord, s32 iReclen, const char* pName, const char* pData);

// Copy a raw value (up to the first separator, quotes honoured) into pBuffer, or
// pDefval when pData is NULL. Returns the length copied, -1 when both are NULL.
s32 TagFieldGetRaw(const char* pData, char* pBuffer, s32 iBuflen, const char* pDefval);

// Store a decimal number (a 64-bit value outside +-9999 is stored as "$<hex>").
s32 TagFieldSetNumber(char* pRecord, s32 iReclen, const char* pName, s32 iValue);
s32 TagFieldSetNumber64(char* pRecord, s32 iReclen, const char* pName, s64 iValue);

// Read a number ("-" / "+" / "$" hex prefixes); iDefval when pData is NULL.
s32 TagFieldGetNumber(const char* pData, s32 iDefval);

// Store a flag word as letters (bit 0 = '@', bit 1 = 'A', ...).
s32 TagFieldSetFlags(char* pRecord, s32 iReclen, const char* pName, s32 iValue);

// Store / read a dotted-quad IPv4 address.
s32 TagFieldSetAddress(char* pRecord, s32 iReclen, const char* pName, u32 uAddr);
u32 TagFieldGetAddress(const char* pData, u32 uDefval);

// Store / read a four-character token (escaped with %xx where needed).
s32 TagFieldSetToken(char* pRecord, s32 iReclen, const char* pName, s32 iToken);
s32 TagFieldGetToken(const char* pData, s32 iDefault);

// Store a string field (quoted when it holds a space or comma, %xx escapes); a NULL
// pValue deletes the field.
s32 TagFieldSetString(char* pRecord, s32 iReclen, const char* pName, const char* pValue);

// Read a string field into pBuffer (up to iBuflen bytes incl. terminator), or pDefval
// when pData is NULL. With a NULL pBuffer returns the decoded length only.
s32 TagFieldGetString(const char* pData, char* pBuffer, s32 iBuflen, const char* pDefval);

// Store a binary blob as "$" + hex, or as "^" + 7-bit packed bytes.
s32 TagFieldSetBinary(char* pRecord, s32 iReclen, const char* pName, const void* pValue, s32 iVallen);
s32 TagFieldSetBinary7(char* pRecord, s32 iReclen, const char* pName, const void* pValue, s32 iVallen);

// Decode a "$" or "^" blob into pBuffer (up to iBuflen bytes); with a NULL pBuffer
// returns the decoded size only. -1 when pData is not a blob.
s32 TagFieldGetBinary(const char* pData, void* pBuffer, s32 iBuflen);

// Store / read a packed structure described by a pattern string (see TagFieldPatternE;
// "<n>s" string, "<n>a" skip, "*" repeat, "#name=" comment).
s32 TagFieldSetStructure(char* pRecord, s32 iReclen, const char* pName,
                         const void* pStruct, s32 iLength, const char* pPattern);
s32 TagFieldGetStructure(const char* pData, void* pBuffer, s32 iLength, const char* pPattern);

// Store / read an epoch (seconds since 1970) as "Y.M.D-h:mm:ss". uEpoch 0 stores the
// current time; the getter returns uDefval (or the current time when uDefval is 0)
// for an absent/invalid field.
s32 TagFieldSetEpoch(char* pRecord, s32 iReclen, const char* pName, u32 uEpoch);
u32 TagFieldGetEpoch(const char* pData, u32 uDefval);

// Read a floating-point field (fDefval when pData is NULL).
float TagFieldGetFloat(const char* pData, float fDefval);

// Format a record: whitespace emits the divider, "#" a decimal int, "%s %d %t %a %f %e"
// the matching TagFieldSet* value, "%i" selects a record and "%x" copies a field
// from it, "%r" a raw string. A leading "~" appends to the record, "," continues it.
s32 TagFieldPrintf(char* pRecord, s32 iLength, const char* pFormat, ...);

// Read element iIndex of a delim-separated list field into pBuffer (or pDefval).
s32 TagFieldGetDelim(const char* pData, char* pBuffer, s32 iBuflen, const char* pDefval,
                     s32 iIndex, s32 iDelim);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYTAGFIELD_H
