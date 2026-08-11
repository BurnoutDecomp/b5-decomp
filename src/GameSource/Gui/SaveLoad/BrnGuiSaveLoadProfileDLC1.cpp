// ===================================================================================
// BrnGuiSaveLoad::ProfileDLC1  -- implementation
//   class:BrnGuiSaveLoad::ProfileDLC1
//
//   IsDLCCarId      @0x824EFF30
//   Construct       @0x824FF400
//   ValidateProfile @0x824FF480
//
// Reconstructed store-for-store from the X360 pseudocode/asm. Member access is by name.
// ===================================================================================
#include "GameSource/Gui/SaveLoad/BrnGuiSaveLoadProfileDLC1.h"

#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // CgsDev::Log::gpDebugPrint, Message::gxMessageFilterFlags

#include <cstring>   // std::memset (the Serialise image clear)

namespace BrnGuiSaveLoad
{
    // Compile-time pin: the object spans the whole serialised DLC1 image, so the clear in
    // ConstructImage is a plain object-sized memset rather than an over-length write.
    static_assert(sizeof(ProfileDLC1) == ProfileDLC1::KI_IMAGE_SIZE_BYTES,
                  "BrnGuiSaveLoad::ProfileDLC1 must span the 9800-byte serialised image");

    // Outlined from BrnProgression::Profile::Serialise @0x8237C1F0 (prologue
    // `memset(a4, 0, 9800)` + epilogue `*a4 = 6`); see the header note.
    void ProfileDLC1::ConstructImage()
    {
        std::memset(this, 0, KI_IMAGE_SIZE_BYTES);
        miVersion = KI_VERSION_CURRENT;
    }

    // @0x824EFF30
    bool ProfileDLC1::IsDLCCarId(const BrnProgression::CarData& lrCar)
    {
        // The X360 reads the 64-bit id (ld r11, 0(r3)), keeps the top 14 bits
        // (clrrdi r11,r11,50) and compares them to the DLC marker shifted up
        // (li r10,0x2957; sldi r10,r10,50). True iff the top 14 bits equal the marker.
        return (lrCar.mId >> 50) == KU_DLC_CAR_ID_MARKER;
    }

    // @0x824FF400
    void ProfileDLC1::Construct()
    {
        // X360 store sequence (the +0 version word is intentionally not written here):
        //   stw 0 @+0x7B0, stw 0 @+0xAC8, std 0 @+0x2628, std 0 @+0x2630,
        //   XMemSet(+0x2638,0,4) .. XMemSet(+0x2644,0,4).
        muField_AC8  = 0;
        muField_7B0  = 0;
        muField_2628 = 0;
        muField_2630 = 0;
        muField_2638 = 0;
        muField_263C = 0;
        muField_2640 = 0;
        muField_2644 = 0;
    }

    // @0x824FF480
    bool ProfileDLC1::ValidateProfile()
    {
        using namespace CgsDev;

        const s32 liVersion = miVersion;

        if (liVersion == KI_VERSION_UNINITIALISED)
        {
            // Uninitialised sentinel: warn and upgrade by resetting to defaults.
            if ((Message::gxMessageFilterFlags & 1) != 0)
            {
                *Log::gpDebugPrint
                    << "DLC1 Profile is uninitialised, upgrading profile\n";
            }

            Construct();
            return true;
        }

        if (liVersion != KI_VERSION_CURRENT)
        {
            // Unrecognised version: reset to defaults and log the mismatch.
            Construct();

            if ((Message::gxMessageFilterFlags & 1) != 0)
            {
                *Log::gpDebugPrint
                    << "DLC1 Profile is not the same version, resetting.  Expected "
                    << KI_VERSION_CURRENT
                    << ", got "
                    << liVersion
                    << "\n";
            }
        }

        return true;
    }
}
