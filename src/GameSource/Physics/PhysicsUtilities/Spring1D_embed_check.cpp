// Spring1D embed/compile check. Includes the home + exercises the three bodied ledger funcs so
// the gate compiles header + bodies together. The struct is a flat run of nine f32 (no vptr),
// so its size is proven == 36 bytes (9 * 4); the asm indexes result[0..8] at a 4-byte stride.

#include "GameSource/Physics/PhysicsUtilities/Spring1D.h"

static_assert(sizeof(BrnPhysics::Spring1D) == 9 * sizeof(f32),
              "Spring1D == nine packed f32 (asm indexes result[0..8] at 4-byte stride)");

namespace
{
    void TouchSpring1D()
    {
        BrnPhysics::Spring1D lSpring;
        lSpring.Construct();
        lSpring.Prepare(/*desired*/ 2.0f, /*current*/ 2.0f, /*mass*/ 1.0f,
                        /*stiffness*/ 50.0f, /*dampening*/ 0.2f,
                        /*minStretch*/ 0.5f, /*maxStretch*/ 4.0f);
        lSpring.Update(/*dt*/ 1.0f / 60.0f);
    }
}
