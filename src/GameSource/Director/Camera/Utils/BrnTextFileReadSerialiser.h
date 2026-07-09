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

    // The nested-block reader-visitor template (X360: `FILE* Serialise<T>(const char*, T&)`). One
    // shared body per instance: consume+discard the field's section-header label line, then recurse
    // into T's own Serialise (which reads T's fields back). The X360 compiler folded the header-line
    // read + the field reads together for some leaf blocks (AttachmentTruck/FixedCam); the source is
    // this de-inlined delegating form. Body + explicit instantiations live in the .cpp; each T
    // supplies `template<class S> void Serialise(S&)` (attested as a separate X360 function).
    template<class T> void Serialise(const char* lpcName, T& lrParams);

    // The per-component vec-float reader (X360 Serialise<0/1/2> @0x82213BA0/0x82213C30/0x82213CC0).
    // The source form is `Serialise<INDEX>(const char*, rw::math::vpu::VecFloatRef<INDEX>)` -- a
    // single-lane accessor over the destination vector; the X360 lowers the VecFloatRef to the raw
    // vector pointer it wraps (the reader loads `*lppVec`, then VMX load/insert/stores lane INDEX).
    // Reconstructed at that lowered level (semantic parity, matching the vec3 dispatcher's own f32**
    // model) so this TU does not have to pull the EATech VecFloatRef vocabulary in alongside the
    // game's rw:: aggregate types (they are separate reconstructions of the same namespace and would
    // clash). Reads one "<label> : <float>" line into lane INDEX of `*lppVec`, preserving the others.
    // Body + explicit instantiations (INDEX 0/1/2 == X/Y/Z) live in the .cpp.
    template<int INDEX>
    FILE* Serialise(const char* lpcLabel, f32** lppVec);

private:
    // The source text file handle the read serialiser wraps. The X360 passes the serialiser BY
    // POINTER and every reader loads the handle as `*this` (lwz r3,0(r3)); i.e. the object IS a
    // FILE holder. (The vec3 dispatcher above is reconstructed with the handle passed explicitly.)
    FILE* mpFile;
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_TEXT_FILE_READ_SERIALISER_H
