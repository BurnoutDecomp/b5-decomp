#include "GameSource/GameState/BrnCgsPlayerName.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (over-length assert message)
#include <cstring>                                           // std::strlen / std::strncpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::PlayerName::Construct @ 0x8230E9C0
//
// Validate and store a player name. Guards (verbatim baked file/line strings from the
// X360 binary): the name pointer is non-null, and the string fits in macName
// (strlen < KI_USERNAME_LENGTH == 16). The over-length asserts build their text through a
// CgsDev::StrStream into a stack message buffer (the X360 streams into the global assert
// buffer; a local buffer is used here, matching the committed FlybyData::AddCar precedent).
// The second over-length check is the inlined StrnCpy<KI_USERNAME_LENGTH> bounds guard
// (CgsStringUtils.h:55). The X360 `return strncpy(...)` is a void-function register artifact.

namespace CgsNetwork
{

void PlayerName::Construct(const char* lpcPlayerName)
{
    CGS_ASSERT(lpcPlayerName, "lpcPlayerName");
    if (std::strlen(lpcPlayerName) >= static_cast<u32>(KI_USERNAME_LENGTH))
    {
        CgsDev::Assert::BeginAssert();
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Player name '" << (lpcPlayerName ? lpcPlayerName : "<NULLSTRING>")
                   << "' too long. Max length=" << static_cast<s32>(KI_USERNAME_LENGTH);
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameShared\\GameClasses\\Network/Players/CgsPlayerName.h",
            87);
        CgsDev::Assert::EndAssert();
    }
    // Inlined StrnCpy<KI_USERNAME_LENGTH>(macName, lpcPlayerName) bounds check.
    if (std::strlen(lpcPlayerName) >= static_cast<u32>(KI_USERNAME_LENGTH))
    {
        CgsDev::Assert::BeginAssert();
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "String too long: " << (lpcPlayerName ? lpcPlayerName : "<NULLSTRING>");
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h",
            55);
        CgsDev::Assert::EndAssert();
    }

    std::strncpy(macName, lpcPlayerName, KI_USERNAME_LENGTH);
}

}
