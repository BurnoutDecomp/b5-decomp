#pragma once

#include "CgsDataStructure.h"

namespace CgsModule
{
    // =====================================================================================
    // ⚠️ KNOWN, DELIBERATE DIVERGENCE FROM THE CONSOLE -- read before touching the virtual
    //    list, and read before writing any code that indexes a module vtable numerically.
    //
    // The console's CgsModule::Module has NO virtual destructor. Its virtual list starts at
    // Construct(), so on the X360 **slot 0 is Construct()**. That is what
    // BrnPhysics::PhysicsModule::Construct @0x825AE308 is doing at 0x825AE9F4:
    //     lwz r11, 0x230(r31)   ; the embedded mSimulationModule's vptr
    //     lwz r11, 0(r11)       ; slot 0
    //     addi r3, r31, 0x230
    //     mtctr r11 ; bctrl     ; == mSimulationModule.Construct()
    // The dwarfdump agrees (it prints base classes and virtual lists, and prints no dtor).
    //
    // THIS TREE ADDS `virtual ~Module() = default`, which puts the destructor at slot 0 and
    // shifts every other virtual down by one. That was reviewed in physics wave 5 and KEPT.
    // Why, explicitly:
    //   * It cannot cause the vtable-slot-0 class of bug (the sky-dome `Create`-at-slot-0
    //     defect). That bug needed TWO disagreeing declarations of one interface, with a
    //     reinterpret_cast between them. Here there is exactly one declaration: nothing in the
    //     tree re-declares CgsModule::Module, nothing reinterpret_casts to it, and no call site
    //     indexes a module vtable numerically (all verified by scan). Every derived class and
    //     every call site is compiled against THIS header, so the compiler picks the slots and
    //     they are self-consistent by construction.
    //   * Reconstructions of the console's slot-0 call write it BY NAME
    //     (`mSimulationModule.Construct()`), which is immune to the slot order.
    //   * Deleting it would make `delete` through a `Module*` undefined behaviour. Nothing does
    //     that today, but a PC-leaf shutdown path plausibly would, and that failure is silent.
    //   * The blast radius is ~28 derived classes across 56 files, for zero behavioural gain.
    //
    // ⛔ THE DIVERGENCE IS ONLY SAFE WHILE BOTH OF THESE HOLD. If either becomes false --
    //    a second/local declaration of this interface appears, or any code starts indexing a
    //    module vtable by slot number (a reconstructed dispatch table, a hand-built vptr, a
    //    cast from a foreign interface) -- then DELETE the destructor and re-shape the list to
    //    the console order below, because the off-by-one becomes live at that moment.
    //
    //    Console slot order:  0 Construct, 1 Prepare, 2 Release, 3 Destruct, 4 Update,
    //                         5 SetMultiThreaded, 6 LockForInput, 7 UnlockForInput,
    //                         8 LockForOutput, 9 UnlockForOutput, then the four protected
    //                         DataStructure hooks.
    // =====================================================================================
    class Module
    {
    public:
        //Module();
        //Module(const Module& other);
        virtual ~Module() = default;   // NOT ON THE CONSOLE -- see the banner above.

        virtual void Construct() = 0;
        virtual bool Prepare() = 0;
        virtual bool Release() = 0;
        virtual void Destruct() = 0;
        virtual void Update() = 0;
        virtual void SetMultiThreaded(bool isMultiThreaded) = 0;
        virtual void LockForInput() = 0;
        virtual void UnlockForInput() = 0;
        virtual void LockForOutput() = 0;
        virtual void UnlockForOutput() = 0;

    protected:
        bool mbIsNewModule = false;

        virtual bool DestroyInputDataStructure(DataStructure* pDataStructure) = 0;
        virtual bool DestroyOutputDataStructure(DataStructure* pDataStructure) = 0;
        virtual bool PrepareDataStructures(DataStructure* pInputDataStructure, DataStructure* pOutputDataStructure) = 0;
        virtual bool ReleaseDataStructures(DataStructure* pInputDataStructure, DataStructure* pOutputDataStructure) = 0;
    };
}
