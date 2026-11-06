import ida_hexrays,ida_pro
f=ida_hexrays.decompile(0x8284beb8)
open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra/rawpuller-prostreet-process.txt','w').write(str(f));ida_pro.qexit(0)
