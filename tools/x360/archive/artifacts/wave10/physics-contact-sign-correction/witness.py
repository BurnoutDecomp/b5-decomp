from pathlib import Path
import ast, hashlib, json, re
source=Path('D:/Reverse/BP-X360-Experimental-Worktrees/native_jacobian_terra_r10/vendor/renderware/include/rw/physics/contact.h')
s=source.read_text(encoding='utf-8')
names=['tangentA','tangentB','upProduct','up','velA','velB']
expr={n:re.search(r'const __vector4\s+'+n+r'\s*=\s*(.*?);',s,re.S).group(1).replace('&lpBodyA->mVel','"velocityA"').replace('&lpBodyB->mVel','"velocityB"') for n in names}
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,b):return tuple(x*y for x,y in zip(a,b))
def madd(a,b,c):return add(mul(a,b),c)
def nmsub(a,b,c):return sub(c,mul(a,b))
def perm(a,b,mask):
 data=a+b
 return tuple(data[i] for i in mask)
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0],0)
def calc(normal,rA,rB,omegaA,omegaB,velocityA,velocityB,tangent_override=None):
 yz=(1,2,0,3);zx=(2,0,1,3);zero=(0,0,0,0)
 scope={'__builtins__':{},'__vmaddfp':madd,'__vnmsubfp':nmsub,'__vperm':perm,'__vmulfp':mul,'__lvx':lambda name,offset: {'velocityA':velocityA,'velocityB':velocityB}[name], 'zero':zero,'yz':yz,'zx':zx,'normal':normal,'omegaA':omegaA,'omegaB':omegaB,'inverseLength':(1,1,1,1)}
 for tag,r in [('a',rA),('b',rB)]:scope[tag+'YZ']=perm(r,r,yz);scope[tag+'ZX']=perm(r,r,zx)
 scope['normalYZ']=perm(normal,normal,yz);scope['normalZX']=perm(normal,normal,zx)
 for name in ['tangentA','tangentB']:scope[name]=eval(compile(ast.parse(expr[name],mode='eval'),'<source-expression>','eval'),scope)
 scope['tangent']=tangent_override or scope['tangentA'];scope['tangentYZ']=perm(scope['tangent'],scope['tangent'],yz);scope['tangentZX']=perm(scope['tangent'],scope['tangent'],zx)
 for name in ['upProduct','up','velA','velB']:scope[name]=eval(compile(ast.parse(expr[name],mode='eval'),'<source-expression>','eval'),scope)
 expected={'tangentA':cross(normal,rA),'tangentB':cross(normal,rB),'up':cross(scope['tangent'],normal),'velA':add(velocityA,cross(omegaA,rA)),'velB':add(velocityB,cross(omegaB,rB))}
 actual={n:scope[n] for n in expected}
 assert actual==expected,(actual,expected)
 return actual
cases=[]
def run(label,**kwargs):
 actual=calc(**kwargs);cases.append({'label':label,'actual':actual})
run('axis-selected',normal=(1,0,0,0),rA=(0,1,0,0),rB=(0,-1,0,0),omegaA=(1,0,0,0),omegaB=(0,0,1,0),velocityA=(2,3,4,0),velocityB=(5,6,7,0))
assert cases[-1]['actual']['tangentA']==(0,0,1,0) and cases[-1]['actual']['up']==(0,1,0,0) and cases[-1]['actual']['velA']==(2,3,5,0) and cases[-1]['actual']['velB']==(6,6,7,0)
run('fallback-and-mixed',normal=(0,1,0,0),rA=(2,3,-4,0),rB=(-2,1,3,0),omegaA=(2,-1,3,0),omegaB=(-1,4,2,0),velocityA=(1,5,-2,0),velocityB=(3,-2,1,0),tangent_override=(0,0,1,0))
result={'method':'Evaluate the six exact current source expressions with lane-wise host VMX arithmetic and an independent cross-product oracle; finite integer inputs and unit inverse length. This is not X360 execution.','source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'expressions':expr,'mask_words':{'YZX':['04050607','08090A0B','00010203','0C0D0E0F'],'ZXY':['08090A0B','00010203','04050607','0C0D0E0F']},'cases':cases,'passed':True}
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave10/physics-contact-sign-correction/witness.json');out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('PASS',len(cases),'cases; source',result['source_sha256'])
