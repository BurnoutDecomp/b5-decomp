#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"

// Per-instantiation TU for CgsResource::ResourcePtr<CgsGui::GuiHudMessageResource>
// (the "CgsGui::GuiHudMessageResource>" templated-instance ledger id). The body IS the
// generic non-const accessor inline in CgsResourcePtr.h; this .cpp only forces the
// out-of-line emission of the one symbol the X360 ARTIST build attested.
//
//   operator->()  @ 0x8267A448  (non-const; baked CgsResourcePtr.h line 544 = 0x220)
//
// X360 @0x8267A448 (verified store-for-store against the assembly):
//   lwz  r11, 0(r28)         ; load mpResourceMemory (the +0 word)
//   cmplwi r11, 0            ; null-test it
//   bne  ...skip-assert      ; if non-null, skip
//   <BeginAssert/Clear/FireAssert/EndAssert with msg
//    "Can not instance resource pointer - it has no main memory resource\n",
//    baked file CgsResourcePtr.h line 0x220 (544)>
//   lwz  r3, 0(r28)          ; return mpResourceMemory DIRECTLY (single deref)
// This is the generic ResourcePtr<Type>::operator->() non-const: a SINGLE load of
// offset 0, the "instance" assert at line 544, return the resource pointer. (Contrast
// the Font sibling @0x823C1C20 which double-derefs; this one does not.) The X360-baked
// file/line are discarded per project convention -- CGS_ASSERT supplies __FILE__/__LINE__.
//
// GuiHudMessageResource stays an INCOMPLETE forward declaration: the accessor only
// static_casts void* mpResourceMemory to GuiHudMessageResource* and returns it, which
// needs only an incomplete class type, never a member access or a dereference. (The
// payload struct's full definition + its FixUp live in CgsGuiHudMessage.cpp.)
namespace CgsGui { struct GuiHudMessageResource; }

template CgsGui::GuiHudMessageResource*
    CgsResource::ResourcePtr<CgsGui::GuiHudMessageResource>::operator->();
