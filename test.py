#!/usr/bin/env python3

import argparse
import gzip
import io
import math
import os
import random
import subprocess
import sys
import tempfile

BINARY = os.path.join(os.path.dirname(__file__), "./build/output/filgunzip")

parser = argparse.ArgumentParser()
parser.add_argument("seed", nargs="?", type=int)
parser.add_argument("-n", type=int, default=200)
args = parser.parse_args()

seed = args.seed if args.seed is not None else random.randrange(2**32)
rng = random.Random(seed)
print(f"seed={seed}")

passed = failed = 0


def make_gzip(data: bytes, level: int) -> bytes:
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=level) as f:
        f.write(data)
    return buf.getvalue()


def run(gz: bytes) -> tuple[int, bytes]:
    with tempfile.NamedTemporaryFile(suffix=".gz", delete=False) as inf:
        inf.write(gz)
        in_path = inf.name
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as outf:
        out_path = outf.name
    try:
        r = subprocess.run([BINARY, in_path, out_path], capture_output=True, timeout=30)
        if r.returncode == 0:
            with open(out_path, "rb") as f:
                return 0, f.read()
        return r.returncode, b""
    finally:
        os.unlink(in_path)
        os.unlink(out_path)


def random_data(length: int) -> bytes:
    kind = rng.randint(0, 2)
    if kind == 0:
        return bytes(rng.getrandbits(8) for _ in range(length))
    elif kind == 1:
        unit = bytes(rng.getrandbits(8) for _ in range(rng.randint(1, 32)))
        return (unit * (length // len(unit) + 1))[:length]
    else:
        out = bytearray()
        while len(out) < length:
            seg = min(rng.randint(1, 512), length - len(out))
            if rng.random() < 0.5:
                out += bytes(rng.getrandbits(8) for _ in range(seg))
            else:
                out += bytes([rng.getrandbits(8)] * seg)
        return bytes(out)


for i in range(args.n):
    length = max(1, int(math.exp(rng.uniform(0, math.log(500_000)))))
    data = random_data(length)
    level = rng.choice([1, 6, 9])
    name = f"{i:03d} len={length} level={level}"

    try:
        rc, got = run(make_gzip(data, level))
    except subprocess.TimeoutExpired:
        print(f"FAIL  {name}: timeout")
        failed += 1
        continue

    if rc != 0:
        print(f"FAIL  {name}: exit code {rc}")
        failed += 1
    elif got != data:
        print(f"FAIL  {name}: mismatch (got {len(got)}, expected {len(data)})")
        failed += 1
    else:
        print(f"PASS  {name}")
        passed += 1

print(f"\n{passed}/{passed + failed} passed")
sys.exit(0 if failed == 0 else 1)
