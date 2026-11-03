#!/usr/bin/env python3
# Copyright (c) 2026 noobpwnftw.
# SPDX-License-Identifier: BSD-2-Clause
"""Fetch the public Silesia files from the corpus author's site and verify MD5."""
import bz2
import hashlib
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

BASE = 'https://sun.aei.polsl.pl/~sdeor/corpus/'
MD5 = {
    'dickens': '88334708559f6db57d79096bc0aca07e',
    'mozilla': 'c7789a2097f1ff944b0c737430a339b3',
    'mr': '38e623e3093b7bf2003ca4b1bbc19927',
    'nci': '31f85bc8706f3c921104e7c169e2e2e1',
    'ooffice': '573c4ae915e36631d8f2dcffb9b9b66d',
    'osdb': 'e734b0c48e6a982adfb5802da3032ecd',
    'reymont': 'd8f54d78105079775f32d76dc55fc671',
    'samba': '154eaea7ea70e89f6339ff0abf4112ca',
    'sao': '79e95a22e18cd82b7e42bf91b380d30b',
    'webster': '474931ad907ac27bf962c75ded46c069',
    'xml': '9b09c0c80104adb8aae910b7d7db003e',
    'x-ray': '9baec32ad14ec3eff487d254382cb91c',
}

def main():
    target = Path(sys.argv[1] if len(sys.argv) > 1 else 'silesia')
    target.mkdir(parents=True, exist_ok=True)
    def fetch(item):
        name, expected = item
        path = target / name
        if path.exists():
            data = path.read_bytes()
        else:
            with urllib.request.urlopen(BASE + name + '.bz2', timeout=90) as stream:
                data = bz2.decompress(stream.read())
        if hashlib.md5(data).hexdigest() != expected:
            raise RuntimeError(f'{name}: checksum mismatch')
        if not path.exists():
            path.write_bytes(data)
        return f'{name}: {len(data)} bytes'
    with ThreadPoolExecutor(max_workers=4) as pool:
        for line in pool.map(fetch, MD5.items()):
            print(line)

if __name__ == '__main__':
    main()
