#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (the vec3 leaf overload)

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
// Bodies: Construct / Destruct / every leaf overload / the vec3 overload / the per-component
// reader template are in BrnTextFileReadSerialiser.cpp; the nested-block template is inline
// below (the console emits it per T).
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

    // ---- leaf readers: one "<label> : <value>\n" line each, read straight into the field ----
    void Serialise(const char* lpcName, f32& lrValue);                    // "%s : %f\n"
    void Serialise(const char* lpcName, u32& lrValue);                    // "%s : %d\n"
    void Serialise(const char* lpcName, s32& lrValue);                    // "%s : %d\n"
    void Serialise(const char* lpcName, bool& lrValue);                   // "%s : %d\n", stored as != 0
    void Serialise(const char* lpcName, Utils::VersionNumber& lrValue);   // "%s : %d\n" into the tag
    void Serialise(const char* lpcName, Utils::FOV& lrValue);             // "%s : %f\n" into the FOV

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
        if (mpFile != nullptr)
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
