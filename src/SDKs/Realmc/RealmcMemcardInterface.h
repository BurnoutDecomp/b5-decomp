#pragma once

// ===========================================================================
// RealmcIface::MemcardInterface -- the abstract memory-card interface object at
// the head of the Realmc (RealmcIface) memory-card SDK. Sibling to the committed
// Realmc core (SDKs/Realmc/RealmcCore.h / RealmcObjectManager.h / RealmcLocale.h /
// BrnEAThreadX360.h). This is the canonical OWNING home for the three functions
// the X360 binary (BURNOUT_X360_ARTIST.XEX) defines for this class:
//
//     RealmcIface::MemcardInterface::MemcardInterface   @ 0x82B51C00  (ctor)
//     RealmcIface::MemcardInterface::`vector deleting destructor' @ 0x82B51BB8
//     RealmcIface::MemcardInterface::CreateInstance     @ 0x82B52BF8  (static factory)
//
// No Feb-2007 leak source and no DWARF exist for this TU; the SHAPE below is
// reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, MemcardInterface,
// CreateInstance) are preserved verbatim per the naming convention.
//
// This class was previously modelled minimally, consumer-side, inside
// GameShared/GameClasses/Gui/CgsSaveLoadX360.h (the X360 save/load front-end that
// dispatches four of its vtable slots). That header explicitly noted "the full SDK
// lives in its own TUs" -- this is that home; CgsSaveLoadX360.h now #includes it so
// the type is defined once.
//
// VTABLE (X360 off_821484E0). The concrete table at off_821485DC supplies the
// ARTIST-named methods in these exact slots. The earlier GUI-facing names and
// reserved entries were consumer guesses and are replaced here by the native
// slot names and signatures:
//   slot 0 (+0x00) vector deleting destructor
//   slot 1 (+0x04) Shutdown
//   slot 2 (+0x08) Update
//   slot 3 (+0x0C) IsResettable
//   slot 4 (+0x10) SetSignedUserIndex
//   slot 5 (+0x14) MessageDone
//   slot 6 (+0x18) SetMessageState
//   slot 7 (+0x1C) CheckCard
//   slot 8 (+0x20) SetActiveCard
//   slot 9 (+0x24) BootupCheck
//   slot 10 (+0x28) SaveCheck
//   slot 11 (+0x2C) Save
//   slot 12 (+0x30) SetAutosave
//   slot 13 (+0x34) SetMonitor
//   slot 14 (+0x38) Load
//
// The concrete implementation is RealmcIface::MemcardInterfaceImpl (a 0x528-byte
// object CreateInstance allocates and constructs); its full layout and method
// bodies are owned by its own TU, RealmcMemcardInterfaceImpl.cpp, which also holds
// CreateInstance -- see RealmcMemcardInterfaceImpl.h.
// ===========================================================================

#include "SDKs/Realmc/RealmcLocale.h"   // RealmcCore::Locale::LocaleGetStrCallback

// The two Realmc/EAThread collaborators CreateParams references by pointer only;
// full definitions live in their own homes (pulled in by the .cpp).
namespace EA { namespace Allocator { class ICoreAllocator; } }
namespace EA { namespace Thread { struct ThreadParameters; } }

namespace RealmcIface
{
class IGameInterface;
class GameInfo;
class SaveCheckParams;
class SaveInfo;
class EntryContentName;
class LoadEntryInfo;
class TitleInfo;

// ---------------------------------------------------------------------------
// MemcardInterface -- abstract base of the Realmc memory-card SDK. It is a pure
// interface (the base vtable off_821484E0 traps the pure slots); the concrete
// MemcardInterfaceImpl overrides them.
// ---------------------------------------------------------------------------
class MemcardInterface
{
public:
    // The parameter block CreateInstance reads (X360 a1 -- three consecutive
    // words). Named members replace the raw a1[0]/a1[1]/a1[2] indexing.
    struct CreateParams
    {
        EA::Allocator::ICoreAllocator*     mpAllocator;    // +0x00 -> GetMemAllocator
        RealmcCore::Locale::LocaleGetStrCallback mpfnGetStr;     // +0x04 -> SetLocaleGetStrCallback
        EA::Thread::ThreadParameters*            mpThreadParams; // +0x08 (optional override)
    };

    // @ 0x82B51C00 -- construct: install the MemcardInterface vtable (off_821484E0).
    // The X360 body is a single `stw vtable, 0(this)`; an empty ctor emits exactly
    // that. Called by the derived MemcardInterfaceImpl ctor/dtor as the base
    // subobject.
    MemcardInterface();

    // slot 0 -- backs the X360 `vector deleting destructor' @ 0x82B51BB8 (restore
    // the vtable, then operator delete when the delete flag bit0 is set).
    virtual ~MemcardInterface();

    virtual void Shutdown() = 0;                                  // slot 1  (+0x04)
    virtual int Update(int liArg) = 0;                            // slot 2  (+0x08)
    virtual bool IsResettable() = 0;                              // slot 3  (+0x0C)
    virtual int SetSignedUserIndex(int liUserIndex) = 0;          // slot 4  (+0x10)
    virtual void MessageDone(int liChoice) = 0;                   // slot 5  (+0x14)
    virtual int SetMessageState(int liMode, int liMask) = 0;      // slot 6  (+0x18)
    virtual int CheckCard(int liArg) = 0;                         // slot 7  (+0x1C)
    virtual int SetActiveCard(int liArg1, int liArg2, int liArg3) = 0; // slot 8 (+0x20)
    virtual int BootupCheck(SaveInfo* lpSaveInfo, int liNumEntries,
                            LoadEntryInfo* lpEntries, TitleInfo* lpTitleInfo) = 0; // slot 9 (+0x24)
    virtual int SaveCheck(SaveCheckParams* lpParams) = 0;         // slot 10 (+0x28)
    virtual int Save(SaveInfo* lpSaveInfo, int liCount, LoadEntryInfo* lpEntries,
                     int liFlags, TitleInfo* lpTitleInfo) = 0;    // slot 11 (+0x2C)
    virtual int SetAutosave(int liEnabled, SaveCheckParams* lpParams) = 0; // slot 12 (+0x30)
    virtual int SetMonitor(int liState) = 0;                      // slot 13 (+0x34)
    virtual int Load(EntryContentName* lpEntryContentName, int liNumEntries,
                     LoadEntryInfo* lpEntries, int liFlags, TitleInfo* lpTitleInfo) = 0; // slot 14 (+0x38)

    // @ 0x82B52BF8 -- the memory-card interface factory / singleton accessor.
    // Installs the caller's allocator + locale callback, prepares the shared
    // EA::Thread::ThreadParameters the memory-card worker uses (either the caller's
    // override or a lazily-configured "realmemcard_xenon" default), brings up the
    // Realmc ObjectManager, then lazily constructs the single MemcardInterfaceImpl
    // and returns it (the same instance on every subsequent call).
    static MemcardInterface* CreateInstance(const CreateParams* pParams,
                                            IGameInterface* pGameInterface, const GameInfo& rGameInfo);
};

} // namespace RealmcIface
