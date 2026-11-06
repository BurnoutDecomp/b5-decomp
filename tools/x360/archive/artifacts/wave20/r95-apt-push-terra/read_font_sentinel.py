import ida_auto,ida_bytes,ida_pro
ida_auto.auto_wait();f=open('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r95-apt-push-terra/artist-font-sentinel.txt','w');f.write(ida_bytes.get_bytes(0x8305f174,8).hex());f.close();ida_pro.qexit(0)
