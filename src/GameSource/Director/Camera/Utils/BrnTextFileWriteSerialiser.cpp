// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.cpp
//
// The out-of-line members of BrnDirector::Camera::TextFileWriteSerialiser: Construct /
// Destruct, FormatName, the leaf writers and the vec3 writer.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"            // Utils::VersionNumber / Utils::FOV
#include "GameShared/GameClasses/Development/CgsStrStream.h"         // CgsDev::StrStream (the vec3 label sink)

#include <cstring>   // strlen (FormatName)

namespace BrnDirector
{
namespace Camera
{

// The per-component label suffixes the vec3 leaf appends to the field name.
static const char* const KAPC_COMPONENT_SUFFIX[3] = { ": x", ": y", ": z" };

void TextFileWriteSerialiser::Construct(const char* lpcFilename)
{
    mpFile           = std::fopen(lpcFilename, "w");
    muRecursionDepth = 0;
}

void TextFileWriteSerialiser::Destruct()
{
    CGS_ASSERT(muRecursionDepth == 0, "muRecursionDepth == 0");
    if (mpFile != nullptr)
        std::fclose(mpFile);
}

void TextFileWriteSerialiser::FormatName(char* lpcDest, const char* lpcSource)
{
    const u32 luNumPrefixChars = 4u * muRecursionDepth;
    const u32 luSourceLength   = static_cast<u32>(strlen(lpcSource));

    u32 luLoop = 0;
    for (; luLoop < luNumPrefixChars; ++luLoop)
        lpcDest[luLoop] = '_';

    for (; luLoop < luSourceLength + luNumPrefixChars; ++luLoop)
    {
        const char lcChar = lpcSource[luLoop - luNumPrefixChars];
        lpcDest[luLoop] = (lcChar == ' ') ? '_' : lcChar;
    }

    lpcDest[luLoop] = '\0';
}

// ----------------------------------------------------------------------------
// The leaf writers. Each formats the label into a 64-byte buffer and writes one line, only
// when the file opened.
// ----------------------------------------------------------------------------
void TextFileWriteSerialiser::Serialise(const char* lpcName, f32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %f\n", lacBuffer, lrValue);
    }
}

void TextFileWriteSerialiser::Serialise(const char* lpcName, s32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %d\n", lacBuffer, lrValue);
    }
}

void TextFileWriteSerialiser::Serialise(const char* lpcName, u32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %d\n", lacBuffer, static_cast<s32>(lrValue));
    }
}

void TextFileWriteSerialiser::Serialise(const char* lpcName, Utils::VersionNumber& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %d\n", lacBuffer, static_cast<s32>(lrValue.muVersion));
    }
}

void TextFileWriteSerialiser::Serialise(const char* lpcName, Utils::FOV& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %f\n", lacBuffer, lrValue.mfFOV);
    }
}

void TextFileWriteSerialiser::Serialise(const char* lpcName, bool& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        FormatName(lacBuffer, lpcName);
        std::fprintf(mpFile, "%s : %d\n", lacBuffer, static_cast<s32>(lrValue));
    }
}

// ----------------------------------------------------------------------------
// Serialise(const char*, Vector3&) -- for each component build "<name><suffix>" in a 64-byte
// StrStream label (a null name streams as "<NULLSTRING>"), then write that component as one
// float line under the formatted label.
// ----------------------------------------------------------------------------
void TextFileWriteSerialiser::Serialise(const char* lpcName, Vector3& lrValue)
{
    char              lacLabel[KI_CHARBUFFERLENGTH];
    CgsDev::StrStream lLabel(lacLabel, sizeof(lacLabel));
    const char*       lpcSafeName = lpcName ? lpcName : "<NULLSTRING>";
    const f32* const  lapfLane[3] = { &lrValue.x, &lrValue.y, &lrValue.z };

    for (s32 liAxis = 0; liAxis < 3; ++liAxis)
    {
        lLabel.Reset();
        lLabel << lpcSafeName;
        lLabel << KAPC_COMPONENT_SUFFIX[liAxis];

        if (mpFile != nullptr)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];
            FormatName(lacBuffer, lLabel.GetBuffer());
            std::fprintf(mpFile, "%s : %f\n", lacBuffer, *lapfLane[liAxis]);
        }
    }
}

} // namespace Camera
} // namespace BrnDirector
