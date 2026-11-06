import json, random, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import validate_native_math as M
import validate_render_states as R
import emu64 as E

ROOT = Path(__file__).resolve().parents[2] / 'b5-decomp'
report = json.loads((ROOT/'build/x360/native-shared-headers/compile-results.json').read_text(encoding='utf-8'))
obj = Path(next(x['object'] for x in report['results'] if x['source'].endswith('/speed.cpp') and x['passed']))
coff = R.Coff(obj, 'ProbeSpeedResponder')


class CPU(R.CPU):
    def fetch(self, pc):
        if pc == 0x82251ab4:
            raise E.Return()
        return super().fetch(pc)


rng = random.Random(360)
special = [0,0x80000000,0x3f800000,0xbf800000,0x3f000000,0x40000000,1,0x80000001,
           0x00800000,0x80800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc01234]
cases = [(a,b) for a in special for b in special]
cases += [(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(300)]
errors = []
for index, (speed, maximum) in enumerate(cases):
    outputs = []
    for compiled in [False,True]:
        c = coff.cpu()
        c.__class__ = CPU
        c.mem.wr(M.S.SELF+0x42c, struct.pack('>I',speed))
        c.mem.wr(M.S.SELF+0x434, struct.pack('>I',maximum))
        c.setg(23,M.S.SELF)
        c.setg(25,M.S.AUX-0x5874)
        c.setf(30,M.V.x360rd.f32(0x82001cc0))
        c.setf(31,M.V.x360rd.f32(0x82001c98))
        c.setf(1,M.value(speed))
        c.setf(2,M.value(maximum))
        c.run(coff.entry if compiled else 0x82251a7c)
        outputs.append(c.f(1) if compiled else M.value(c.rd32(M.S.AUX)))
    if not M.same(*outputs):
        errors.append({'case':index,'speed':hex(speed),'maximum':hex(maximum),'outputs':[hex(M.bits(x)) for x in outputs]})
result = {'cases':len(cases),'mismatches':errors,'object':str(obj),'symbol':coff.fn[0],
          'target':'real SpeedResponder header compiled to PPC COFF versus ARTIST82251A7C..82251AB0',
          'limits':'shared instruction interpreter; finite/zero/infinity/NaN cases; no hardware or full caller execution; NaN payloads not compared',
          'game_booted':False}
out = ROOT/'build/x360/native-shared-validation'
out.mkdir(exist_ok=True)
(out/'speed-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('SpeedResponder:',len(cases),'cases,',len(errors),'mismatches')
if errors:
    print(json.dumps(errors[:6],indent=2))
raise SystemExit(bool(errors))
