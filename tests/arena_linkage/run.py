#!/usr/bin/env python3
"""Run C11 arena linkage regression tests with available GCC/Clang compilers."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

sources_dir = Path(__file__).resolve().parent
repository = sources_dir.parent.parent
compilers = [cc for cc in ("gcc", "clang") if shutil.which(cc)]
if not compilers:
    raise SystemExit("No GCC or Clang compiler available")
for cc in ("gcc", "clang"):
    if cc not in compilers:
        print(f"{cc}: unavailable", flush=True)

with tempfile.TemporaryDirectory(prefix="cfd-arena-linkage-") as temporary:
    build = Path(temporary)
    for cc in compilers:
        for optimization in ("-O0", "-O2"):
            for layout in ("split", "unity"):
                for sanitizer in ("plain", "asan", "ubsan"):
                    name = f"{cc}-{optimization[1:]}-{layout}-{sanitizer}"
                    flags = ["-std=c11", "-D_GNU_SOURCE", optimization,
                             "-g", f"-I{repository}"]
                    if sanitizer == "asan":
                        flags += ["-fsanitize=address", "-fno-omit-frame-pointer"]
                    if sanitizer == "ubsan":
                        flags += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
                    sources = (["unity.c"] if layout == "unity" else
                               ["impl.c", "consumer_a.c", "consumer_b.c", "main.c"])
                    objects = []
                    for source in sources:
                        obj = build / f"{name}-{source}.o"
                        subprocess.run([cc, *flags, "-c", str(sources_dir / source),
                                        "-o", str(obj)], check=True)
                        objects.append(str(obj))
                    executable = build / name
                    subprocess.run([cc, *flags, *objects, "-o", str(executable)],
                                   check=True)
                    environment = os.environ.copy()
                    # Leak checking can be enabled by the caller where supported.
                    environment.setdefault("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=1")
                    environment.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
                    subprocess.run([str(executable), "65"], env=environment, check=True)
                    print(f"{name}: PASS", flush=True)
