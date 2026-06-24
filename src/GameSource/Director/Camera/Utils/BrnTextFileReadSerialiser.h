#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H

#include "types.hpp"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (the label buffer)

#include <cstdio>   // FILE (the serialiser reads from a text file handle held by the caller)

// ============================================================================
// GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h
//
// BrnDirector::Camera::TextFileReadSerialiser -- the text-file READ serialiser the camera-rig
// parameter system drives when loading a vec3 parameter from a human-readable tunings file. It
// builds a per-component label ("<name>" + a component suffix) into a fixed stack buffer through
// a CgsDev::StrStream sink, then asks a per-component reader (Serialise<0/1/2>) to parse that
// labelled value out of the file into the matching float.
//
// HOME for the class slice owned by this TU:
//   - TextFileReadSerialiser::Serialise(FILE**, const char* name, f32* pVec3) @0x82219AE0
//     (the dispatcher: builds the x/y/z labels and forwards to the three per-component readers)
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// The text-file read serialiser. The X360 object built on the stack lays out exactly like
// CgsDev::StrStream (vtable@0, mePrintMode@4, mpcBuffer@8, miBufferSize@0xC, then a 0x40-byte
// label buffer), so the label-building part of Serialise reuses CgsDev::StrStream by value over
// a caller-supplied buffer. The leading construction phase the asm renders as
// BasePriorityQueue::Clear(&this) is the base-subobject reset that precedes installing the
// StrStream sink vtable; it clears the same head bytes the StrStream ctor then re-initialises.
class TextFileReadSerialiser
{
public:
    // Read a vec3 parameter named `lpcName` from the text file `*lppFile` into `lpfVec3` (the
    // three components in order). @0x82219AE0. For each component i in {0,1,2}: reset the label
    // buffer, stream the name (substituting "<NULLSTRING>" for null), stream the component
    // suffix, then call the per-component reader Serialise<i>. Returns the result of the last
    // (z) reader.
    int Serialise(FILE** lppFile, const char* lpcName, f32* lpfVec3);
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H
