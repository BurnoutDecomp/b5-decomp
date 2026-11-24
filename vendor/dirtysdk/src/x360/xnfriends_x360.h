#ifndef DIRTYSDK_X360_XNFRIENDS_X360_H
#define DIRTYSDK_X360_XNFRIENDS_X360_H

#include "types.hpp"
#include <xonline.h>

// Private X360 XnFriends contract shared by the provider and HLB adapter.
typedef struct XnFriendsRef XnFriendsRef;   // DirtySDK is C
struct XnFriendsRef;
typedef void (*XnFriendsChangeCallback)(XnFriendsRef* pFriendRef, s32 iChangeCount,
                                        s32 iInitialised, const void* pChanges,
                                        void* pUserData);

#ifdef __cplusplus
extern "C" {
#endif
XnFriendsRef* XnFriendsCreate(s32 iArg, void* pArg);
void XnFriendsDestroy(XnFriendsRef* pRef);
s32 XnFriendsConnect(XnFriendsRef* pRef, s32 iArg,
                     XnFriendsChangeCallback pCallback, void* pUserData);
s32 XnFriendsDisconnect(XnFriendsRef* pRef);
void XnFriendsUpdate(XnFriendsRef* pRef);
s32 XnFriendsSetUserIndex(XnFriendsRef* pRef, s32 iUserIndex);
XONLINE_FRIEND* XnFriendsGetFriend(XnFriendsRef* pRef, s32 iIndex);
XONLINE_FRIEND* XnFriendsGetFriendByName(XnFriendsRef* pRef, const char* pName);
#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_X360_XNFRIENDS_X360_H
