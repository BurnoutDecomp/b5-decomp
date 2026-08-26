// dirtysdk lobby -- player-name utilities.
// LobbyNameCmp is the SDK's table-driven name comparator (case-insensitive, skips
// spaces/control bytes); ~40 callers image-wide (buddy sort, challenge ownership,
// player lookup). Body: ../src/lobbyname.cpp (@0x82B10050).

#ifndef _lobbyname_h
#define _lobbyname_h

#ifdef __cplusplus
extern "C" {
#endif

// Returns 0 when the two NUL-terminated names denote the same player; otherwise the
// difference of the first non-matching translated characters (strcmp convention).
int LobbyNameCmp(const char* pName1, const char* pName2);

#ifdef __cplusplus
}
#endif

#endif // _lobbyname_h
