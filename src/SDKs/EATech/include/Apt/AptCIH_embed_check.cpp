// Tiny embed/ODR check for AptCIH.h.
#include "SDKs/EATech/include/Apt/AptCIH.h"

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
static void AptCIH_EmbedCheck(AptCIH* p)
{
    (void)p->GetInstanceName();
    (void)p->GetCharacterInst();
    (void)p->GetDepth();
    (void)p->GetDisplayListParent();
    p->SetDisplayListNext(0);
    (void)p->GetCIHState();
    p->SetCIHState(1);
    (void)p->GetZombieCount();
    p->IncZombieCount();
    (void)p->IsInCtor();
    (void)p->GetCreatedOnFrame();
}

// FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
void AptCIH_EmbedCheckEntry(AptCIH* p)
{
    AptCIH_EmbedCheck(p);
}
