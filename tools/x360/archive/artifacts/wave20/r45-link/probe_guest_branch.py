import argparse
import pathlib
import re
import runpy
import socket
import subprocess
import time

import psutil


class Remote:
    def __init__(self, sock, log):
        self.sock = sock
        self.log = log

    def record(self, message):
        print(message, flush=True)
        print(message, file=self.log, flush=True)

    def exact(self, n):
        value = b""
        while len(value) < n:
            block = self.sock.recv(n - len(value))
            if not block:
                raise EOFError("stub closed")
            value += block
        return value

    def reply(self):
        while self.exact(1) != b"$":
            pass
        data = bytearray()
        while (byte := self.exact(1)) != b"#":
            data += byte
        assert int(self.exact(2), 16) == sum(data) % 256
        self.sock.sendall(b"+")
        return data.decode("ascii", errors="replace")

    def send(self, query):
        data = query.encode("ascii")
        self.sock.sendall(b"$" + data + b"#" + f"{sum(data) % 256:02x}".encode())

    def request(self, query):
        self.send(query)
        value = self.reply()
        self.record(f"{query} -> {value}")
        return value

    def run(self):
        self.request("c")
        time.sleep(4)
        return self.request("?")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True)
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--breakpoint", default="822b9b98")
    args = parser.parse_args()
    assert re.fullmatch(r"run-r45-[a-z0-9-]+", args.run)
    assert re.fullmatch(r"[a-f0-9]{8}", args.breakpoint)
    wave = pathlib.Path(__file__).resolve().parents[1]
    smoke = wave / "native-xenia-smoke"
    harness = runpy.run_path(str(smoke / "smoke.py"))
    namespace = harness["stage"].__globals__
    original_config = namespace["config_text"]

    def debug_config(*inputs):
        text = original_config(*inputs)
        for key, value in {
            "debug": "true", "break_on_start": "true",
            "gdbport": str(args.port), "discord": "false",
        }.items():
            text, count = re.subn(rf"^{key}\s*=\s*[^\r\n]+", f"{key} = {value}", text, flags=re.M)
            assert count == 1, key
        return text

    namespace["config_text"] = debug_config
    run = smoke / args.run
    receipt = harness["stage"](wave / "r45-link/result.json", run)
    with (run / "host-output.log").open("xb") as output:
        child = subprocess.Popen(
            receipt["command"], cwd=receipt["private_storage_root"], stdin=subprocess.DEVNULL,
            stdout=output, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW,
        )
        harness["write"](run / "started.json", {"pid": child.pid, "command": receipt["command"]})
        try:
            target = psutil.Process(child.pid)
            deadline = time.monotonic() + 12
            log_path = run / "emulator/xenia.log"
            while child.poll() is None and time.monotonic() < deadline:
                listening = any(c.status == "LISTEN" and c.laddr.port == args.port
                                and c.laddr.ip == "127.0.0.1" for c in target.net_connections(kind="tcp"))
                if listening and log_path.is_file() and "Breaking into debugger because" in log_path.read_text(errors="replace"):
                    break
                time.sleep(.1)
            assert child.poll() is None and listening
            with socket.create_connection(("127.0.0.1", args.port), timeout=20) as sock, (run / "rsp-output.log").open("x") as log:
                remote = Remote(sock, log)
                remote.record("initial -> " + remote.reply())
                remote.request("qSupported")
                remote.request("qfThreadInfo")
                assert remote.request("Hg6") == "OK"
                remote.request("Hc6")
                assert remote.request(f"Z0,{args.breakpoint},4") == "OK"
                remote.run()
                remote.request("qC")
                assert remote.request("Hg6") == "OK"
                registers = {reg: remote.request("p" + reg) for reg in ("1", "3", "1f", "40", "42")}
                if re.fullmatch(r"[0-9a-fA-F]{8}", registers["1"]):
                    stack = int(registers["1"], 16)
                    remote.request(f"m{stack + 0x50:x},8")
                remote.request("g")
        except (TimeoutError, EOFError) as error:
            print(type(error).__name__, str(error), flush=True)
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=5)


if __name__ == "__main__":
    main()
