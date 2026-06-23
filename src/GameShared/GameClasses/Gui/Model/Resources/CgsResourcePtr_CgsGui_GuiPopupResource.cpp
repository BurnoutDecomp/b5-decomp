#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<CgsGui::GuiPopupResource>
// (the "CgsGui::GuiPopupResource>" templated-instance ledger id). The body IS the
// generic const accessor inline in CgsResourcePtr.h; this .cpp only forces the
// out-of-line emission of the one symbol the X360 ARTIST build attested.
//
//   operator->() const  @ 0x8267A4E8  (const; baked CgsResourcePtr.h line 563 = 0x233)
//
// X360 @0x8267A4E8 (verified store-for-store against the assembly):
//   lwz  r11, 0(r28)         ; load mpResourceMemory (the +0 word)
//   cmpwi r11, 0             ; null-test it
//   bne  ...skip-assert      ; if non-null, skip
//   <BeginAssert/Clear/FireAssert/EndAssert with msg
//    "Can not instance resource pointer - it has no main memory resource\n",
//    baked file CgsResourcePtr.h line 0x233 (563)>
//   lwz  r3, 0(r28)          ; return mpResourceMemory DIRECTLY (single deref)
// Byte-identical to the HudMessage sibling @0x8267A448 except the baked assert line
// (0x233 = 563 vs 0x220 = 544). Line 563 in CgsResourcePtr.h is the const
// ResourcePtr<Type>::operator->() const, which uses the same "instance" message; both
// do a SINGLE load of offset 0 and return the resource pointer (no double deref). The
// X360-baked file/line are discarded per project convention.
//
// GuiPopupResource stays an INCOMPLETE forward declaration: the accessor only
// static_casts void* mpResourceMemory to const GuiPopupResource* and returns it, which
// needs only an incomplete class type. (The payload struct + its FixDown live in
// CgsGuiPopupResource.cpp.)
namespace CgsGui { struct GuiPopupResource; }

template const CgsGui::GuiPopupResource*
    CgsResource::ResourcePtr<CgsGui::GuiPopupResource>::operator->() const;
