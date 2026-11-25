#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"

#include <cstdio>   // FILE / fscanf (the nested-block template reads its header line inline)

// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h
//
// BrnDirector::Camera::TextFileReadSerialiser -- the text-file READ serialiser the camera
// tunings bank and the ICE playlists drive when loading a human-readable tunings file
// ("d:\\camera.txt" / the playlist file). The read counterpart of TextFileWriteSerialiser:
// every leaf consumes one "<label> : <value>\n" line, ignoring the label (only the value is
// used; the walk order is what matches lines to fields).
//
// Layout: one word, the wrapped FILE* (every reader loads it as `*this`).
//
// The scalar readers and nested-block template are inline. BrnTextFileReadSerialiser.cpp
// owns construction, destruction, and the vector readers.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// The versioned-block tag and the FOV wrapper (definitions in CameraUtils.h).
namespace Utils { struct VersionNumber; struct FOV; }

class TextFileReadSerialiser
{
public:
    // Fixed label-buffer length.
    static const s32 KI_CHARBUFFERLENGTH = 64;

    // Open `lpcFilename` for reading ("r"). Inlined into BehaviourParameterBank::LoadParameters
    // and DebugComponent::LoadPlaylists on the console.
    void Construct(const char* lpcFilename);

    // Close the file if it opened.
    void Destruct();

    // ARTIST expands the scalar readers at their call sites. The
    // label is consumed from the file rather than compared to the field name.
    void Serialise(const char* /*lpcName*/, f32& lrValue)
    {
        if (mpFile != 0)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];
            std::fscanf(mpFile, "%s : %f\n", lacBuffer, &lrValue);
        }
    }

    void Serialise(const char* /*lpcName*/, u32& lrValue)
    {
        if (mpFile != 0)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];
            std::fscanf(mpFile, "%s : %d\n", lacBuffer, &lrValue);
        }
    }

    void Serialise(const char* /*lpcName*/, s32& lrValue)
    {
        if (mpFile != 0)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];
            std::fscanf(mpFile, "%s : %d\n", lacBuffer, &lrValue);
        }
    }

    // The flag is scanned as an int and stored as (value != 0). The console scans into an
    // uninitialised stack word; it is seeded with the current flag here so a failed scan cannot
    // store an indeterminate value.
    void Serialise(const char* /*lpcName*/, bool& lrValue)
    {
        if (mpFile != 0)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];
            s32  liValue = lrValue ? 1 : 0;
            std::fscanf(mpFile, "%s : %d\n", lacBuffer, &liValue);
            lrValue = (liValue != 0);
        }
    }

    void Serialise(const char* /*lpcName*/, Utils::VersionNumber& lrValue)
    {
        if (mpFile != 0)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];
            std::fscanf(mpFile, "%s : %d\n", lacBuffer, &lrValue.muVersion);
        }
    }

    void Serialise(const char* /*lpcName*/, Utils::FOV& lrValue)
    {
        if (mpFile != 0)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];
            std::fscanf(mpFile, "%s : %f\n", lacBuffer, &lrValue.mfFOV);
        }
    }

    // The vec3 leaf: builds "<name>: x" / ": y" / ": z" labels and hands each component to the
    // per-component reader below.
    void Serialise(const char* lpcName, Vector3& lrValue);

    // The per-component reader: one "<label> : <float>\n" line into lane INDEX (0/1/2 == x/y/z)
    // of lrVector, the other lanes untouched. The console's argument is a
    // rw::math::vpu::VecFloatRef<INDEX> -- a lane reference into the vector -- which this
    // tree's rw::math::vpu vocabulary does not model; the reference is spelled as the vector
    // it refers into plus the lane template argument. Body + the three instances in the .cpp.
    template<s32 INDEX>
    void Serialise(const char* lpcName, Vector3& lrVector);

    // The nested-block reader: consume the section-header line the writer emitted, then read
    // T's own fields. One shared body; the console emits it per T.
    template<class T>
    void Serialise(const char* /*lpcName*/, T& lrParams)
    {
        if (mpFile != 0)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];   // the header token, read and discarded
            std::fscanf(mpFile, "%s\n", lacBuffer);
        }
        lrParams.Serialise(*this);
    }

private:
    std::FILE* mpFile;   // +0x00
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H
