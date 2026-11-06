"""Capture app presents from isolated ARTIST and rebuilt Xenia v6 runs."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
import subprocess
import sys
import time


here = Path(__file__).resolve().parent
smoke = here / "native-xenia-smoke"
original = Path("D:/Emulation/Emulators/Xenia/Xenia Burnout 5 v6/Burnout_tcartwright/BURNOUT_X360_ARTIST.XEX")
presentmon = Path("C:/Program Files/NVIDIA Corporation/FrameViewSDK/bin/PresentMon_x64.exe")
duration = 240


def now():
    return datetime.now(timezone.utc).isoformat()


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def capture(name, image, expected, reference=False):
    run = smoke / name
    assert not run.exists()
    command = [sys.executable, "-X", "utf8", "-B", str(smoke / "launch_from_private_snapshot.py"),
               str(image), name, "--seconds", str(duration), "--windowed", "--no-trace"]
    if reference:
        command.append("--reference")
    with (here / (name + "-launcher.log")).open("xb") as output:
        launcher = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
    started_file = run / "started.json"
    deadline = time.monotonic() + 40
    while not started_file.exists():
        if launcher.poll() is not None or time.monotonic() >= deadline:
            raise RuntimeError(f"Launch did not return a PID for {name}")
        time.sleep(0.25)
    started = json.loads(started_file.read_text())
    staged = json.loads((run / "staged.json").read_text())
    assert started["command"] == staged["command"]
    assert staged["image_sha256"] == expected == sha(Path(staged["image"]))
    assert started["pid"] > 0
    csv = run / "presentmon.csv"
    pm = [str(presentmon), "--process_id", str(started["pid"]), "--session_name", "BPX360-" + name,
          "--output_file", str(csv), "--no_console_stats", "--no_track_input", "--date_time",
          "--v1_metrics", "--timed", "236", "--terminate_after_timed", "--terminate_on_proc_exit"]
    time_started = now()
    print(f"{name} emulator PID {started['pid']}, image {expected}", flush=True)
    with (run / "presentmon.log").open("xb") as output:
        capture_result = subprocess.run(pm, stdout=output, stderr=subprocess.STDOUT,
                                        creationflags=subprocess.CREATE_NO_WINDOW, timeout=260)
    time_finished = now()
    launch_exit = launcher.wait(timeout=40)
    report = {"pid": started["pid"], "image_sha256": expected, "command": pm,
              "capture_start_utc": time_started, "capture_end_utc": time_finished,
              "capture_exit": capture_result.returncode, "launch_exit": launch_exit,
              "csv_size": csv.stat().st_size if csv.exists() else 0}
    (run / "present-capture.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report), flush=True)
    if capture_result.returncode or launch_exit or report["csv_size"] < 512:
        raise RuntimeError(f"Frame capture failed for {name}; inspect {run / 'presentmon.log'}")


if __name__ == "__main__":
    result, expected = Path(sys.argv[1]), sys.argv[2]
    assert presentmon.is_file() and original.is_file() and result.is_file()
    capture("run-r113-fps-artist", original, sha(original), reference=True)
    capture("run-r113-fps-rebuilt", result, expected)
