"""Regression coverage for Windows text decoding and Git CRLF checkouts.

Run: python -m unittest discover -s tests -p test_verify_modular_portability.py
No PlatformIO packages, credentials or paid libraries are read.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class VerifierPortabilityTests(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name) / 'project'
        for folder in ['src', 'include/traffic', 'tests', 'tools']:
            shutil.copytree(ROOT / folder, self.root / folder)

    def run_windows_verifier(self):
        # Simulate a non-UTF8 Windows default even on a UTF8 Linux test host.
        program = '''from pathlib import Path
import runpy,sys
original=Path.read_text
def legacy_default(self,encoding=None,errors=None):
    return original(self,encoding=encoding or 'cp1252',errors=errors)
Path.read_text=legacy_default
sys.argv=[sys.argv[1]]
runpy.run_path(sys.argv[0],run_name='__main__')
'''
        return subprocess.run([sys.executable, '-c', program, str(self.root / 'tools/verify_modular.py')], capture_output=True, text=True)

    def test_legacy_windows_encoding_with_crlf_passes(self):
        for path in self.root.rglob('*'):
            if path.is_file() and path.suffix in ['.cpp', '.h', '.json', '.py']:
                path.write_bytes(path.read_bytes().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
        result = self.run_windows_verifier()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('PASS:', result.stdout)

    def test_changed_setup_server_still_fails(self):
        path = self.root / 'src/services/WebService.cpp'
        data = path.read_text(encoding='utf-8')
        self.assertIn('HTTP server started', data)
        path.write_text(data.replace('HTTP server started', 'CHANGED server started'), encoding='utf-8')
        result = self.run_windows_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('setupServer', result.stderr)

if __name__ == '__main__':
    unittest.main()
