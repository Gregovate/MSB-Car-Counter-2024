#!/usr/bin/env python3
"""Check extraction completeness, executable preservation, and shared-file drift.

This is a structural regression check, not an ESP32 build or hardware test.
Run from either repository: python tools/verify_modular.py [--peer OTHER_REPO]
"""
import argparse
import hashlib
import json
from pathlib import Path
import re

def mask_comments(source):
    pattern = r'R"([^ ()\\\t\r\n]{0,16})\([\s\S]*?\)\1"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]) if m[0].startswith(('//', '/*')) else m[0], source)

def tokens(source):
    return re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]', mask_comments(source))

def body_hashes(source):
    clean = mask_comments(source)
    masked = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', lambda m: ' ' * len(m[0]), clean)
    pattern = r'^(?:(?:static|inline)\s+)*(?:void|bool|int|String|float)\s+(\w+)\s*\([^;{}]*\)\s*\{'
    for match in re.finditer(pattern, masked, re.M):
        start = masked.index('{', match.start())
        depth, end = 1, start + 1
        while depth:
            depth += (masked[end] == '{') - (masked[end] == '}')
            end += 1
        name = match[1]
        if name == 'publishMQTT':
            name += '/3' if 'bool retainFlag' in clean[match.start():start] else '/2'
        digest = hashlib.sha256(json.dumps(tokens(source[start:end]), ensure_ascii=False).encode()).hexdigest()
        yield name, digest

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--peer', type=Path, help='Other counter repository for byte-for-byte shared-file comparison')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    expected = json.loads((root / 'tests/baseline_function_hashes.json').read_text(encoding="utf-8"))
    actual = {}
    for path in sorted((root / 'src').rglob('*.cpp')):
        for name, digest in body_hashes(path.read_text(encoding="utf-8")):
            if name in actual:
                raise SystemExit('FAIL: duplicate function definition: ' + name)
            actual[name] = digest
        if path.name != 'NetworkService.cpp' and '#include "secrets.h"' in path.read_text(encoding="utf-8"):
            raise SystemExit('FAIL: credentials must be compiled only by NetworkService.cpp')
    for name, digest in expected.items():
        if actual.get(name) != digest:
            raise SystemExit('FAIL: missing or changed baseline function: ' + name)
    if set(actual) - set(expected) != {'configureWifiAccessPoints'}:
        raise SystemExit('FAIL: unexpected added/removed functions')
    registration = 'void configureWifiAccessPoints() {' + ''.join(
        f'wifiMulti.addAP(secret_ssid_AP_{n}, secret_pass_AP_{n});' for n in range(1, 6)) + '}'
    expected_registration = dict(body_hashes(registration))['configureWifiAccessPoints']
    if actual['configureWifiAccessPoints'] != expected_registration:
        raise SystemExit('FAIL: AP registration changed')
    config = json.loads((root / 'tests/baseline_config.json').read_text(encoding="utf-8"))
    definitions = {}
    for filename in ['DeviceConfig.h', 'MqttTopics.h']:
        source = mask_comments((root / 'include/traffic' / filename).read_text(encoding="utf-8"))
        definitions.update({m[1]: ' '.join(m[2].split()) for m in re.finditer(r'^#define\s+(\w+)\s+(.+)$', source, re.M)})
    if definitions != config['defines']:
        raise SystemExit('FAIL: GPIO, identity, topic or constant macro changed')
    state = re.sub(r'^\s*#.*$', '', (root / 'src/services/RuntimeState.cpp').read_text(encoding="utf-8"), flags=re.M)
    if tokens(state) != tokens(config['state_tokens']):
        raise SystemExit('FAIL: state initialization values or ordering changed')
    state_header = mask_comments((root / 'include/traffic/RuntimeState.h').read_text(encoding="utf-8"))
    enums = ' '.join(re.findall(r'enum\s+\w+\s*\{[^}]*\}\s*;', state_header))
    if tokens(enums) != tokens(config['enum_tokens']):
        raise SystemExit('FAIL: detector/pattern enum values changed')
    shared = json.loads((root / 'tests/shared_files.json').read_text(encoding="utf-8"))
    for name, digest in shared.items():
        # Git may check text out as CRLF on Windows; compare canonical LF bytes.
        data = (root / name).read_bytes().replace(b'\r\n', b'\n')
        if hashlib.sha256(data).hexdigest() != digest:
            raise SystemExit('FAIL: shared file changed: ' + name)
        if args.peer and data != (args.peer / name).read_bytes().replace(b'\r\n', b'\n'):
            raise SystemExit('FAIL: counterpart shared file differs: ' + name)
    print(f'PASS: {len(expected)} baseline function bodies preserved; AP registration, GPIO/topics, state initialization and enums preserved; {len(shared)} shared files verified' + (' against peer' if args.peer else ''))

if __name__ == '__main__':
    main()
