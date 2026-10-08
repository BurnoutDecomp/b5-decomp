#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_WRITE_SERIALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_WRITE_SERIALISER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 (the vec3 leaf overload)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the nesting-depth guard in Serialise<T>)

#include <cstdio>   // FILE / fprintf (the nested-block template writes its header line inline)

// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h
//
// BrnDirector::Camera::TextFileWriteSerialiser -- the text-file WRITE serialiser the camera
// tunings bank and the ICE playlists drive when saving a human-readable tunings file. Every
// leaf writes one "<formatted-name> : <value>\n" line; nested blocks write a "<formatted-name>\n"
// header line and indent their fields one level deeper (FormatName prefixes 4 underscores per
// level and turns spaces into underscores).
//
// Layout (console member order):
//     u32        muRecursionDepth;   // +0x00
//     std::FILE* mpFile;             // +0x04
//
// Bodies: Construct / Destruct / FormatName / every leaf overload / the vec3 overload are in
// BrnTextFileWriteSerialiser.cpp; the nested-block template is inline below.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// The versioned-block tag and the FOV wrapper (definitions in CameraUtils.h).
namespace Utils { struct VersionNumber; struct FOV; }

class TextFileWriteSerialiser
{
public:
    // Fixed label-buffer length.
    static const s32 KI_CHARBUFFERLENGTH = 64;

    // Open `lpcFilename` for writing ("w") and reset the nesting depth. Inlined into
    // BehaviourParameterBank::SaveParameters and DebugComponent::SavePlaylists on the console.
    void Construct(const char* lpcFilename);

    // Assert the nesting fully unwound, then close the file if it opened.
    void Destruct();

    // ---- leaf writers: one "<formatted-name> : <value>\n" line each (no-op without a file) ----
    void Serialise(const char* lpcName, f32& lrValue);                    // "%s : %f\n"
    void Serialise(const char* lpcName, s32& lrValue);                    // "%s : %d\n"
    void Serialise(const char* lpcName, u32& lrValue);                    // "%s : %d\n"
    void Serialise(const char* lpcName, Utils::VersionNumber& lrValue);   // "%s : %d\n"
    void Serialise(const char* lpcName, Utils::FOV& lrValue);             // "%s : %f\n"
    void Serialise(const char* lpcName, bool& lrValue);                   // "%s : %d\n"

    // The vec3 leaf: three lines, labelled "<name>: x" / ": y" / ": z".
    void Serialise(const char* lpcName, Vector3& lrValue);

    // The nested-block writer: the "<formatted-name>\n" header line, then T's own fields one
    // nesting level deeper. One shared body; the console emits it per T.
    template<class T>
    void Serialise(const char* lpcName, T& lrParams)
    {
        if (mpFile != nullptr)
        {
            char lacBuffer[KI_CHARBUFFERLENGTH];
            FormatName(lacBuffer, lpcName);
            std::fprintf(mpFile, "%s\n", lacBuffer);
        }

        ++muRecursionDepth;
        CGS_ASSERT(muRecursionDepth < KI_MAX_RECURSION_DEPTH,
                   "muRecursionDepth < KI_MAX_RECURSION_DEPTH");

        lrParams.Serialise(*this);

        --muRecursionDepth;
    }

private:
    // The per-field label builder: (4 * muRecursionDepth) leading '_' characters, then
    // lpcSource with every space replaced by '_', NUL-terminated.
    void FormatName(char* lpcDest, const char* lpcSource);

    // Nesting cap of the depth guard.
    static const u32 KI_MAX_RECURSION_DEPTH = 8u;

    u32        muRecursionDepth;   // +0x00 -- current nesting depth (indent = 4 * depth)
    std::FILE* mpFile;             // +0x04 -- the destination text file handle
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_WRITE_SERIALISER_H
