// CgsGui::SaveLoadSystem - wave-B partfile (group 4: the mugshot buffer family).
// Reconstructed from BURNOUT_X360_ARTIST.XEX per the keystone spec
// (scratchpad/waveB/class_CgsGui__SaveLoadSystem.spec.md, rows 14-16). The class
// layout is frozen in CgsSaveLoadX360.h; this file only bodies functions.
//
// Functions in scope for this partfile:
//   CopyImageToBuffer                @ 0x828522D0
//   CreateRealmcMugshotLoadEntryInfo @ 0x828523D0
//
// (GetMugshotBufferFromImageId @0x8284C910 was also reconstructed here by wave B, but the
// later profile link-closure wave landed its own body in CgsSaveLoadPS3.cpp -- that TU owns
// it now; the duplicate was removed here in the wave-C reconcile, which also renamed the
// mugshot-grid members to the header's X360-assert-attested names
// miNumberOfMugshotTypes/miNumberOfMugshotsPerType.)

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

namespace CgsGui
{
    // X360 0x828522D0. Copy one image record's bytes INTO the mugshot buffer slot the
    // record's imageId maps to. The record is the homed CgsGui::ImageFileInfo:
    // lwz r4,0(r31)=miId, lwz r11,4(r31)=miSize, lwz r29,8(r31)=mpBuffer. (The +8 word is
    // the source pointer on the 32-bit console; access it by name so the host's 8-byte
    // pointer is not truncated -- BrnGui::ProfileManager::CopyImageToBuffer hands us a live
    // heap pointer there.)
    void SaveLoadSystem::CopyImageToBuffer(const void* lpImageFile)
    {
        CGS_ASSERT(lpImageFile != nullptr, "lpImageFile");

        const ImageFileInfo* lpRecord = reinterpret_cast<const ImageFileInfo*>(lpImageFile);

        CGS_ASSERT(lpRecord->miSize == miExtraFilesSizeBytes,
                   "lpImageFile->miSize == miExtraFilesSizeBytes");

        void* lpDest = GetMugshotBufferFromImageId(lpRecord->miId);

        std::memcpy(lpDest, lpRecord->mpBuffer, static_cast<usize>(miExtraFilesSizeBytes));   // XMemCpy
    }

    // X360 0x828523D0. Build the "Mugshots" Realmc load-entry record: the data half
    // points at the whole mugshot buffer (miExtraFilesSizeBytes * miNumberOfMugshotsPerType *
    // miNumberOfMugshotTypes bytes); the first DataBuffer pair is a zeroed {0,0}. Returned by
    // value (X360 sret).
    RealmcIface::LoadEntryInfo SaveLoadSystem::CreateRealmcMugshotLoadEntryInfo()
    {
        RealmcIface::DataBuffer lZeroPair;   // {0, 0} (default ctor)
        RealmcIface::DataBuffer lDataPair;
        lDataPair.mpData = mpMugshotBufferData;
        lDataPair.muSize = static_cast<u32>(miExtraFilesSizeBytes * miNumberOfMugshotsPerType
                                            * miNumberOfMugshotTypes);

        return RealmcIface::LoadEntryInfo("Mugshots", &lZeroPair, &lDataPair);
    }
}
