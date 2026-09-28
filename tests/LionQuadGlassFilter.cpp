#include <cstdio>
#include <cstdlib>
#include <cstring>

struct TextureName { const char* value; const char* Get() const { return value; } };
struct cParticleMaterial { TextureName mpTextureName; };
static int giLogs = 0;
#include "lionquad_filter.inc"

int main()
{
    _putenv_s("BRN_LIONQUAD_GLASS", GLASS_MODE ? "1" : "0");
    const char* names[] = {"GLINTERGLASS", "TWINKY", "BLAGUNA", "SQUARELIGHT", nullptr};
    int failed = 0;
    for (int i = 0; i < 5; ++i)
    {
        const int before = giLogs;
        Probe({{names[i]}});
        const bool expected = !GLASS_MODE || i < 2;
        if ((giLogs != before) != expected)
        {
            ++failed;
            std::printf("FAIL witness selection for %s\n", names[i] ? names[i] : "<null>");
        }
    }
    std::printf("LionQuadGlassFilter: 5 checks, %d failures (glass=%d)\n", failed, GLASS_MODE);
    return failed ? 1 : 0;
}
