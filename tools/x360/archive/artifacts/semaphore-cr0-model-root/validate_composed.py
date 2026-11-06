"""Recheck the semaphore pair with isolated Xenon word-sized CR0 recording.

No shared emulator or source file is changed. Prior 18-mismatch evidence remains.
Reference: Xenia PPCHIRBuilder::UpdateCR(single value) truncates to INT32, and
InstrEmit_addx invokes that path for the record bit. Full GPR results stay 64-bit.
"""
from pathlib import Path
import hashlib, json, random, struct, sys

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
PRIOR = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave6/replay-provider-terra')
sys.path.insert(0, str(PARENT / 'scratch/x360_sdk'))
import native_atomic_fixture as F

class WordRecordCPU(F.AtomicCPU):
    def step(self, pc, word, nxt, depth):
        if word >> 26 == 31 and word & 1 and ((word >> 1) & 511) in (40, 104, 266):
            operation = (word >> 1) & 511
            dest, ra, rb = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
            value = (self.g(rb)-self.g(ra) if operation == 40 else
                     -self.g(ra) if operation == 104 else self.g(ra)+self.g(rb)) & 0xffffffffffffffff
            self.setg(dest, value)
            signed = (value & 0x7fffffff) - (value & 0x80000000)
            self.setcr(0, signed < 0, signed > 0, signed == 0)
            return nxt
        return super().step(pc, word, nxt, depth)

report = json.loads((PRIOR / 'native/compile-results.json').read_text())
objects = {Path(row['source']).name: Path(row['object']) for row in report['results'] if row['passed']}
bundle = F.Bundle([F.Coff(objects['eathread_semaphore.cpp'], '?Post@Semaphore@'),
                   F.Coff(objects['eathread_semaphore_data.cpp'], '?UpdateCancelCount@EASemaphoreData@')])
bundle.select('?Post@Semaphore@')
assert F.M.V.x360rd.u32(0x82815570) == 0x7c8a2378
assert F.M.V.x360rd.u32(0x82f264a8) == 4
data = F.M.S.SELF
rng = random.Random(82642)
errors = []; cases = 0
for intra in (0, 1):
    for count in (-9, -2, -1, 0, 1, 8):
        for cancel in (-1, 0, 1, 5):
            for amount in (-2, 0, 1, 3, 10):
                for ok in (0, 1):
                    initial = bytearray(rng.randbytes(0x80))
                    struct.pack_into('>IiiB', initial, 0, 0x4003a000, count, cancel, intra)
                    both = []
                    for compiled in (False, True):
                        cpu = F.cpu(bundle); cpu.__class__ = WordRecordCPU
                        cpu.mem.wr(data, initial); cpu.setg(3, data); cpu.setg(4, amount & 0xffffffff)
                        trace = []
                        def release(c):
                            trace.append(tuple(c.g(i) & 0xffffffff for i in (3,4,5)))
                            c.setg(3, ok)
                        cpu.hooks[0x82c204b8] = release
                        for address, name in bundle.unresolved.items():
                            if name == 'ReleaseSemaphore': cpu.hooks[address] = release
                        cpu.run(bundle.entry if compiled else 0x82b42b70)
                        both.append((cpu.mem.rd(data, 0x80).hex(), trace, cpu.g(3) & 0xffffffff))
                    if both[0] != both[1]:
                        errors.append({'intra': intra, 'count': count, 'cancel': cancel,
                                       'amount': amount, 'osok': ok, 'original': both[0], 'compiled': both[1]})
                    cases += 1

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
result = {'cases': cases, 'mismatches': errors,
          'objects': {name: {'path': str(objects[name]), 'sha256': sha(objects[name])} for name in
                      ('eathread_semaphore.cpp', 'eathread_semaphore_data.cpp')},
          'prior_failures': str(PRIOR / 'post-composed-comparison.json'),
          'prior_failures_sha256': sha(PRIOR / 'post-composed-comparison.json'),
          'shared_fixture': str(PARENT / 'scratch/x360_sdk/native_atomic_fixture.py'),
          'shared_fixture_sha256': sha(PARENT / 'scratch/x360_sdk/native_atomic_fixture.py'),
          'references': [
              'https://github.com/xenia-project/xenia/blob/master/src/xenia/cpu/ppc/ppc_hir_builder.cc#L362-L364',
              'https://github.com/xenia-project/xenia/blob/master/src/xenia/cpu/ppc/ppc_emit_alu.cc#L42-L52'],
          'method': 'Isolated subclass records add./subf./neg. CR0 from the low signed word; '
                    'the full 64-bit GPR result remains unchanged. No source/helper/native object edits.',
          'limits': 'Software model corroborated against Xenia source; real kernel release is controlled. '
                    'No concurrency/contention, hardware, all-emulator or full program claim.',
          'validator_sha256': sha(__file__)}
with (HERE / 'composed-word-cr0-comparison.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print(json.dumps({'cases': cases, 'mismatches': len(errors),
                  'report_sha256': sha(HERE / 'composed-word-cr0-comparison.json')}, indent=2))
assert not errors
