from pathlib import Path
import hashlib
import json
import math
import random
import struct
import sys

parent = Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(parent / 'scratch/x360_sdk'))
sys.path.insert(0, str(parent / 'tools/re'))
import native_atomic_fixture as F
import x360rd

class LionCPU(F.AtomicCPU):
    def step(self, pc, word, nxt, depth):
        # stvewx: the effective address selects one 32-bit vector element.
        if word >> 26 == 31 and ((word >> 1) & 1023) == 199:
            source, ra, rb = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
            address = ((self.g(ra) if ra else 0) + self.g(rb)) & 0xffffffff
            F.put(self, address & ~3, self.vr[source][(address >> 2) & 3])
            return nxt
        return super().step(pc, word, nxt, depth)

def cpu(image):
    result = F.cpu(image)
    result.__class__ = LionCPU
    return result

out = Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-material-526adc/lion-rgba8-native1')
rows = json.loads((out / 'compile-results.json').read_text())['results']
paths = {Path(row['source']).name: Path(row['object']) for row in rows}
constructor = F.Coff(paths['rgba8_probe.cpp'], 'NativeRGBA8Construct')
copy = F.Coff(paths['rgba8_probe.cpp'], 'NativeRGBA8Copy')
quad = F.Coff(paths['BrnLionBlendRenderer.cpp'], '?QuadDraw@')
writer = F.Coff(paths['BrnLionBlendVertex.cpp'], '?Write@VertexIterator@LionBlendVertex@')
bundle = F.Bundle([quad, writer])
bundle.select('?QuadDraw@')
writer_entry = F.address(bundle, '?Write@VertexIterator@LionBlendVertex@')
original_export = json.loads(Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82282330.json').read_text())
original_build_uv = int(next(row['address'] for row in original_export['xrefs_from'] if row['name'] == 'BrnEffects::Utils::BuildUVs'), 16)
SELF, AUX = F.M.S.SELF, F.M.S.AUX
ITER, UVD, PART, POS, EMITTER, COLOR, DEST = [SELF + n for n in (0x100, 0x300, 0x500, 0x700, 0x900, 0xb00, 0xc00)]
VERTICES = AUX + 0x1000
rng = random.Random(0x82282330)
checks = {'stvewx_distinct_lane_fixture': 0, 'constructor_native_packing': 0, 'copy_exact_word': 0, 'whole_quad_color_writer_chain': 0}
errors = []

scale = struct.pack('>4f', 511, 511, 511, 255)
maximum = struct.pack('>4f', 255, 255, 255, 255)

def ptr(cpu, reg):
    return cpu.g(reg) & 0xffffffff

def vr_bytes(cpu, reg):
    return struct.pack('>4I', *cpu.vr[reg])

def put_gpr_pair(cpu, first, data):
    cpu.setg(first, int.from_bytes(data[:8], 'big'))
    cpu.setg(first + 1, int.from_bytes(data[8:], 'big'))

def run_checks():
    for lane in range(4):
        c = cpu(constructor)
        c.mem.wr(COLOR, bytes(16))
        c.vr[0] = (0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00)
        c.setg(7, COLOR)
        c.setg(8, lane * 4)
        word = x360rd.u32(0x8227c324)
        c.step(0x8227c324, word, 0x8227c328, 0)
        assert c.mem.rd(COLOR, 16) == bytes(lane * 4) + struct.pack('>I', c.vr[0][lane]) + bytes(12 - lane * 4)
        checks['stvewx_distinct_lane_fixture'] += 1
    for index in range(512):
        channels = bytes((index & 255, (index * 73 + 19) & 255,
                          (index * 151 + 7) & 255, (index * 199 + 233) & 255))
        surrounding = rng.randbytes(16)
        c = cpu(constructor)
        c.mem.wr(COLOR - 4, surrounding)
        c.setg(3, COLOR)
        for reg, value in enumerate(channels, 4):
            c.setg(reg, value)
        c.run(constructor.entry)
        actual = c.mem.rd(COLOR - 4, 16)

        # Run the original RGBA8 construction inlined after rw::RGBA's ABGR
        # intermediate. The exact ARTIST instructions assemble and store the
        # native word at SP+50, immediately before the first Write call.
        original = cpu(constructor)
        original.setg(31, int.from_bytes(channels[::-1], 'big'))
        original.setg(28, POS)
        original.setg(29, ITER)
        original.setg(30, UVD)
        original.vr[0] = (0, 0, 0, 0)
        original.mem.wr(POS, rng.randbytes(64))
        for pc in range(0x82282414, 0x82282478, 4):
            word = x360rd.u32(pc)
            if not original.exec_vector(pc, word, 'native_RGBA8_constructor'):
                original.step(pc, word, pc + 4, 0)
        native_word = original.mem.rd(ptr(original, 1) + 0x50, 4)
        if actual != surrounding[:4] + native_word + surrounding[8:] or native_word != channels:
            errors.append({'group': 'constructor', 'case': index,
                           'compiled': actual.hex(), 'original_word': native_word.hex()})
        checks['constructor_native_packing'] += 1

        c = cpu(copy)
        c.mem.wr(COLOR - 4, surrounding)
        c.mem.wr(DEST - 4, bytes(reversed(surrounding)))
        before = c.mem.rd(DEST - 4, 16)
        c.setg(3, DEST)
        c.setg(4, COLOR)
        c.run(copy.entry)
        if c.mem.rd(DEST - 4, 16) != before[:4] + surrounding[4:8] + before[8:] or c.mem.rd(COLOR - 4, 16) != surrounding:
            errors.append({'group': 'copy', 'case': index})
        checks['copy_exact_word'] += 1

    edge = [-1.0e20, -2.0, -0.0, 0.0, 1.0 / 511.0, 0.25, 0.5,
            1.0, 2.0, 1.0e20, 254.5 / 511.0, 254.0 / 255.0]
    for index in range(512):
        particle = bytearray(rng.randbytes(0x80))
        uv_data = bytearray(rng.randbytes(0x50))
        colors = [edge[(index + lane * 3) % len(edge)] if index < 192 else rng.uniform(-0.75, 1.25) for lane in range(4)]
        struct.pack_into('>4f', particle, 0x50, *colors)
        frame = [-3.75, -0.5, 0.0, 0.125, 1.5, 17.25][index % 6]
        next_frame = frame + 1.25
        struct.pack_into('>f', particle, 0x3c, frame)
        struct.pack_into('>f', particle, 0x4c, next_frame)
        struct.pack_into('>I', uv_data, 0x40, 2 if index % 2 else 0)
        positions = b''.join(struct.pack('>4f', float(n + 1), float(index - n), float(n * 7 - 9), -123.0) for n in range(4))
        uvs = b''.join(struct.pack('>4f', n / 8.0, 1.0 - n / 8.0, float(n + 2), float(11 - n)) for n in range(4))
        vertices = rng.randbytes(0x200)
        iterator = struct.pack('>4I', VERTICES - 4, VERTICES, VERTICES + 0x200, 36)
        outcomes = []
        for compiled in (False, True):
            c = cpu(bundle)
            # These dynamic-constant globals are zero in the pre-initialization
            # image. Supply matching provider inputs to both executions; this
            # fixture does not claim their static initializer is recovered.
            c.mem.add(0x82fac210, 32, scale + maximum)
            c.mem.wr(ITER, iterator)
            c.mem.wr(UVD, uv_data)
            c.mem.wr(PART, particle)
            c.mem.wr(POS, positions)
            c.mem.wr(VERTICES - 4, vertices)
            sp = ptr(c, 1)
            c.mem.wr(sp - 0x2000, bytes([index & 255]) * 0x2000)
            for reg, value in enumerate((ITER, UVD, PART, POS, EMITTER), 3):
                c.setg(reg, value)
            trace = []

            def build_uv(cpu):
                if compiled:
                    output = ptr(cpu, 8)
                    frame_arg = (cpu.g(4) & 0xffffffffffffffff).to_bytes(8, 'big') + (cpu.g(5) & 0xffffffffffffffff).to_bytes(8, 'big')
                    next_arg = (cpu.g(6) & 0xffffffffffffffff).to_bytes(8, 'big') + (cpu.g(7) & 0xffffffffffffffff).to_bytes(8, 'big')
                else:
                    output = ptr(cpu, 4)
                    frame_arg, next_arg = vr_bytes(cpu, 1), vr_bytes(cpu, 2)
                trace.append(('BuildUVs', ptr(cpu, 3), frame_arg.hex(), next_arg.hex()))
                cpu.mem.wr(output, uvs)

            def observe(pc, word):
                if pc == (writer_entry if compiled else 0x8227e568):
                    color_arg = ptr(c, 6 if compiled else 4)
                    trace.append(('Write', ptr(c, 3), c.mem.rd(color_arg, 4).hex(), color_arg % 4,
                                  c.mem.rd(VERTICES - 4, 0x200).hex()))

            c.instruction_observer = observe
            c.hooks[original_build_uv] = build_uv
            if compiled:
                for address, name in bundle.unresolved.items():
                    if name.startswith('?BuildUVs@'):
                        c.hooks[address] = build_uv
                    elif name.startswith('?K_VECFLOAT_255@'):
                        c.mem.wr(address, maximum)
                    elif name.startswith('?K_VECTOR4_511_511_511_255@'):
                        c.mem.wr(address, scale)
                    elif name == 'floor':
                        c.hooks[address] = lambda cpu: cpu.setf(1, math.floor(cpu.f(1)))
            c.run(bundle.entry if compiled else 0x82282330)
            assert sum(item[0] == 'Write' for item in trace) == 4
            outcomes.append((c.mem.rd(ITER, 16), c.mem.rd(VERTICES - 4, 0x200), c.mem.rd(PART, 0x80),
                             c.mem.rd(UVD, 0x50), c.mem.rd(POS, 64), trace))
        if outcomes[0] != outcomes[1]:
            errors.append({'group': 'whole_quad', 'case': index,
                           'color_bits': particle[0x50:0x60].hex(),
                           'vertex_differences': [n for n,(a,b) in enumerate(zip(outcomes[0][1], outcomes[1][1])) if a != b],
                           'traces': [outcome[-1] for outcome in outcomes]})
        checks['whole_quad_color_writer_chain'] += 1

if __name__ == '__main__':
    run_checks()
    result = {'checks': checks, 'errors': errors,
              'objects': {name: {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for name,path in paths.items()},
              'scope': 'Actual shared RGBA8 four-u8 constructor and single-word copy; ARTIST82282414..82282474 inline packing/store; complete actual compiled QuadDraw plus actual compiled VertexIterator::Write versus ARTIST82282330 -> 8227E568 -> 8227C2E8 on valid stride/capacity paths.',
              'limits': ['BuildUVs is controlled and the external floor service is controlled; both real color conversion/packing and all four real writer bodies execute.',
                         'Scale/maximum dynamic-constant globals are matching supplied inputs (511,511,511,255) and (255,255,255,255); original static initialization is outside this fixture.',
                         'The existing compiled POD-vector GPR ABI is normalized only at the controlled BuildUVs service boundary. This is a data/store comparison, not proof that the unresolved shared VPU by-value ABI matches retail.',
                         'Default RGBA8 constructor remains declaration-only; no unverified default initialization body is added or consumed.',
                         'Finite color inputs are claimed. Earlier expanded NaN-color probe retained in lion-rgba8-comparison.json has64 NaN discrepancies in unchanged scalar conversion; no production math changed.',
                         'No invalid iterator/assert-path, strict-link, GPU or boot claim.']}
    report = out / 'lion-rgba8-finite-comparison.json'
    assert not report.exists(), 'Refusing to overwrite completed evidence'
    report.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'checks':checks, 'errors':len(errors), 'first_error':errors[:1]}, indent=2))
    raise SystemExit(bool(errors))
