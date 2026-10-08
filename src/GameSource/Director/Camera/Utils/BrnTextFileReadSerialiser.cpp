// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.cpp
//
// The out-of-line members of BrnDirector::Camera::TextFileReadSerialiser: Construct /
// Destruct, the leaf readers, the vec3 reader and its per-component reader template.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"            // Utils::VersionNumber / Utils::FOV
#include "GameShared/GameClasses/Development/CgsStrStream.h"         // CgsDev::StrStream (the vec3 label sink)

namespace BrnDirector
{
namespace Camera
{

// The per-component label suffixes the vec3 leaf appends to the field name.
static const char* const KAPC_COMPONENT_SUFFIX[3] = { ": x", ": y", ": z" };

void TextFileReadSerialiser::Construct(const char* lpcFilename)
{
    mpFile = std::fopen(lpcFilename, "r");
}

void TextFileReadSerialiser::Destruct()
{
    if (mpFile != nullptr)
        std::fclose(mpFile);
}

// ----------------------------------------------------------------------------
// The leaf readers. Each consumes one "<label> : <value>\n" line when the file is open; the
// label token lands in a scratch buffer and is discarded. The scanned value goes straight
// into the field, so a line that fails to parse leaves the field as it was.
// ----------------------------------------------------------------------------
void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, f32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        std::fscanf(mpFile, "%s : %f\n", lacBuffer, &lrValue);
    }
}

void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, u32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        std::fscanf(mpFile, "%s : %d\n", lacBuffer, &lrValue);
    }
}

void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, s32& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        std::fscanf(mpFile, "%s : %d\n", lacBuffer, &lrValue);
    }
}

// The flag is scanned as an int and stored as (value != 0). The console scans into an
// uninitialised stack word; it is seeded with the current flag here so a failed scan cannot
// store an indeterminate value.
void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, bool& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        s32  liValue = lrValue ? 1 : 0;
        std::fscanf(mpFile, "%s : %d\n", lacBuffer, &liValue);
        lrValue = (liValue != 0);
    }
}

void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, Utils::VersionNumber& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        std::fscanf(mpFile, "%s : %d\n", lacBuffer, &lrValue.muVersion);
    }
}

void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, Utils::FOV& lrValue)
{
    if (mpFile != nullptr)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        std::fscanf(mpFile, "%s : %f\n", lacBuffer, &lrValue.mfFOV);
    }
}

// ----------------------------------------------------------------------------
// Serialise(const char*, Vector3&) -- for each component build "<name><suffix>" in a 64-byte
// StrStream label (a null name streams as "<NULLSTRING>"), then read that component through
// Serialise<INDEX>.
// ----------------------------------------------------------------------------
void TextFileReadSerialiser::Serialise(const char* lpcName, Vector3& lrValue)
{
    char              lacLabel[KI_CHARBUFFERLENGTH];
    CgsDev::StrStream lLabel(lacLabel, sizeof(lacLabel));
    const char*       lpcSafeName = lpcName ? lpcName : "<NULLSTRING>";

    lLabel.Reset();
    lLabel << lpcSafeName;
    lLabel << KAPC_COMPONENT_SUFFIX[0];
    Serialise<0>(lLabel.GetBuffer(), lrValue);

    lLabel.Reset();
    lLabel << lpcSafeName;
    lLabel << KAPC_COMPONENT_SUFFIX[1];
    Serialise<1>(lLabel.GetBuffer(), lrValue);

    lLabel.Reset();
    lLabel << lpcSafeName;
    lLabel << KAPC_COMPONENT_SUFFIX[2];
    Serialise<2>(lLabel.GetBuffer(), lrValue);
}

// ----------------------------------------------------------------------------
// Serialise<INDEX> -- one "<label> : <float>\n" line into lane INDEX. The current lane value
// is the scan default, so a failed scan rewrites the lane unchanged.
// ----------------------------------------------------------------------------
template<s32 INDEX>
void TextFileReadSerialiser::Serialise(const char* /*lpcName*/, Vector3& lrVector)
{
    if (mpFile != nullptr)
    {
        f32& lrLane = (INDEX == 0) ? lrVector.x : (INDEX == 1) ? lrVector.y : lrVector.z;

        char lacBuffer[KI_CHARBUFFERLENGTH];
        f32  lfTemp = lrLane;
        std::fscanf(mpFile, "%s : %f\n", lacBuffer, &lfTemp);
        lrLane = lfTemp;
    }
}

template void TextFileReadSerialiser::Serialise<0>(const char*, Vector3&);
template void TextFileReadSerialiser::Serialise<1>(const char*, Vector3&);
template void TextFileReadSerialiser::Serialise<2>(const char*, Vector3&);

} // namespace Camera
} // namespace BrnDirector
