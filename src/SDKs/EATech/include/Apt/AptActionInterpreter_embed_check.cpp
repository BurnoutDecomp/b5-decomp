// Tiny embed/ODR check for AptActionInterpreter.h (self-contained from a foreign TU).
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"

AptValue* AptActionInterpreter_EmbedCheckEntry(AptActionInterpreter* vm, AptValue* v)
{
    vm->stackPush(v);
    return vm->stackGetPop();
}
