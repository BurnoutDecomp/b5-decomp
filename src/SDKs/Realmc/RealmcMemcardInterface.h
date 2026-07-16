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
// VTABLE (X360 off_821484E0 -- the MSVC vtable the compiler emits from this class
// definition; the base installs it in its ctor and restores it in its deleting
// destructor). Slot order is X360-authoritative (the save/load front-end dispatches
// these byte offsets):
//   slot 0  (+0x00)  the (vector deleting) destructor
//   slot 1  (+0x04)  reserved
//   slot 2  (+0x08)  Update(int)            -- SaveLoadSystem::Update forwards here
//   slot 3  (+0x0C)  reserved
//   slot 4  (+0x10)  UserSignedIn()         -- SaveLoadSystem::SetSignedInUserIndex (wave B)
//   slot 5  (+0x14)  MessageChoice(int)     -- SaveLoadSystem::HandleMemcardOption (wave B)
//   slot 6  (+0x18)  SetSilent(int, int)    -- SaveLoadSystem::SetSilentMode (wave B)
//   slots 7-8        reserved
//   slot 9  (+0x24)  Bootup(...)            -- SaveLoadSystem::BootupStart (wave B)
//   slot 10 (+0x28)  reserved
//   slot 11 (+0x2C)  WriteSave(...)         -- SaveLoadSystem::Save
//   slot 12 (+0x30)  CheckSave(int, void*)  -- SaveLoadSystem::Save
//   slot 13 (+0x34)  SetActive(int)         -- SaveLoadSystem::Prepare (passes 1)
//   slot 14 (+0x38)  ReadSave(...)          -- SaveLoadSystem::LoadHandleConfirmLoad (wave B)
// The remaining slots are reserved (declared, not invented) to pin the attested ones
// at their byte offsets; the wave-B slot names/signatures are grounded in the
// CgsGui::SaveLoadSystem call sites (FLAGGED inline below) -- their true SDK names are
// not recovered.
//
// The concrete implementation is RealmcIface::MemcardInterfaceImpl (a 0x528-byte
// object CreateInstance allocates and constructs); its full layout and method
// bodies are owned by its own (not-yet-homed) TU -- see RealmcMemcardInterfaceImpl.h.
// ===========================================================================

#include "SDKs/Realmc/RealmcLocale.h"   // RealmcCore::Locale::LocaleGetStrCallback

// The two Realmc/EAThread collaborators CreateParams references by pointer only;
// full definitions live in their own homes (pulled in by the .cpp).
namespace RealmcCore { class IRealmcAllocatorBackend; }
namespace EA { namespace Thread { struct ThreadParameters; } }

namespace RealmcIface
{

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
        RealmcCore::IRealmcAllocatorBackend*     mpAllocator;    // +0x00 -> GetMemAllocator
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

    virtual void Reserved01() = 0;                                // slot 1  (+0x04)
    virtual int  Update(int liArg) = 0;                          // slot 2  (+0x08)
    virtual void Reserved03() = 0;                                // slot 3  (+0x0C)
    // slot 4 (+0x10) -- notify the memory card that a user signed in. Dispatched by
    // CgsGui::SaveLoadSystem::SetSignedInUserIndex @0x8284C210 (tail-call, no arguments)
    // whenever the cached user index becomes >= 0. Was Reserved04 before wave B.
    // FLAG: slot name inferred from the consumer.
    virtual void UserSignedIn() = 0;                              // slot 4  (+0x10)
    // slot 5 (+0x14) -- deliver the user's message-box choice (1..4, the Realmc codes
    // CgsGui::SaveLoadSystem::MessageChoiceForOptionIndex maps option indices 0..3 onto).
    // Dispatched by SaveLoadSystem::HandleMemcardOption @0x8284C6D8. Was Reserved05.
    // FLAG: slot name inferred from the consumer.
    virtual void MessageChoice(int liChoice) = 0;                 // slot 5  (+0x14)
    // slot 6 (+0x18) -- toggle silent mode. Dispatched by SaveLoadSystem::SetSilentMode
    // @0x82473110 as (0|1, -1); the -1 second argument's role is unrecovered. Was
    // Reserved06. FLAG: slot name inferred from the consumer.
    virtual void SetSilent(int liSilent, int liArg) = 0;          // slot 6  (+0x18)
    virtual void Reserved07() = 0;                                // slot 7  (+0x1C)
    virtual void Reserved08() = 0;                                // slot 8  (+0x20)
    // slot 9 (+0x24) -- start the bootup save check. Dispatched by
    // SaveLoadSystem::BootupStart @0x82855A60 as (saveInfo, count, entries, titleInfo)
    // with count/entries == (2, the two LoadEntryInfo records) on the reload lane or
    // (0, NULL) on the plain check. Was Reserved09. FLAG: slot name inferred from the
    // consumer.
    virtual void Bootup(void* lpSaveInfo, int liNumEntries, void* lpEntries,
                        void* lpTitleInfo) = 0;                   // slot 9  (+0x24)
    virtual void Reserved10() = 0;                                // slot 10 (+0x28)
    virtual void WriteSave(void* lpSaveInfo, int liCount, void* lpEntries,
                           int liFlags, void* lpTitleInfo) = 0;   // slot 11 (+0x2C)
    virtual void CheckSave(int liArg, void* lpCheckParams) = 0;   // slot 12 (+0x30)
    virtual void SetActive(int liActive) = 0;                     // slot 13 (+0x34)
    // slot 14 (+0x38) -- read a save's entries back. Dispatched by
    // SaveLoadSystem::LoadHandleConfirmLoad @0x82855EC0 as
    // (&EntryContentName, 2, the two LoadEntryInfo records, 0, TitleInfo::Empty()).
    // Appended by wave B (the slot was beyond the previously pinned +0x34 tail).
    // FLAG: slot name inferred from the consumer.
    virtual void ReadSave(void* lpEntryContentName, int liNumEntries, void* lpEntries,
                          int liFlags, void* lpTitleInfo) = 0;    // slot 14 (+0x38)

    // @ 0x82B52BF8 -- the memory-card interface factory / singleton accessor.
    // Installs the caller's allocator + locale callback, prepares the shared
    // EA::Thread::ThreadParameters the memory-card worker uses (either the caller's
    // override or a lazily-configured "realmemcard_xenon" default), brings up the
    // Realmc ObjectManager, then lazily constructs the single MemcardInterfaceImpl
    // and returns it (the same instance on every subsequent call).
    static MemcardInterface* CreateInstance(const CreateParams* pParams,
                                            int liParam2, int liParam3);
};

} // namespace RealmcIface
