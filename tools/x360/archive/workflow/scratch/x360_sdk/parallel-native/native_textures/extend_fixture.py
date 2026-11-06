from pathlib import Path
p=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/parallel-native/native_textures/validate_loading_state.py')
s=p.read_text(encoding='utf-8').replace("errors=[];groups=[]",'''errors=[];groups=[]
class TextureCPU(F.AtomicCPU):
    def step(self,pc,w,nxt,depth):
        if w>>26==31 and (w>>1)&1023==457:
            rt=(w>>21)&31;ra=(w>>16)&31;rb=(w>>11)&31
            assert self.g(rb)!=0
            result=self.g(ra)//self.g(rb);self.setg(rt,result)
            if w&1:self.setcr(0,bool(result>>63),bool(result and not result>>63),result==0)
            return nxt
        return super().step(pc,w,nxt,depth)
''').replace('c=F.cpu(b);c.mem.wr', 'c=F.cpu(b);c.__class__=TextureCPU;c.mem.wr')
p.write_text(s,encoding='utf-8')
