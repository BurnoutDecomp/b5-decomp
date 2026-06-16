#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (long-string assert message)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsIDCompress        @ 0x82815A20
//   CgsIDUnCompress      @ 0x82815C78
//   CgsIDConvertToString @ 0x82815D30

// @ 0x82815A20. Compress up to KI_CGSID_STRING_LEN-1 (=12) characters of a
// NUL-terminated string into a single 64-bit CgsID by accumulating a base-40
// number (lID = lID * 40 + digit), most-significant character first. After the
// input is consumed the remaining slots are filled by multiplying by 40 again,
// left-justifying the encoded text in the 12-digit base-40 word.
//
// If the string did not terminate within 12 characters the X360 fires the assert
// TWICE (CgsID.cpp:180 and :185 - two identical guards in the original source).
CgsID CgsIDCompress(const char* lpcString)
{
    CgsID lID = 0;

    s32 liI;
    for (liI = 0; liI < KI_CGSID_STRING_LEN - 1; ++liI)
    {
        const s32 liChar = static_cast<u8>(lpcString[liI]);
        if (liChar == 0)
            break;

        lID *= 40;

        if (liChar == '_')
        {
            lID += 39;
        }
        else if (liChar < 'a')
        {
            if (liChar < 'A')
            {
                if (liChar < '0')
                {
                    if (liChar == '/')
                        lID += 2;
                    else if (liChar == '-')
                        lID += 1;
                    // any other punctuation maps to slot 0 (space)
                }
                else
                {
                    // '0'..'9' -> 3..12
                    lID += liChar - ('0' - 3);
                }
            }
            else
            {
                // 'A'..'Z' -> 13..38
                lID += liChar - ('A' - 13);
            }
        }
        else
        {
            // 'a'..'z' -> 13..38 (same indices as the uppercase letters)
            lID += liChar - ('a' - 13);
        }
    }

    // CgsID.cpp:180 - string did not fit in 12 characters.
    if (lpcString[liI])
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "CgsIDCompress: input string too long! - \"";
        lStrStream << (lpcString ? lpcString : "<NULLSTRING>");
        lStrStream << "\"\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lacMessage,
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\core\\CgsID.cpp",
            180);
        CgsDev::Assert::EndAssert();
    }

    // CgsID.cpp:185 - duplicate guard in the original source.
    if (lpcString[liI])
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "CgsIDCompress: input string too long! - \"";
        lStrStream << (lpcString ? lpcString : "<NULLSTRING>");
        lStrStream << "\"\n";
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lacMessage,
            "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\core\\CgsID.cpp",
            185);
        CgsDev::Assert::EndAssert();
    }

    // Left-justify: shift the encoded characters up to the top of the base-40 word.
    for (; liI < KI_CGSID_STRING_LEN - 1; ++liI)
        lID *= 40;

    return lID;
}

// @ 0x82815C78. Expand a CgsID into KI_CGSID_STRING_LEN-1 (=12) characters plus a
// terminating NUL. The base-40 digits are pulled out least-significant first and
// written from the last character position backwards, so the printable string reads
// most-significant-digit first. The letter slots (13..38) always reproduce uppercase.
void CgsIDUnCompress(CgsID lID, char* lpcString)
{
    for (s32 liIndex = KI_CGSID_STRING_LEN - 2; liIndex >= 0; --liIndex)
    {
        s32 liDigit = static_cast<s32>(lID % 40);
        lID /= 40;

        char lcChar;
        if (liDigit == 39)
        {
            lcChar = '_';
        }
        else if (liDigit < 13)
        {
            if (liDigit < 3)
            {
                if (liDigit == 2)
                    lcChar = '/';
                else if (liDigit == 1)
                    lcChar = '-';
                else
                    lcChar = ' ';   // slot 0
            }
            else
            {
                // 3..12 -> '0'..'9'
                lcChar = static_cast<char>(liDigit + ('0' - 3));
            }
        }
        else
        {
            // 13..38 -> 'A'..'Z'
            lcChar = static_cast<char>(liDigit + ('A' - 13));
        }

        lpcString[liIndex] = lcChar;
    }

    lpcString[KI_CGSID_STRING_LEN - 1] = '\0';
}

// @ 0x82815D30. Un-compress the id into the caller's buffer, then trim the trailing
// space padding in place (the unused low-order base-40 slots decode to spaces at the
// end of the string).
void CgsIDConvertToString(CgsID lID, char* lpcString)
{
    CgsIDUnCompress(lID, lpcString);

    for (char* lpc = lpcString + (KI_CGSID_STRING_LEN - 2); *lpc == ' '; --lpc)
    {
        *lpc = '\0';
        if (lpc <= lpcString)
            break;
    }
}
