import sys,struct,json
from pathlib import Path
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re')
import x360rd as x
assert x.u32(0x82815570)==0x7C8A2378 and x.u32(0x82F264A8)==4
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
needles=[b'ICEController',b'miStatus',b'StatusText',b'StatusStream',b'StatusLine',b'StatusCount',b'maRamp',b'menuTimer',b'status message',b'ICEWidgetIcon']
strings=[];consts=[]
for s,e,c in x._segs:
 # Entire loaded image; ID1 flag low bytes copied directly, calibrated by two known values.
 b=x._m[x._base+c*4:x._base+(c+e-s)*4:4]
 for nd in needles:
  st=0
  while True:
   k=b.lower().find(nd.lower(),st)
   if k<0:break
   strings.append({'va':hex(s+k),'needle':nd.decode(),'context':b[max(0,k-32):k+160].decode('ascii','replace')})
   st=k+1
 if s < 0x821f0000 or s>=0x82cd0000:
  for off in (0x7200,0x7204,0x7208,0x720c,0x724c,0x7268,0x72fc,0x7390,0x73b8,0x73d0,0x90e0):
   nd=struct.pack('>I',off);st=0
   while True:
    k=b.find(nd,st)
    if k<0:break
    if (s+k)%4==0:
     a=max(0,k-20);z=min(len(b),k+24)
     consts.append({'va':hex(s+k),'value':hex(off),'context_begin':hex(s+a),'words':[hex(struct.unpack('>I',b[i:i+4])[0]) for i in range(a,z-3,4)]})
    st=k+1
(out/'raw-string-and-layout-constant-scan.json').write_text(json.dumps({'strings':strings,'constants':consts},indent=2),encoding='utf-8')
print('raw targetstrings',len(strings),'layout value occurrences',len(consts))
for v in strings:print(v)
for v in consts:print(v)
