#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert Begin/Fire/End + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h" // CgsDev::StrStream (long-string assert message)
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\core\\CgsID.cpp"
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
        // FLAG: ARTIST 0x82815A40 `extsb r11` sign-extends the byte (DecFIGS
        // 0xB171E0 reads it as `char v5` and uses signed compares vs 'a'/'A'/'0'/
        // '/'/'-'). Must be a SIGNED char so bytes >= 0x80 go negative and fall to
        // slot 0, not into the 'a'..'z' branch. (Was static_cast<u8>, a silent
        // sign mismatch for non-ASCII input.)
        const s32 liChar = static_cast<s8>(lpcString[liI]);
        if (liChar == 0)
            break;

        lID *= 40;

        if (liChar == '_')
            lID += 39;
        else if (liChar >= 'a')
            lID += liChar - ('a' - 13);
        else if (liChar >= 'A')
            lID += liChar - ('A' - 13);
        else if (liChar >= '0')
            lID += liChar - ('0' - 3);
        else if (liChar == '/')
            lID += 2;
        else if (liChar == '-')
            lID += 1;
    }

    CGS_ASSERT_STREAM_AT(!lpcString[liI],
        "CgsIDCompress: input string too long! - \"" << lpcString << "\"\n", CGS_ARTIST_FILE, 180);
    CGS_ASSERT_STREAM_AT(!lpcString[liI],
        "CgsIDCompress: input string too long! - \"" << lpcString << "\"\n", CGS_ARTIST_FILE, 185);

    // Left-justify: shift the encoded characters up to the top of the base-40 word.
    for (; liI < KI_CGSID_STRING_LEN - 1; ++liI)
        lID *= 40;

    return lID;
}

// ARTIST 0x82815C78. Decode the current digit before dividing for the next one.
void CgsIDUnCompress(CgsID lID, char* lpcString)
{
    for (s32 liIndex = KI_CGSID_STRING_LEN - 2; liIndex >= 0; --liIndex)
    {
        const char lcDigit = static_cast<char>(lID % 40);
        char lcChar;
        if (lcDigit == 39)
            lcChar = '_';
        else if (lcDigit >= 13)
            lcChar = static_cast<char>(lcDigit + ('A' - 13));
        else if (lcDigit >= 3)
            lcChar = static_cast<char>(lcDigit + ('0' - 3));
        else if (lcDigit == 2)
            lcChar = '/';
        else if (lcDigit == 1)
            lcChar = '-';
        else
            lcChar = lcDigit == 0 ? ' ' : '\0';
        lpcString[liIndex] = lcChar;
        lID /= 40;
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

#undef CGS_ARTIST_FILE
