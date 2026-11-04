#ifndef DIRTYSDK_PROTOFILTER_H
#define DIRTYSDK_PROTOFILTER_H

#include "types.hpp"

// DirtySDK 5.5.3 - proto/include/protofilter.h
// X360 string verification through the XDK XStringVerify service. A request is
// asynchronous: ProtoFilterRequest returns -1 and writes a slot index, Result
// returns -1 while that slot is pending, then 0/1 for the XDK result. Two
// requests can be in flight at once.
struct ProtoFilterRefT;

#ifdef __cplusplus
extern "C" {
#endif

ProtoFilterRefT* ProtoFilterCreate(void);
void ProtoFilterDestroy(ProtoFilterRefT* pFilter);

// Submit pText for online string verification. Returns -1 with iRequest set to
// the pending slot, or -2 when no slot accepted the request.
s32 ProtoFilterRequest(ProtoFilterRefT* pFilter, const char* pText, s32* pRequest);

// Cancel one slot, or all slots with -1. Result polls a slot: -1 pending, -3
// invalid/inactive, 0 clear, or 1 flagged.
s32 ProtoFilterRequestCancel(ProtoFilterRefT* pFilter, s32 iRequest);
s32 ProtoFilterResult(ProtoFilterRefT* pFilter, u32 uRequest);

// Apply the Lobby text policy to pText: action 1 masks non-space characters;
// action 2 replaces the text with the configured profanity string or "@/&!".
s32 ProtoFilterApplyFilter(ProtoFilterRefT* pFilter, s32 iAction, char* pText);

// The native provider accepts the 'pstr' select and copies pValue into its
// 32-byte replacement string. Other selects return -1.
s32 ProtoFilterControl(ProtoFilterRefT* pFilter, s32 iSelect, s32 iValue,
                       s32 iValue2, const char* pValue);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_PROTOFILTER_H
