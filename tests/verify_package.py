"""Validate and smoke-test a native CPack archive away from the build tree."""

import argparse
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zipfile


def verify_architecture(executable, target):
    data = executable.read_bytes()
    if target == "macos-arm64":
        # Thin, little-endian 64-bit Mach-O with CPU_TYPE_ARM64.
        assert struct.unpack_from("<II", data) == (0xFEEDFACF, 0x0100000C)
    elif target == "windows-x64":
        assert data[:2] == b"MZ"
        offset = struct.unpack_from("<I", data, 0x3C)[0]
        assert data[offset:offset + 4] == b"PE\0\0"
        assert struct.unpack_from("<H", data, offset + 4)[0] == 0x8664
    else:
        assert data[:6] == b"\x7fELF\x02\x01"
        assert struct.unpack_from("<H", data, 18)[0] == 62  # EM_X86_64


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--target", required=True,
                        choices=["macos-arm64", "windows-x64", "linux-x64"])
    parser.add_argument("--mesa-dir", type=Path, help="Windows CI-only software OpenGL DLLs")
    args = parser.parse_args()
    archive = args.archive.resolve()
    checksum = Path(str(archive) + ".sha256").read_text().split()[0]
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == checksum

    # Spaces and non-ASCII also exercise Windows process arguments and file APIs.
    with tempfile.TemporaryDirectory(prefix="pcbee-package-") as temporary:
        destination = Path(temporary) / "PCBEE test \u00e9 \u677f"
        destination.mkdir()
        shutil.unpack_archive(str(archive), str(destination))
        if archive.suffix == ".zip":
            # Python's ZIP extraction omits executable mode bits on Unix.
            with zipfile.ZipFile(archive) as package:
                for item in package.infolist():
                    if item.external_attr >> 16:
                        (destination / item.filename).chmod((item.external_attr >> 16) & 0o777)
        roots = list(destination.iterdir())
        assert len(roots) == 1 and roots[0].is_dir(), "Expected one package root"
        root = roots[0]
        for required in ["README.md", "PCB_DESIGN_GUIDE.md", "examples/demo.pcb",
                         "licenses/raylib.txt", "licenses/glfw.txt"]:
            assert (root / required).is_file(), f"Missing {required}"
        executables = {
            "macos-arm64": "PCBEE.app/Contents/MacOS/PCBEE",
            "windows-x64": "pcbee.exe",
            "linux-x64": "pcbee",
        }
        executable = root / executables[args.target]
        assert executable.is_file(), "Missing executable"
        verify_architecture(executable, args.target)
        assert not any(root.rglob("*.a")), "Development libraries leaked into package"
        assert not any(root.rglob("*.lib")), "Development libraries leaked into package"
        assert not any(root.rglob("*.dll")), "Unexpected runtime DLL dependency"
        if args.mesa_dir:
            dlls = list(args.mesa_dir.glob("*.dll"))
            assert dlls, "Missing Mesa DLLs"
            for dll in dlls:
                shutil.copy2(dll, executable.parent)
        subprocess.run([str(executable), "--help"], cwd=root, check=True, timeout=30)
        subprocess.run([str(executable), "--smoke-test", str(root / "examples/demo.pcb")],
                       cwd=root, check=True, timeout=30)
    print(f"Package verified: {archive.name} ({args.target})")


if __name__ == "__main__":
    main()
