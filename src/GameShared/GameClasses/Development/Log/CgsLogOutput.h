#ifndef CGS_LOG_OUTPUT_H
#define CGS_LOG_OUTPUT_H

#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace CgsDev
{
namespace Log
{
class LogOutput : public StrStreamBase
{
public:
    using StrStreamBase::operator<<;
    StrStreamBase& operator<<(const char* lpcText)
    {
        Append(lpcText);
        return *this;
    }

    int Append(const char* lpcText);
};
}
}
#endif
