from pathlib import Path
p=Path(__file__).with_name('trace_case1.py');s=p.read_text(encoding='utf-8-sig')
s=s.replace('class StopTrace(Exception):pass', '''class CorrectLoadCPU(CPU):
 def exec_vector(self,pc,w,mn):
  if w>>26==4 and(w&0x7f3)in[0x403,0x443]:return cylmodel.Cpu.exec_vector(self,pc,w,mn)
  return super().exec_vector(pc,w,mn)
CPU=CorrectLoadCPU
class StopTrace(Exception):pass''')
s=s.replace('case1-logexp-trace.json','case1-logexp-correct-load-trace.json')
Path(__file__).with_name('trace_case1_correct_load.py').write_text(s)
