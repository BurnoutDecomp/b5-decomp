#include <EABase/eabase.h>
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h"
#include <cstdio>

bool8_t PCLionFirstEcho(bool8_t value);
unsigned PCLionFirstBoolSize();

int main()
{
    volatile bool8_t inputs[] = {false, true};
    const bool off = !PCLionFirstEcho(inputs[0]);
    const bool on = PCLionFirstEcho(inputs[1]);
    const bool size = PCLionFirstBoolSize() == sizeof(bool8_t) && sizeof(bool8_t) == 1;
    const unsigned failures = !off + !on + !size;
    std::printf("PCLionBoolABI: 3 checks, %u failures\n", failures);
    return failures ? 1 : 0;
}
