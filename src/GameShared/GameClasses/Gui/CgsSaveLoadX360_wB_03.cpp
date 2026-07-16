// CgsGui::SaveLoadSystem - wave-B partfile (group 4: the mugshot buffer family).
// Reconstructed from BURNOUT_X360_ARTIST.XEX per the keystone spec
// (scratchpad/waveB/class_CgsGui__SaveLoadSystem.spec.md, rows 14-16). The class
// layout is frozen in CgsSaveLoadX360.h; this file only bodies functions.
//
// Functions in scope for this partfile:
//   GetMugshotBufferFromImageId      @ 0x8284C910
//   CopyImageToBuffer                @ 0x828522D0
//   CreateRealmcMugshotLoadEntryInfo @ 0x828523D0
//
// The 0x214/0x218 asserts speak of miNumberOfMugshotTypes/miNumberOfMugshotsPerType
// but the committed members are miImageWidth/miImageHeight -- code uses the committed
// member names; the assert STRINGS are kept verbatim from the X360 rodata.

// Layout home FIRST (fwd-declares ::SystemUserProfile / ::LanguageManager at global
// scope; any other CgsGui header must follow it).
#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"      // CGS_ASSERT
#include "SDKs/Realmc/RealmcMemcardInterface.h"
#include "SDKs/Realmc/RealmcMemcardInterfaceImpl.h"
#include "SDKs/Realmc/RealmcLoadEntryInfo.h"            // RealmcIface::LoadEntryInfo
#include "SDKs/Realmc/RealmcDataBuffer.h"               // RealmcIface::DataBuffer
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"

#include <cstring>   // std::memcpy
#include <cstdint>   // uintptr_t

namespace CgsGui
{
    // X360 0x8284C910. Resolve a mugshot image id to its byte address inside the
    // shared mugshot buffer: liImageId decodes to (type, index) as id/1000, id%1000;
    // the flat slot is miImageHeight * type + index; the address is
    // mpMugshotBufferData + miExtraFilesSizeBytes * slot. Fires the four X360 range
    // asserts on the way (member names committed, assert strings verbatim).
    void* SaveLoadSystem::GetMugshotBufferFromImageId(s32 liImageId)
    {
        const s32 liType  = liImageId / 1000;
        s32       liIndex = liImageId % 1000;

        CGS_ASSERT(liType < miImageWidth, "liType < miNumberOfMugshotTypes");
        CGS_ASSERT(liIndex < miImageHeight, "liIndex < miNumberOfMugshotsPerType");

        // The X360 reuses the index register as the flat slot from here on.
        liIndex = miImageHeight * liType + liIndex;

        CGS_ASSERT(liIndex >= 0, "0 <= liIndex");
        CGS_ASSERT(liIndex <= miImageWidth * miImageHeight,
                   "liIndex <= miNumberOfMugshotTypes * miNumberOfMugshotsPerType");

        return static_cast<u8*>(mpMugshotBufferData) + miExtraFilesSizeBytes * liIndex;
    }

    // X360 0x828522D0. Copy one image record's bytes INTO the mugshot buffer slot the
    // record's imageId maps to. The 16-byte record is walked as u32 words:
    // [0]=imageId, [1]=miSize, [2]=source data pointer (committed LoadImageFiles idiom).
    void SaveLoadSystem::CopyImageToBuffer(const void* lpImageFile)
    {
        CGS_ASSERT(lpImageFile != nullptr, "lpImageFile");

        const u32* lpRecord = reinterpret_cast<const u32*>(lpImageFile);

        CGS_ASSERT(static_cast<s32>(lpRecord[1]) == miExtraFilesSizeBytes,
                   "lpImageFile->miSize == miExtraFilesSizeBytes");

        const void* lpSource = reinterpret_cast<const void*>(static_cast<uintptr_t>(lpRecord[2]));
        void*       lpDest   = GetMugshotBufferFromImageId(static_cast<s32>(lpRecord[0]));

        std::memcpy(lpDest, lpSource, static_cast<usize>(miExtraFilesSizeBytes));   // XMemCpy
    }

    // X360 0x828523D0. Build the "Mugshots" Realmc load-entry record: the data half
    // points at the whole mugshot buffer (miExtraFilesSizeBytes * miImageHeight *
    // miImageWidth bytes); the first DataBuffer pair is a zeroed {0,0}. Returned by
    // value (X360 sret).
    RealmcIface::LoadEntryInfo SaveLoadSystem::CreateRealmcMugshotLoadEntryInfo()
    {
        RealmcIface::DataBuffer lZeroPair;   // {0, 0} (default ctor)
        RealmcIface::DataBuffer lDataPair;
        lDataPair.mpData = mpMugshotBufferData;
        lDataPair.muSize = static_cast<u32>(miExtraFilesSizeBytes * miImageHeight * miImageWidth);

        return RealmcIface::LoadEntryInfo("Mugshots", &lZeroPair, &lDataPair);
    }
}
