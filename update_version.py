"""Inspect or explicitly set the modular firmware identity; never infer it from history."""
import argparse
from pathlib import Path
import re

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--set', dest='version', help='Explicit new firmware version (no automatic upload/build)')
    args = parser.parse_args()
    path = Path(__file__).resolve().parent / 'include/traffic/DeviceConfig.h'
    text = path.read_text()
    pattern = r'(#define\s+FWVersion\s+)"([^"\r\n]+)"'
    matches = list(re.finditer(pattern, text))
    if len(matches) != 1:
        raise SystemExit('Expected exactly one FWVersion in ' + str(path))
    if args.version:
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{0,63}', args.version):
            raise SystemExit('Version must be 1–64 letters, numbers, dots, underscores or hyphens')
        text = re.sub(pattern, lambda m: m[1] + '"' + args.version + '"', text)
        path.write_text(text)
    print('Firmware version: ' + re.search(pattern, text)[2])
    print('Definition: ' + str(path))

if __name__ == '__main__':
    main()
