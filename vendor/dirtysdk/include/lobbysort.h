#ifndef DIRTYSDK_LOBBYSORT_H
#define DIRTYSDK_LOBBYSORT_H

#include "types.hpp"

// DirtySDK 5.5.3 - lobby sort utility (core/source/util/lobbysort.c).
// A stable natural merge sort over an array of nmemb fixed-size elements. The
// compare callback receives pointers to two elements (plus the caller's refptr)
// and returns > 0 when the first sorts after the second.
// Body: ../src/lobbysort.cpp.

typedef s32 (LobbySortCompareT)(void* pRefPtr, const void* pElem1, const void* pElem2);

#ifdef __cplusplus
extern "C" {
#endif

void LobbyMSort(void* pRefPtr, void* pBase, s32 iNumMemb, s32 iSize, LobbySortCompareT* pCompare);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_LOBBYSORT_H
