// DirtySDK NetPrintWrapCode, ARTIST 0x82B30020.
#include "types.hpp"
#include "dirtylib.h"
#include <cstring>

extern "C" void NetPrintWrapCode(const char* text, s32 width)
{
    char line[256] = { ' ', ' ', ' ' };
    bool done = false;
    do
    {
        const char* end = text + width;
        const char* cursor = text;
        const char* whitespace = 0;
        const char* equals = 0;
        while (cursor < end)
        {
            if (*cursor == ' ' || *cursor == '\t') whitespace = cursor;
            if (*cursor == '=') equals = cursor;
            if (*cursor == '\n' || !*cursor) break;
            ++cursor;
        }
        if (cursor == end)
        {
            if (whitespace) cursor = whitespace;
            else if (equals) cursor = equals;
        }
        const s32 count = static_cast<s32>(cursor - text) + 1;
        std::strncpy(line + 3, text, count);
        if (!*cursor)
        {
            line[count + 2] = '\n';
            line[count + 3] = 0;
            done = true;
        }
        else if (*cursor != '\n' && *cursor != '\r')
        {
            line[count + 3] = '\n';
            line[count + 4] = 0;
        }
        else line[count + 3] = 0;
        NetPrintfCode("%s", line);
        text = cursor + 1;
    } while (!done);
}
