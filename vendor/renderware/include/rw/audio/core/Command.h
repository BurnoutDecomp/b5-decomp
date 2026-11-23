#pragma once

namespace rw
{
namespace audio
{
namespace core
{

// The RenderWare System command header (DecFIGS system.h:96).
// ExecuteCommands passes the record to handler and advances by its returned size.
struct Command
{
    int (*handler)(Command *);
    void *pObject;
};

} // namespace core
} // namespace audio
} // namespace rw
