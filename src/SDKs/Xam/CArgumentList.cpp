#include "SDKs/Xam/CArgumentList.h"

// ===========================================================================
// CArgumentList -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm.
// AddArgument @ 0x8297DC68. See CArgumentList.h for the layout.
// ===========================================================================

// ---------------------------------------------------------------------------
// CArgumentList::AddArgument @ 0x8297DC68
//
//   mr   r11, r3            ; r11 = this
//   li   r9, 4              ; type tag constant 4
//   extsw r8, r4            ; r8 = sign_extend32->64(liValue)
//   lwz  r10, 0x200(r11)    ; r10 = muCount
//   slwi r10, r10, 4        ; r10 = muCount * 16  (element stride)
//   add  r10, r10, r11      ; r10 = &maElements[muCount]
//   stw  r9, 0(r10)         ; element.muType  = 4
//   std  r8, 8(r10)         ; element.miValue = sign_extend(liValue)   (64-bit)
//   lwz  r3, 0x200(r11)     ; r3 = muCount (old)
//   addi r10, r3, 1         ; muCount + 1
//   stw  r10, 0x200(r11)    ; muCount = old + 1
//   blr                     ; return old muCount
//
// The element's +0x04 word is intentionally left untouched (the asm writes only
// the +0x00 type and the +0x08 value), so it is not written here either. The
// 64-bit `std` of the sign-extended argument is preserved exactly (not a 32-bit
// store): r4 is widened by `extsw` before the store.
// ---------------------------------------------------------------------------
int CArgumentList::AddArgument(int liValue)
{
    Element* lpSlot = &maElements[muCount];
    lpSlot->muType = E_ARG_INT64;        // li r9,4 ; stw r9,0(r10)
    lpSlot->miValue = (s64)liValue;       // extsw r8,r4 ; std r8,8(r10)

    int liIndex = (int)muCount;           // lwz r3,0x200(r11)  (returned value)
    muCount = liIndex + 1;                // addi/stw -> muCount = old + 1
    return liIndex;
}
