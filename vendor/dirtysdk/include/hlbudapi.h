#ifndef DIRTYSDK_HLBUDAPI_H
#define DIRTYSDK_HLBUDAPI_H

#include "types.hpp"

// DirtySDK 5.5.3 - buddy/include/hlbudapi.h
// High-level buddy API: the platform friends list as a sorted list of buddy records with
// presence / relationship flags, change and presence callbacks, and chat / invite messages.
// The CGS buddy manager is the only caller. PC body: ../src/pc/hlbudapipc.cpp.

// Opaque buddy API handle.
struct HLBApiRefT;

// One buddy record. The accessors below and the game's invite checks read these fields;
// the record continues past uFlags with data nothing outside the module reads.
struct HLBBudT
{
    u64  uXuid;       // +0x00  platform user id
    char strName[16]; // +0x08  display name
    u32  uFlags;      // +0x18  presence / relationship / invite flag word (HLB_BUDFLAG_*)
};

// HLBBudT::uFlags bits.
#define HLB_BUDFLAG_ONLINE        (0x00000001)
#define HLB_BUDFLAG_PASSIVE       (0x00000002)   // HLBBudGetState answers 5
#define HLB_BUDFLAG_JOINABLE      (0x00000010)
#define HLB_BUDFLAG_NOT_REAL      (0xC0000060)   // any of these: a temporary / pending record
#define HLB_BUDFLAG_SAME_TITLE    (0x00010000)   // title field value for which HLBBudGetState answers 2
#define HLB_BUDFLAG_TITLE_MASK    (0x000F0000)
#define HLB_BUDFLAG_INVITE_SENT   (0x04000000)   // the local user invited this buddy
#define HLB_BUDFLAG_INVITE_RECV   (0x08000000)   // this buddy invited the local user

// Results.
#define HLB_ERROR_NONE            (0)
#define HLB_ERROR_ALREADY_SET     (-5)   // a callback of that kind is already installed
#define HLB_ERROR_UNSUPPORTED     (-6)   // not available on this platform

#ifdef __cplusplus
extern "C" {
#endif

// ---- API lifecycle ----
HLBApiRefT* HLBApiCreate(void* pConfigA, void* pConfigB, s32 iMemGroup);
void        HLBApiDestroy(HLBApiRefT* pApi);
void        HLBApiDisconnect(HLBApiRefT* pApi);
void        HLBApiUpdate(HLBApiRefT* pApi);
void        HLBApiSetUserIndex(HLBApiRefT* pApi, s32 iUserIndex);

// ---- callbacks (HLB_ERROR_ALREADY_SET when one is installed and pCallback is not NULL) ----
s32         HLBApiRegisterBuddyChangeCallback(HLBApiRefT* pApi, void* pCallback, void* pUserData);
s32         HLBApiRegisterBuddyPresenceCallback(HLBApiRefT* pApi, void* pCallback, void* pUserData);

// Install the buddy-list sort comparator.
void        HLBApiSetSortFunction(HLBApiRefT* pApi, void* pSortCallback);

// ---- list ----
s32         HLBListGetBuddyCount(HLBApiRefT* pApi);
HLBBudT*    HLBListGetBuddyByIndex(HLBApiRefT* pApi, s32 iIndex);
HLBBudT*    HLBListGetBuddyByName(HLBApiRefT* pApi, const char* pName);
void        HLBListSort(HLBApiRefT* pApi);

// Chat / invite message to a buddy (every message entry point shares this shape).
s32         HLBListSendChatMsg(HLBApiRefT* pApi, const void* pName, s32 iPayload, void* pCallback,
                               void* pUserData);

// ---- one buddy ----
const char* HLBBudGetName(HLBBudT* pBuddy);
u64         HLBBudGetXenonXUID(HLBBudT* pBuddy);
s32         HLBBudIsRealBuddy(HLBBudT* pBuddy);
s32         HLBBudIsJoinable(HLBBudT* pBuddy);
s32         HLBBudIsBlocked(HLBBudT* pBuddy);
s32         HLBBudGetState(HLBBudT* pBuddy);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_HLBUDAPI_H
