"""Verify that a post-resume loader failure leaves the owned test process alive."""
from pathlib import Path
import argparse
import ctypes
import json
import os
import subprocess
import tempfile
import shutil

parser = argparse.ArgumentParser()
parser.add_argument("--native-bin", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
parser.add_argument("--efmi-failure", action="store_true")
args = parser.parse_args()
binary = args.native_bin.resolve()
args.work.mkdir(parents=True, exist_ok=True)
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateEventW.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_wchar_p]
kernel.CreateEventW.restype = ctypes.c_void_p
kernel.SetEvent.argtypes = [ctypes.c_void_p]
kernel.CloseHandle.argtypes = [ctypes.c_void_p]
kernel.OpenProcess.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_uint32]
kernel.OpenProcess.restype = ctypes.c_void_p
kernel.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
kernel.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]

with tempfile.TemporaryDirectory(dir=args.work.resolve()) as directory:
    root = Path(directory)
    index = root / "index.json"
    index.write_text(json.dumps({"schema": 1, "port": 41001, "token": "fixture", "modules": []}))
    broken_host = root / "Broken.Host.dll"
    broken_host.write_bytes(b"This is intentionally not a DLL")
    ready = root / "ready.txt"
    event = kernel.CreateEventW(None, True, False, r"Local\EAI.FailureTest." + str(os.getpid()))
    assert event
    environment = os.environ.copy()
    environment["EML_TEST_STOP"] = r"Local\EAI.FailureTest." + str(os.getpid())
    environment["EML_TEST_READY"] = str(ready)
    extra = []
    host_path = broken_host
    if args.efmi_failure:
        efmi = root / "efmi"
        (efmi / "Core/EFMI").mkdir(parents=True)
        shutil.copyfile(binary / "EFMIFailureStub.dll", efmi / "3dmloader.dll")
        for name in ["d3d11.dll", "d3dcompiler_47.dll", "d3dx.ini", "Core/EFMI/main.ini"]:
            (efmi / name).touch()
        extra = ["--efmi", str(efmi)]
        host_path = binary / "EModLoader.Host.dll"
    process = None
    try:
        result = subprocess.run(
            [str(binary / "EMLControlledLauncher.exe"),
             "--game", str(binary / "EMLControlledTarget.exe"),
             "--host", str(host_path), "--index", str(index), *extra],
            env=environment, capture_output=True, text=True, timeout=15,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        expected_code = 14 if args.efmi_failure else 8
        assert result.returncode == expected_code, (result.returncode, result.stdout, result.stderr)
        assert ready.is_file(), "Controlled target did not start"
        ready_lines = ready.read_text().splitlines()
        pid = int(ready_lines[0])
        assert ready_lines[1] == ("1" if args.efmi_failure else "0")
        process = kernel.OpenProcess(0x1000 | 0x100000, False, pid)
        assert process, "Loader failure terminated the resumed target"
        code = ctypes.c_uint32()
        assert kernel.GetExitCodeProcess(process, ctypes.byref(code))
        assert code.value == 259, f"Resumed target exited with {code.value}"
        log = (root / "launch.log").read_text()
        assert f"game resumed pid={pid}" in log and f"result code={expected_code}" in log
    finally:
        kernel.SetEvent(event)
        if process:
            assert kernel.WaitForSingleObject(process, 5000) == 0
            kernel.CloseHandle(process)
        kernel.CloseHandle(event)
print("PASS: loader failure reports diagnostics, validates D3D11 arguments and preserves the resumed game process")
