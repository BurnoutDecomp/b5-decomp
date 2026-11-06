import json, random, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import validate_native_math as M
from validate_render_states import Coff
from validate_resource_pointer import Bundle

ROOT = Path(__file__).resolve().parents[2] / 'b5-decomp'
REPORT = ROOT / 'build/x360/native-game-state-validation-objects/compile-results.json'
OUT = ROOT / 'build/x360/native-game-state-validation'
OUT.mkdir(exist_ok=True)

def main():
    report = json.loads(REPORT.read_text(encoding='utf-8'))
    rows = {Path(r['source']).name:r for r in report['results']}
    training = Coff(Path(rows['BrnTrainingManager.cpp']['object']), '?Construct@TrainingManager@')
    achievement = Coff(Path(rows['BrnGameStateAchievementManagerBase.cpp']['object']), '?Construct@AchievementManagerBase@')
    bundle = Bundle([training, achievement])
    rng = random.Random(36028)
    cases, mismatches = 0, []
    for kind, prefix, original in [('training','?Construct@TrainingManager@',0x8238057c),
                                   ('achievement','?Construct@AchievementManagerBase@',0x8235a948)]:
        bundle.select(prefix)
        for i in range(128):
            seed = rng.randbytes(0x100)
            owners = [rng.getrandbits(32) or 1 for _ in range(4)]
            if i < 4:
                owners = [1,0x7fffffff,0x80000000,0xffffffff][i:] + [1,0x7fffffff,0x80000000,0xffffffff][:i]
            outputs=[]
            for compiled in (False,True):
                cpu = bundle.cpu()
                cpu.mem.wr(M.S.SELF,seed)
                cpu.setg(3,M.S.SELF)
                for reg,value in enumerate(owners,4):cpu.setg(reg,value)
                if kind=='training' and not compiled:
                    cpu.setg(29,M.S.SELF)
                    cpu.setg(28,owners[0])
                    cpu.setg(31,owners[1])
                    cpu.setg(30,0)
                    cpu.setg(19,0xffffffff)
                    cpu.setg(24,1)
                    # End the original inline store window before the rumble call.
                    fetch=cpu.fetch
                    cpu.fetch=lambda addr: 0x4e800020 if addr==0x823805c8 else fetch(addr)
                cpu.run(bundle.entry if compiled else original)
                outputs.append(cpu.mem.rd(M.S.SELF,0x100))
            cases+=1
            if outputs[0]!=outputs[1]:
                mismatches.append({'kind':kind,'case':i,'byte_offsets':[n for n,(a,b) in enumerate(zip(*outputs)) if a!=b]})
    # Compare the actual compiled license table with the six words in ARTIST.
    tables=[(name,val,sec) for name,val,sec in achievement.symbols.values() if 'KAE_LICENSE_ACHIEVEMENTS' in name and sec>0]
    assert len(tables)==1,tables
    name,value,sec=tables[0]
    section=achievement.sections[sec]
    compiled=achievement.data[section[4]+value:section[4]+value+24]
    original=b''.join(struct.pack('>I',M.V.x360rd.u32(0x8202af68+4*i)) for i in range(6))
    assert compiled==original,(compiled.hex(),original.hex())
    assert M.V.x360rd.u32(0x8202ae78)==0x44160000
    assert M.V.x360rd.u32(0x8202cf3c)==0xc4160000
    result={'cases':cases,'mismatches':mismatches,'license_table_words':list(struct.unpack('>6I',compiled)),
            'target':'actual compiled PPC COFF constructors versus original ARTIST instructions',
            'original_training_window':'8238057C..823805C4; execution ends before original rumble call',
            'objects':[r['object'] for r in rows.values()],
            'limits':'no hardware/boot proof; nonnull owner inputs; padding and surrounding sentinel bytes compared; unresolved calls reject execution'}
    (OUT/'comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('Game-state constructors:',cases,'cases;',len(mismatches),'mismatches; license table matches ARTIST')
    if mismatches:print(json.dumps(mismatches[:4],indent=2))
    raise SystemExit(bool(mismatches))

if __name__=='__main__':main()
