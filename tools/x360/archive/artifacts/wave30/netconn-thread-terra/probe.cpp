typedef unsigned long DWORD;
extern "C" void worker(void*);
extern "C" DWORD __stdcall candidate(void* p)
{
    worker(p);
    __asm { lwz r1,0(r1) }
    __asm { lwz r12,-8(r1) }
    __asm { mtlr r12 }
    __asm { blr }
}
