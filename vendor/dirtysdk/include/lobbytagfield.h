#ifndef DIRTYSDK_LOBBYTAGFIELD_H
#define DIRTYSDK_LOBBYTAGFIELD_H

#include "types.hpp"

// DirtySDK 5.5.3 - core/include/lobbytagfield.h
// The "tagfield" record is the lobby server-interface wire format: a flat text
// record of NAME=value fields. The CGS DirtySock components serialise their
// parameter/info structs into (and parse out of) such a record through these
// helpers. Only the prototypes the reconstructed CGS code actually calls are
// declared here; signatures mirror the DirtySDK sources verbatim (the SDK uses
// fixed-width integer + char* C interfaces).

#ifdef __cplusplus
extern "C" {
#endif

// Locate the value text for a named field within a record (NULL if absent).
const char* TagFieldFind(const char* pRecord, const char* pName);

// Read a string field (writes up to iBuflen bytes into pBuffer, falling back to
// pDefval); returns the number of bytes written.
s32 TagFieldGetString(const char* pData, char* pBuffer, s32 iBuflen, const char* pDefval);

// Read an "epoch" (seconds-since-1970) timestamp field.
u32 TagFieldGetEpoch(const char* pData, u32 uDefval);

// Read a packed structure field (pPat describes the byte pattern).
s32 TagFieldGetStructure(const char* pData, void* pBuf, s32 iLen, const char* pPat);

// Read a raw binary blob field (encoded in the record) into pBuf, up to iLen
// bytes; returns the number of bytes decoded. pData points at the value text
// (typically the result of TagFieldFind/strchr to the field marker). X360
// __fastcall mirrors the DirtySDK C signature (source text, byte destination,
// length).
s32 TagFieldGetBinary(const char* pData, void* pBuf, s32 iLen);

// Append a numeric field to the record.
s32 TagFieldSetNumber(char* pRecord, s32 iReclen, const char* pName, s32 iValue);

// Append a packed structure field to the record.
s32 TagFieldSetStructure(char* pRecord, s32 iReclen, const char* pName,
                         const void* pStruct, s32 iLength, const char* pPattern);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYTAGFIELD_H
