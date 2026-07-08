#include "exception.hpp"

#include <stdlib.h> // malloc / free
#include <string.h> // strlen / strcpy_s

//
// std::exception — Microsoft/EA CRT base (X360 build).
// Ground truth: X360 ARTIST asm.
//
namespace std {

// The X360 compiler emitted only a "vector deleting destructor" thunk
// (0x82C08CB8); its body is the real base destructor (reset vtable, free the
// owned string) plus the scalar-delete tail.  We reconstruct the logical
// destructor and let the compiler regenerate the deleting thunk/vtable.
exception::~exception() noexcept
{
    if (miDoFree)
    {
        free(const_cast<char*>(mpWhat));
    }
}

// Copy constructor (0x82C08C08).  A non-owning source (miDoFree == 0) shares
// the literal message pointer; an owning source is deep-copied via malloc +
// strcpy_s (strlen was inlined in the X360 asm).
exception::exception(const exception& rOther) noexcept
    : mpWhat(nullptr)
    , miDoFree(rOther.miDoFree)
{
    if (!miDoFree)
    {
        mpWhat = rOther.mpWhat;
    }
    else if (rOther.mpWhat)
    {
        const size_t nSize = strlen(rOther.mpWhat) + 1;
        char* pBuf = static_cast<char*>(malloc(nSize));
        mpWhat = pBuf;
        if (pBuf)
        {
            strcpy_s(pBuf, nSize, rOther.mpWhat);
        }
    }
    else
    {
        mpWhat = nullptr;
    }
}

// what() (0x82C08CA0).
const char* exception::what() const noexcept
{
    if (!mpWhat)
    {
        return "Unknown exception";
    }
    return mpWhat;
}

} // namespace std
