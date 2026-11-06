"""Read-only OnlineSelectRoute distance-window negative control.

Run with Python -B from any directory. The default object is the retained scalar
object; expected ARTIST/native results are 43480000/4347FFFF for the fixed witness.
No build, source edit, report write or whole-function comparison is performed.

Reuse for a repaired object: import this module, construct its real F.Bundle,
then call run_window(bundle, start, stop, setup, result_kind, result_register).
The GUI owner must bind start/stop and setup to that object's actual instructions.
setup_legacy is ONLY the retained scalar object's register/stack binding; do not
assume it describes a repaired VMX object. run_original uses the original binding.

Model limits: vrsqrtefp is the shared exact-then-f32-rounded reciprocal-sqrt model,
not the physical Xenon estimate table. ARTIST's actual two refinement iterations
execute. vmsum3fp uses the shared ROUNDING_RULE1 model (f64 products/sum, one f32
cast); fused VMX operations use the shared model. The local fsqrts model uses
math.sqrt followed by f32 rounding. This finite-positive witness does not certify
estimates, NaN payloads, exceptional inputs, full callers, icon queues or hardware.
"""

import argparse
import hashlib
import inspect
import json
import math
from pathlib import Path
import struct
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
WORKFLOW = HERE.parents[2]
SHARED = HERE.parent
sys.path.insert(0, str(SHARED))

# Existing helper imports contain idempotent mkdir(exist_ok=True) calls. Require
# their directories so importing this evidence cannot create them on a new host.
for directory in ['native-render-validation', 'native-resource-pointer-validation']:
    if not (WORKFLOW / 'b5-decomp/build/x360' / directory).is_dir():
        raise RuntimeError(f'Existing {directory} fixture directory required; no directories created')
import native_atomic_fixture as F

POINT_BITS = (0xC18A14F2, 0xC2E26F40, 0xC323F694)
LEGACY_SHA256 = '569718c363fd626d8f788d27853107592f3854e72d3b5df0bd6a2aab30cc2d3b'
LEGACY_OBJECT = WORKFLOW / 'b5-decomp/build/x360/parallel-wave3-fixed/BrnOnlineSelectRoute-db6a5389.obj'
SYMBOL = '?UpdateAfterToggleChange@OnlineSelectRoute@'
ORIGINAL_START, ORIGINAL_STOP = 0x8249E6C4, 0x8249E73C


def bits(value):
    return struct.unpack('>I', struct.pack('>f', value))[0]


def value(word):
    return struct.unpack('>f', struct.pack('>I', word))[0]


class RouteWindowCPU(F.AtomicCPU):
    def exec_vector(self, pc, word, mnemonic):
        # CPU.run dispatches vector instructions before step, so stop here.
        if pc == self.window_end:
            raise F.CPU.run.__globals__['Return']()
        return super().exec_vector(pc, word, mnemonic)

    def step(self, pc, word, nxt, depth):
        if word >> 26 == 59 and ((word >> 1) & 31) == 22:  # fsqrts
            operand = self.f((word >> 11) & 31)
            result = math.sqrt(operand) if operand >= 0 else float('nan')
            self.setf((word >> 21) & 31, value(bits(result)))
            return nxt
        return super().step(pc, word, nxt, depth)


def new_cpu(bundle):
    cpu = F.cpu(bundle)
    cpu.__class__ = RouteWindowCPU
    return cpu


def setup_original(cpu):
    # ARTIST8249E648..688 names the peer vector and zero/half/one constants.
    cpu.mem.wr(F.M.S.SELF, struct.pack('>4I', *POINT_BITS, 0))
    cpu.setg(30, F.M.S.SELF)
    cpu.setg(22, 0)
    cpu.setg(29, 0x82050000)  # ha of original 200.0 constant8204CA54
    cpu.vr[124] = (0, 0, 0, 0)  # peer position
    cpu.vr[125] = (bits(0.5),) * 4
    cpu.vr[126] = (bits(1.0),) * 4
    cpu.vr[127] = (0, 0, 0, 0)


def setup_legacy(cpu):
    # Retained scalar object: point r30, peer at sp+110, threshold f31.
    cpu.mem.wr(F.M.S.SELF, struct.pack('>4I', *POINT_BITS, 0))
    cpu.setg(30, F.M.S.SELF)
    cpu.mem.wr(cpu.g(1) + 0x110, bytes(16))
    cpu.setf(31, 200.0)


def run_window(bundle, start, stop, setup, result_kind='v', result_register=0):
    """Execute actual instructions with explicit caller-provided window binding.

    stop is exclusive. result_kind is 'v' (vector lane0) or 'f' (scalar FPR).
    """
    cpu = new_cpu(bundle)
    cpu.window_end = stop
    setup(cpu)
    cpu.run(start)
    distance = cpu.vf(result_register)[0] if result_kind == 'v' else cpu.f(result_register)
    return dict(value=float(distance), bits=f'{bits(distance):08X}', within_200=bool(distance < 200.0))


def run_original(bundle):
    return run_window(bundle, ORIGINAL_START, ORIGINAL_STOP, setup_original)


def model_files():
    # Bind the actual shared files currently loaded, rather than a copied model.
    paths = {Path(F.__file__), Path(F.M.__file__), Path(F.M.V.__file__),
             SHARED / 'validate_resource_pointer.py', SHARED / 'validate_native_strings.py'}
    paths.update(Path(inspect.getsourcefile(cls)) for cls in F.AtomicCPU.__mro__ if cls is not object)
    return [dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(paths)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--object', type=Path, default=LEGACY_OBJECT,
                        help='Actual retained scalar COFF. For repaired bindings import run_window instead.')
    args = parser.parse_args()
    digest = hashlib.sha256(args.object.read_bytes()).hexdigest()
    if digest != LEGACY_SHA256:
        raise RuntimeError('Default native offsets/setup are bound only to the retained scalar object SHA; import run_window for a repaired object')
    assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
    bundle = F.Bundle([F.Coff(args.object, SYMBOL)])
    bundle.select(SYMBOL)
    original = run_original(bundle)
    native = run_window(bundle, bundle.entry + 0x144, bundle.entry + 0x178, setup_legacy, 'f', 0)
    assert (original['bits'], native['bits']) == ('43480000', '4347FFFF'), (original, native)
    assert not original['within_200'] and native['within_200']
    print(json.dumps(dict(
        point_bits=[f'{x:08X}' for x in POINT_BITS], peer=[0, 0, 0],
        original=original, retained_native=native,
        original_window=['8249E6C4', '8249E73C exclusive'],
        native_window=['entry+144', 'entry+178 exclusive'],
        object=str(args.object), object_sha256=digest, model_files=model_files(),
        limits='Shared exact-then-rounded reciprocal estimate and RULE1 dot model; local math.sqrt-to-f32 fsqrts; actual instruction windows only, no hardware/full-function claim'
    ), indent=2))


if __name__ == '__main__':
    main()
