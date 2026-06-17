#pragma once

#include "types.hpp"

// CgsDev::MapFile::Reader - the abstract function-map reader the assert manager holds (the X360
// off_83018F20 vtable). Prepare hands it the map filename + the captured call-stack; Update advances an
// incremental load a slice at a time; GetStackEntryName returns the resolved function name for a frame.
// Decompiled from the DWARF (CgsMapFileReader.h) + the X360 callers (DoAssert / DisplayAssertScreen).

namespace CgsDev
{
    struct StackUnpickBase;

namespace MapFile
{
    struct Reader
    {
        Reader();
        virtual ~Reader() {}

        // vtable[0] (DoAssert: (**reader)(reader, name, stack)) - latch the call-stack to resolve.
        virtual void        Prepare(const char* lpcMapFileName, StackUnpickBase* lpCallstack);
        // vtable[1] (DisplayAssertScreen: (*(*reader+4))(reader)) - advance an incremental load.
        virtual void        Update();
        // vtable[2] (DoAssert/DisplayCallstack: (*(*reader+8))(reader, i)) - name of frame i, or null.
        virtual const char* GetStackEntryName(s32 liIndex);

    protected:
        StackUnpickBase* mpCallstack;
    };
}
}
