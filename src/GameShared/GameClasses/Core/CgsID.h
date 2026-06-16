#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"   // committed home of `typedef u64 CgsID`

// CgsID: a 64-bit packed identity built from a base-40 alphabet so that up to 12
// printable characters fit in one 64-bit word (CgsID.h:195 -> committed in
// BrnCommonTypes.h). The alphabet (CgsID.cpp:37 `kacCgsIDCharacters`) maps:
//   index 0      -> ' ' (space / unused slot)
//   index 1      -> '-'
//   index 2      -> '/'
//   index 3..12  -> '0'..'9'
//   index 13..38 -> 'A'..'Z'   (lowercase 'a'..'z' compress to the SAME indices;
//                                un-compression always reproduces uppercase)
//   index 39     -> '_'
// The most-significant base-40 digit is the first character; trailing unused slots
// decode to spaces.

// CgsID.h:34 (DWARF) - buffer length for the printable form: 12 chars + NUL.
const s32 KI_CGSID_STRING_LEN = 13;

// --- CgsID free functions (CgsID.cpp) ---
// Only the three boot-trace functions are reconstructed; the rest of the CgsID.cpp
// helpers are declared-only here so the header is the coherent shared slice.

// @ 0x82815A20 - compress a NUL-terminated string (<= 12 chars) into a CgsID.
CgsID CgsIDCompress(const char* lpcString);

// @ 0x82815C78 - expand a CgsID back into KI_CGSID_STRING_LEN chars (NUL-terminated,
// space-padded on the right). lpcString must hold at least KI_CGSID_STRING_LEN bytes.
void CgsIDUnCompress(CgsID lID, char* lpcString);

// @ 0x82815D30 - un-compress then strip the trailing space padding in place.
void CgsIDConvertToString(CgsID lID, char* lpcString);

// Declared-only (other CgsID.cpp helpers, not part of this boot-trace TU):
char  CgsIDUnConvert(CgsID lCompressed);
CgsID CgsIDMaskCharacters(CgsID lID, s32 liStartIndex, s32 liEndIndex);
CgsID CgsIDScroll(CgsID lID, s32 liAmount);
CgsID CgsIDConcatenate(CgsID lID1, CgsID lID2);
s32   CgsIDGetLength(CgsID lID);
s32   CgsIDExtractTrailingNumber(CgsID lID);
