from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPO_ROOT = ROOT.parents[3]
BUILD_DIR = ROOT / "build-sender-single"
PACKAGE_HTML = ROOT / "package-cimbar-html.py"
OUTPUT_HTML = ROOT / "web" / "cimbar_js.html"


def read_properties(path: Path) -> dict[str, str]:
    props: dict[str, str] = {}
    if not path.exists():
        return props

    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        props[key.strip()] = value.strip().replace("\\:", ":").replace("\\\\", "\\")
    return props


def read_cache_entry(cache_text: str, key: str) -> str | None:
    match = re.search(rf"^{re.escape(key)}:[^=]*=(.+)$", cache_text, re.MULTILINE)
    return match.group(1).strip() if match else None


def existing_cache_text() -> str:
    for candidate in (BUILD_DIR / "CMakeCache.txt", ROOT / "build-wasm" / "CMakeCache.txt"):
        if candidate.exists():
            return candidate.read_text(encoding="utf-8", errors="replace")
    return ""


def resolve_sdk_cmake_bin(local_props: dict[str, str]) -> Path:
    sdk_dir = (
        os.environ.get("ANDROID_SDK_ROOT")
        or os.environ.get("ANDROID_HOME")
        or local_props.get("sdk.dir")
    )
    if not sdk_dir:
        raise SystemExit("Missing Android SDK path. Set sdk.dir in local.properties or ANDROID_SDK_ROOT.")

    cmake_root = Path(sdk_dir) / "cmake"
    preferred = cmake_root / "3.22.1" / "bin"
    if preferred.exists():
        return preferred

    versions = sorted([p for p in cmake_root.glob("*") if p.is_dir()], reverse=True)
    if not versions:
        raise SystemExit(f"Could not find bundled CMake under {cmake_root}")
    return versions[0] / "bin"


def resolve_emsdk_dir(local_props: dict[str, str], cache_text: str) -> Path:
    emsdk_dir = (
        os.environ.get("EMSDK")
        or local_props.get("emsdkdir")
        or local_props.get("emsdk")
    )
    if emsdk_dir:
        return Path(emsdk_dir)

    toolchain = read_cache_entry(cache_text, "CMAKE_TOOLCHAIN_FILE")
    if toolchain:
        return Path(toolchain).parents[5]

    raise SystemExit(
        "Missing emsdk path. Set EMSDK or emsdkdir in local.properties/gradle.properties."
    )


def resolve_node_executable(emsdk_dir: Path, cache_text: str) -> Path:
    cached = read_cache_entry(cache_text, "CMAKE_CROSSCOMPILING_EMULATOR")
    if cached:
        cached_path = Path(cached)
        if cached_path.exists():
            return cached_path

    candidates = sorted((emsdk_dir / "node").glob("*/bin/node.exe"), reverse=True)
    if candidates:
        return candidates[0]

    unix_candidates = sorted((emsdk_dir / "node").glob("*/bin/node"), reverse=True)
    if unix_candidates:
        return unix_candidates[0]

    raise SystemExit(f"Could not find emsdk Node executable under {emsdk_dir / 'node'}")


def resolve_opencv_wasm_dir(local_props: dict[str, str], cache_text: str) -> Path:
    explicit = (
        os.environ.get("OPENCV_WASM_DIR")
        or local_props.get("opencvwasmdir")
        or local_props.get("opencvwasmroot")
        or local_props.get("opencvwasminstall")
    )
    if explicit:
        return Path(explicit)

    include_flags = read_cache_entry(cache_text, "CMAKE_CXX_FLAGS") or ""
    match = re.search(r"-I(.+?/opencv-install-wasm)/include/opencv4", include_flags.replace("\\", "/"))
    if match:
        return Path(match.group(1))

    raise SystemExit(
        "Missing OpenCV wasm install path. Set OPENCV_WASM_DIR or opencvwasmdir in local.properties/gradle.properties."
    )


def run(command: list[str], cwd: Path = ROOT) -> None:
    print(">>", " ".join(command))
    subprocess.run(command, cwd=str(cwd), check=True)


def main() -> None:
    local_props = read_properties(REPO_ROOT / "local.properties")
    gradle_props = read_properties(REPO_ROOT / "gradle.properties")
    merged_props = {**gradle_props, **local_props}
    cache_text = existing_cache_text()

    cmake_bin = resolve_sdk_cmake_bin(merged_props)
    cmake = cmake_bin / ("cmake.exe" if os.name == "nt" else "cmake")
    ninja = cmake_bin / ("ninja.exe" if os.name == "nt" else "ninja")
    emsdk_dir = resolve_emsdk_dir(merged_props, cache_text)
    toolchain = emsdk_dir / "upstream" / "emscripten" / "cmake" / "Modules" / "Platform" / "Emscripten.cmake"
    node = resolve_node_executable(emsdk_dir, cache_text)
    opencv_wasm_dir = resolve_opencv_wasm_dir(merged_props, cache_text)
    opencv_libs = (
        os.environ.get("OPENCV_WASM_LIBS")
        or merged_props.get("opencvwasmlibs")
        or read_cache_entry(cache_text, "OPENCV_LIBS")
        or "opencv_imgproc;opencv_photo;opencv_core;opencv_flann;opencv_features2d;opencv_calib3d;opencv_objdetect;opencv_video;opencv_dnn"
    )

    if not toolchain.exists():
        raise SystemExit(f"Missing emscripten toolchain file: {toolchain}")
    if not opencv_wasm_dir.exists():
        raise SystemExit(f"Missing OpenCV wasm install directory: {opencv_wasm_dir}")

    configure_cmd = [
        str(cmake),
        "-S",
        str(ROOT),
        "-B",
        str(BUILD_DIR),
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja}",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        f"-DCMAKE_CROSSCOMPILING_EMULATOR={node}",
        f"-DCMAKE_C_FLAGS=-I{opencv_wasm_dir / 'include' / 'opencv4'}",
        f"-DCMAKE_CXX_FLAGS=-I{opencv_wasm_dir / 'include' / 'opencv4'}",
        f"-DCMAKE_EXE_LINKER_FLAGS=-L{opencv_wasm_dir / 'lib'}",
        f"-DOPENCV_LIBS={opencv_libs}",
        "-DUSE_WASM=2",
        "-DDISABLE_TESTS=1",
    ]

    build_cmd = [
        str(cmake),
        "--build",
        str(BUILD_DIR),
        "--target",
        "install",
        "--parallel",
        "6",
    ]

    run(configure_cmd, cwd=REPO_ROOT)
    run(build_cmd, cwd=REPO_ROOT)
    run([sys.executable, str(PACKAGE_HTML)], cwd=ROOT)

    if not OUTPUT_HTML.exists():
        raise SystemExit(f"Expected sender HTML was not generated: {OUTPUT_HTML}")


if __name__ == "__main__":
    main()
