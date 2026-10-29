#pragma once
#include "types.hpp"
#include "SDKs/Realmc/RealmcSaveInfo.h"
namespace RealmcIface
{
// ARTIST 82B51D60: 32-byte filename, SaveInfo at20, request flags/size/title
// at158/15C/160. The embedded object's copy ctor is 82B51D10.
class SaveReq
{
public:
    // @ 0x82B51D60 -- copy-construct the embedded params from pParamsSource,
    //                 store the three trailing words (a4, a5, *a6), copy the
    //                 32-byte head from pHeadSource, then zero maHead[0x1F].
    SaveReq(const void* pHeadSource,
            const void* pParamsSource,
            uint32_t uField158,
            uint32_t uField15C,
            const uint32_t* pField160Source);

    // Copy-construct: the embedded SaveInfo through its copy constructor, the
    // three trailing words, then the 32-byte head with maHead[0x1F] zeroed last.
    SaveReq(const SaveReq& rOther);

    uint8_t  maHead[0x20];    // +0x000  32-byte head (last byte zeroed)
    SaveInfo mSaveInfo; // +0x020  native SaveInfo
    uint32_t muField158;      // +0x158
    uint32_t muField15C;      // +0x15C
    uint32_t muField160;      // +0x160
};
// sizeof(SaveReq) == 0x164 (356) bytes.

} // namespace RealmcIface
