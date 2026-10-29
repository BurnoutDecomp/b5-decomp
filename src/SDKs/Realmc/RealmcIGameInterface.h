#pragma once
#include "types.hpp"

namespace RealmcIface
{
// ARTIST base table 820CDC58, derived SaveLoad table 820CF148 (17 entries).
// The IGameInterface name is attested by the scalar deleting destructor's IDA
// symbol at 827DB478. Argument registers are corroborated by the corresponding
// GameCallbackProcessor dispatches at 82B543F0..82B54BD8.
class IGameInterface
{
public:
    virtual ~IGameInterface();
    virtual void ShowMessage(const char*, u32, const char**) = 0; // +04
    virtual void ClearMessage() = 0;                            // +08
    virtual void ShowAutosaveIcon(bool) = 0;                    // +0C
    virtual void BootupCheckDone(u32) = 0;                      // +10
    // No-op in SaveLoad (8284CB38); six arguments forwarded by 82B54AC0.
    virtual void SaveCheckDone(u32, u32, u32, u32, const int*, s32) = 0; // +14
    virtual void SaveDone(u32, u32, const char*) = 0;            // +18
    virtual void SetAutosaveDone(s32, s32, s32) = 0;             // +1C
    virtual void CardRemoved() = 0;                            // +20
    virtual bool CheckLoadedData(u32, u32) = 0;                 // +24
    virtual void LoadDone(u32, s32, u32, void*) = 0;             // +28
    virtual void DeleteDone() = 0;                             // +2C
    virtual void ClearEntries() = 0;                           // +30
    virtual void FoundEntry(const void*) = 0;                  // +34
    virtual void FindEntriesDone(u32, u32) = 0;                 // +38
    // These two roles have no surviving names in ARTIST. They are real slots,
    // not reserved padding: 82B546C0 forwards (msg+8,msg+C,&msg+10) to +3C;
    // 82B54740 forwards MessageSetActiveCardDone's two words to +40.
    virtual void Callback3C(u32, u32, const void*) = 0;          // +3C
    virtual void SetActiveCardDone(u32, u32) = 0;                // +40
};
}
