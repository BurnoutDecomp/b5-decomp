#include "SDKs/Xam/CSchemaData.h"

// ===========================================================================
// CSchemaData -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm.
// LookupConstantFromTable @ 0x8297F280. See CSchemaData.h for the layout.
// ===========================================================================

// E_FAIL HRESULT the failure path returns (lis r3,-0x8000 ; ori r3,r3,0x4005).
static const s32 KI_E_FAIL = (s32)0x80004005;

// ---------------------------------------------------------------------------
// CSchemaData::LookupConstantFromTable @ 0x8297F280
//
//   lhz   r10, 0x18(r3)        ; r10 = muConstantCount (u16)
//   clrlwi r11, r4, 16         ; r11 = luIndex zero-extended to 32 bits
//   cmplw cr6, r11, r10        ; unsigned compare index vs count
//   ble   cr6, loc_8297F29C    ; index <= count -> do the lookup
//   lis   r3, -0x8000          ; else return 0x80004005 (E_FAIL)
//   ori   r3, r3, 0x4005
//   blr
// loc_8297F29C:
//   lwz   r10, 0x40(r3)        ; r10 = mpConstantTable
//   slwi  r11, r11, 2          ; r11 = index * 4  (u32 stride)
//   li    r3, 0                ; S_OK
//   lwzx  r11, r11, r10        ; r11 = mpConstantTable[index]
//   stw   r11, 0(r5)           ; *lpOut = value
//   blr                        ; return 0
//
// The guard is strict `index > count` -> fail (i.e. index == count is a valid,
// in-range slot), matching the `ble` (<=) branch taken to the success path. The
// index comparison is unsigned (cmplw); luIndex arrives as a u16 already in range
// of the zero-extend.
// ---------------------------------------------------------------------------
s32 CSchemaData::LookupConstantFromTable(u16 luIndex, u32* lpOut)
{
    if (luIndex > muConstantCount)
    {
        return KI_E_FAIL;
    }

    *lpOut = mpConstantTable[luIndex];
    return 0;
}
