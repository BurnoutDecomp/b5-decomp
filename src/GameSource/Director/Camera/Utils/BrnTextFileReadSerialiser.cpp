// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.cpp
//
// The out-of-line members of BrnDirector::Camera::TextFileReadSerialiser: Construct /
// Destruct, the vec3 reader and its per-component reader template.
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
    if (mpFile != 0)
        std::fclose(mpFile);
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
    if (mpFile != 0)
    {
        char lacBuffer[KI_CHARBUFFERLENGTH];
        f32 lfTemp = lrVector.GetComponent(INDEX).GetFloat();
        std::fscanf(mpFile, "%s : %f\n", lacBuffer, &lfTemp);
        lrVector.SetComponent(INDEX, lfTemp);
    }
}

template void TextFileReadSerialiser::Serialise<0>(const char*, Vector3&);
template void TextFileReadSerialiser::Serialise<1>(const char*, Vector3&);
template void TextFileReadSerialiser::Serialise<2>(const char*, Vector3&);

} // namespace Camera
} // namespace BrnDirector
