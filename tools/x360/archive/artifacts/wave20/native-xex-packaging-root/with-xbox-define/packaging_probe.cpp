#include <xtl.h>

int main()
{
    volatile DWORD tick = GetTickCount();
    return tick == 0;
}
