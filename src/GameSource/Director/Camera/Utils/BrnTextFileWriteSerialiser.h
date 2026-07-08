#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_WRITE_SERIALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_WRITE_SERIALISER_H

#include "types.hpp"

#include <cstdio>   // std::FILE (the serialiser writes to a text file handle held by it)

// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h
//
// BrnDirector::Camera::TextFileWriteSerialiser -- the text-file WRITE serialiser the
// camera-rig parameter system drives when saving tunings to a human-readable text file.
// It is the write counterpart of TextFileReadSerialiser.
//
// SHAPE authoritative from the DecFIGS DWARF
//   (references/DecFIGS/dwarfdump/.../Serialisation.h:161):
//       struct TextFileWriteSerialiser {
//       private:
//           uint32_t   muRecursionDepth;   // +0x00
//           std::FILE* mpFile;             // +0x04
//       public:  Construct(const char*); Destruct(); Serialise(...) overloads;
//       private: void FormatName(char*, const char*);
//       };
// The +0x00 recursion-depth word and the FormatName store-for-store body are gated
// against the X360 binary (FormatName @ 0x821F6128).
//
// HOME for the class slice owned by this TU:
//   - TextFileWriteSerialiser::FormatName(char* dest, const char* src) @0x821F6128
//     (writes 4*muRecursionDepth indent underscores into dest, then copies src with
//      spaces replaced by underscores, NUL-terminating). Called by every
//      Serialise<...> instance to build the per-field label.
// The Construct/Destruct/Serialise overloads are separate TUs; declared here so they can
// be called by name.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

class TextFileWriteSerialiser
{
public:
    // The per-field label builder. @0x821F6128.
    //   lpcDest : output buffer (the caller's fixed stack label buffer)
    //   lpcSrc  : the field name to format
    // Writes (4 * muRecursionDepth) leading '_' indent characters, then copies lpcSrc
    // into the buffer replacing every space (' ') with '_', and NUL-terminates. The
    // recursion depth governs how deeply nested fields are indented in the text file.
    void FormatName(char* lpcDest, const char* lpcSrc);

    // Open the destination text file `lpcFilename` (mode "w") and reset muRecursionDepth to 0.
    // @Serialisation.h:634 (DWARF `void Construct(const char*)`). Body is a separate TU;
    // declared here so callers can drive it by name -- e.g. BehaviourParameterBank::
    // SaveParameters, which the X360 compiler inlines this into.
    void Construct(const char* lpcFilename);

    // Assert the nesting fully unwound (CGS_ASSERT muRecursionDepth == 0 @Serialisation.h:643)
    // and close mpFile. @Serialisation.h:641 (DWARF `void Destruct()`). Body is a separate TU;
    // declared here for the same reason as Construct.
    void Destruct();

    // Write a single named f32 field as one "<formatted-name> : <value>\n" text line. @0x82208878.
    // If mpFile is open: build the indented/space-escaped label into a fixed stack buffer via
    // FormatName, then fprintf it followed by the float value (promoted to double for the varargs
    // "%f"). A no-op when the file failed to open.
    void Serialise(const char* lpcName, f32& lrValue);

private:
    u32        muRecursionDepth;   // +0x00 -- current nesting depth (indent = 4*depth)
    std::FILE* mpFile;             // +0x04 -- the destination text file handle
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_WRITE_SERIALISER_H
