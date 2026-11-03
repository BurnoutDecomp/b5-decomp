#ifndef CGS_LOG_OUTPUT_H
#define CGS_LOG_OUTPUT_H

#include "GameShared/GameClasses/Development/CgsStrStream.h"

namespace CgsDev
{
namespace Log
{
class LogOutput : public StrStreamBase
{
protected:
    virtual void Append(const char* lpcText);
};
}
}
#endif
