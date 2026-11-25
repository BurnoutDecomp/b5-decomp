#pragma once

#include "types.hpp"

// DecFIGS CgsNonConstructedClassContainer.h:36 and the LegacySrc header describe
// a 16-aligned byte array with typed access. ARTIST 0x825A312C copies the entire
// InAddRigidBody event, including this unconstructed payload, as storage.
template <class T>
class NonConstructedClassContainer
{
public:
    void operator=(T lSource)
    {
        *GetObjectPointer() = lSource;
    }

    const T* GetObjectPointer() const
    {
        return reinterpret_cast<const T*>(maData);
    }

    T* GetObjectPointer()
    {
        return reinterpret_cast<T*>(maData);
    }

private:
    CGS_ALIGN(16) u8 maData[sizeof(T)];
};
