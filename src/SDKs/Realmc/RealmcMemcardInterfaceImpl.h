#pragma once

// ===========================================================================
// RealmcIface::MemcardInterfaceImpl -- the concrete memory-card interface the
// factory RealmcIface::MemcardInterface::CreateInstance (@ 0x82B52BF8) allocates
// (a 0x528 / 1320-byte object) and constructs. It derives from the abstract
// RealmcIface::MemcardInterface and overrides its interface slots.
//
// This is a MINIMAL home: it declares only what CreateInstance needs to construct
// the object faithfully -- the derivation, the four-argument constructor the X360
// asm calls, and the interface overrides required to make the class concrete
// (instantiable). The FULL 0x528-byte member layout and every method BODY are
// owned by the impl's own (not-yet-homed) TU, which extends this header additively.
// Nothing here fabricates that layout.
//
// CreateInstance's construction site (X360 asm @ 0x82B52CC8..0x82B52CD4):
//   r3 = pMem (the 1320-byte block), r4 = pThreadParams, r5 = liParam2,
//   r6 = liParam3, then bl MemcardInterfaceImpl::MemcardInterfaceImpl.
// ===========================================================================

#include "SDKs/Realmc/RealmcMemcardInterface.h"   // RealmcIface::MemcardInterface (base)

namespace EA { namespace Thread { struct ThreadParameters; } }

namespace RealmcIface
{

class MemcardInterfaceImpl : public MemcardInterface
{
public:
    // @ (its own TU) -- the ctor CreateInstance calls on the freshly-allocated
    // 1320-byte block: r4 = the shared ThreadParameters, r5/r6 = CreateInstance's
    // pass-through liParam2/liParam3 (roles owned by the impl TU).
    MemcardInterfaceImpl(EA::Thread::ThreadParameters* pThreadParams,
                         int liParam2, int liParam3);

    // Interface overrides (bodies owned by the impl TU). Declared here so the class
    // is concrete and CreateInstance can construct it; each matches the base slot.
    ~MemcardInterfaceImpl() override;
    void Reserved01() override;
    int  Update(int liArg) override;
    void Reserved03() override;
    // Wave B: slots 4/5/6/9 were renamed in the base to their consumer-attested
    // shapes (UserSignedIn/MessageChoice/SetSilent/Bootup) and slot 14 (ReadSave)
    // was appended -- the override decls track the base so the class stays concrete.
    void UserSignedIn() override;
    void MessageChoice(int liChoice) override;
    void SetSilent(int liSilent, int liArg) override;
    void Reserved07() override;
    void Reserved08() override;
    void Bootup(void* lpSaveInfo, int liNumEntries, void* lpEntries,
                void* lpTitleInfo) override;
    void Reserved10() override;
    void WriteSave(void* lpSaveInfo, int liCount, void* lpEntries,
                   int liFlags, void* lpTitleInfo) override;
    void CheckSave(int liArg, void* lpCheckParams) override;
    void SetActive(int liActive) override;
    void ReadSave(void* lpEntryContentName, int liNumEntries, void* lpEntries,
                  int liFlags, void* lpTitleInfo) override;
};

} // namespace RealmcIface
