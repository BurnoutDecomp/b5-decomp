// CgsGui::SaveLoadSystem -- wave-B partfile 04 (group G5: save-info / check-params chain).
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The class is homed in CgsSaveLoadX360.h (the
// X360 save/load front-end; its assert sites cite gui/CgsSaveLoadX360.cpp). This partfile
// bodies three members store-for-store from the X360 ASM:
//
//   CgsGui::SaveLoadSystem::CreateRealmcSaveInfo         @ 0x8284C818
//   CgsGui::SaveLoadSystem::CreateRealmcSaveCheckParams  @ 0x8284C898
//   CgsGui::SaveLoadSystem::EnableAutosave               @ 0x82852368
//
// One cross-TU callee is not yet reconstructed (RealmcIface::SaveInfo::SaveInfo). It is
// reached through an anonymous-namespace declaration whose signature is taken from the
// X360 call-site argument registers; it resolves to its own SDK TU at link time (this TU
// only needs the declaration to compile) -- the committed sibling CgsSaveLoadPS3.cpp uses
// the same idiom for its un-homed RealmcIface helpers.

#include "GameShared/GameClasses/Gui/CgsSaveLoadX360.h"   // CgsGui::SaveLoadSystem (+ RealmcIface::MemcardInterface)

#include "SDKs/Realmc/RealmcEntryContentName.h"           // RealmcIface::EntryContentName
#include "SDKs/Realmc/RealmcDataBuffer.h"                 // RealmcIface::DataBuffer
#include "SDKs/Realmc/RealmcTitleInfo.h"                  // RealmcIface::TitleInfo
#include "SDKs/Realmc/RealmcSaveReq.h"                    // RealmcIface::SaveReq
#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"       // RealmcIface::SaveCheckParams

namespace
{
    // RealmcIface::SaveInfo::SaveInfo -- the un-homed SDK ctor CreateRealmcSaveInfo invokes.
    // Constructs a SaveInfo (X360: 352-byte stack object) at lpSaveInfo from the entry-content
    // name and the content-information { buffer, size } data buffer. Declared (not defined)
    // here; its body lives in the RealmcIface::SaveInfo TU. Argument order matches the ASM
    // (a1 = dest, a2 = EntryContentName, a3 = DataBuffer pair).
    void RealmcSaveInfo_Construct(void* lpSaveInfo, const void* lpContentName,
                                  const void* lpDataBuffer);
}

namespace CgsGui
{
    // X360 0x8284C818. Build the RealmcIface::SaveInfo for the current save into lpSaveInfo:
    // an EntryContentName (wide metadata title, ascii title, and a size seed of the total
    // mugshot bytes plus the game-data size) plus the { content-info buffer, content-info size }
    // data buffer. Returns lpSaveInfo.
    void* SaveLoadSystem::CreateRealmcSaveInfo(void* lpSaveInfo)
    {
        // Size seed: (extra-file bytes * image height * image width) + the game-data size.
        const s32 liSizeSeed = miExtraFilesSizeBytes * miImageHeight * miImageWidth
                             + static_cast<s32>(muGameDataSizeKb);
        RealmcIface::EntryContentName lContentName(macwMetadataTitle, macTitle, liSizeSeed);

        // The content-information file's { buffer, size } descriptor.
        RealmcIface::DataBuffer lContentInfo;
        lContentInfo.mpData = mpContentInfoFileBuffer;
        lContentInfo.muSize = static_cast<u32>(miContentInfoFileSize);

        RealmcSaveInfo_Construct(lpSaveInfo, &lContentName, &lContentInfo);
        return lpSaveInfo;
    }

    // X360 0x8284C898 (X360 sret: dest in r3, `this` in r4). Build the autosave SaveCheckParams:
    // one SaveReq for the current save (title + save-info + the fixed 0x100 flag + game-data
    // size + the empty title-info word), wrapped in a one-entry SaveCheckParams.
    RealmcIface::SaveCheckParams SaveLoadSystem::CreateRealmcSaveCheckParams()
    {
        // The shared empty title-info singleton (fetched first, matching the X360 order).
        RealmcIface::TitleInfo& lTitleInfo = RealmcIface::TitleInfo::Empty();

        // Build the save-info into an opaque stack buffer (X360 SaveInfo == 352 bytes).
        u8    laSaveInfo[352];
        void* lpSaveInfo = CreateRealmcSaveInfo(laSaveInfo);

        RealmcIface::SaveReq lSaveReq(macTitle, lpSaveInfo, 0x100u, muGameDataSizeKb,
                                      &lTitleInfo.muTitle);
        RealmcIface::SaveReq* lpSaveReq = &lSaveReq;
        return RealmcIface::SaveCheckParams(1, &lpSaveReq);
        // (The trailing X360 STUB is the stack SaveReq's scope-end dtor -- emitted implicitly.)
    }

    // X360 0x82852368. (Re)arm the autosave pre-flight: build the SaveCheckParams and hand it to
    // the memory-card interface (CheckSave(1, &params)). liArg is passed by every caller as -1
    // and is never read; the ASM's explicit ~SaveCheckParams is the scope-end dtor (implicit).
    void SaveLoadSystem::EnableAutosave(s32 liArg)
    {
        (void)liArg;
        RealmcIface::SaveCheckParams lCheckParams = CreateRealmcSaveCheckParams();
        mpMemcardInterface->CheckSave(1, &lCheckParams);
    }
}
