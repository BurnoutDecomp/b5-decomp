import ida_auto,ida_bytes,ida_pro
ida_auto.auto_wait();open('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r95-apt-push-terra/artist-action-table.bin','wb').write(ida_bytes.get_bytes(0x82f73068,1024));ida_pro.qexit(0)
