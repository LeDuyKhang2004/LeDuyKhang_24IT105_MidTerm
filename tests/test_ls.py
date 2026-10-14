#!/usr/bin/env python3
"""Behavioral checks against the supplied manual, not host ls defaults."""
import errno
import os
from pathlib import Path
import pty
import socket
import stat
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = str(Path(sys.argv.pop(1) if len(sys.argv) > 1 else './ls').resolve())


class LsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='midterm-ls-')
        self.root = Path(self.temp.name)
        self.env = dict(os.environ, LC_ALL='C', TZ='UTC')
        self.env.pop('BLOCKSIZE', None)

    def tearDown(self):
        self.temp.cleanup()

    def file(self, name, data=b''):
        path = self.root / name
        path.write_bytes(data)
        return path

    def run_ls(self, *args, env=None, success=True):
        result = subprocess.run([BINARY, *map(str, args)], cwd=self.root,
                                env=dict(self.env, **(env or {})),
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
        return result

    def output(self, *args, **kwargs):
        return self.run_ls(*args, **kwargs).stdout.decode()

    def tty_output(self, *args):
        master, slave = pty.openpty()
        process = subprocess.Popen([BINARY, *args], cwd=self.root, env=self.env,
                                   stdout=slave, stderr=subprocess.PIPE)
        os.close(slave)
        result = b''
        try:
            while True:
                try:
                    part = os.read(master, 4096)
                except OSError as error:
                    if error.errno == errno.EIO:
                        break
                    raise
                if not part:
                    break
                result += part
        finally:
            os.close(master)
        _, error = process.communicate(timeout=10)
        self.assertEqual(process.returncode, 0, error)
        return result.replace(b'\r\n', b'\n').decode()

    def test_default_and_hidden(self):
        for name in ['z', '.hidden', 'a']:
            self.file(name)
        expected = '.hidden\na\nz\n' if os.geteuid() == 0 else 'a\nz\n'
        self.assertEqual(self.output(), expected)
        self.assertEqual(self.output('-A'), '.hidden\na\nz\n')
        self.assertEqual(self.output('-a'), '.\n..\n.hidden\na\nz\n')
        self.assertEqual(self.output('-aA'), self.output('-a'))

    def test_empty_long_total(self):
        self.assertEqual(self.output('-l'), 'total 0\n')
        self.assertEqual(self.output(), '')

    def test_reverse_and_unsorted_operands(self):
        for name in ['c', 'b', 'a']:
            self.file(name)
        self.assertEqual(self.output('-r'), 'c\nb\na\n')
        self.assertEqual(self.output('-fr', 'b', 'c', 'a'), 'b\nc\na\n')
        self.assertEqual(self.output('-f', 'b', 'c', 'a'), 'b\nc\na\n')

    def test_unsorted_directory(self):
        for name in ['q', 'm', 'x', 'a']:
            self.file(name)
        self.assertEqual(self.output('-f').splitlines(), os.listdir(self.root))

    def test_size_and_tie_sort(self):
        for name, size in [('c', 1), ('b', 5), ('a', 5)]:
            self.file(name, b'x' * size)
        self.assertEqual(self.output('-S'), 'a\nb\nc\n')
        self.assertEqual(self.output('-Sr'), 'c\nb\na\n')
        self.assertEqual(self.output('-S', 'c', 'b', 'a'), 'a\nb\nc\n')

    def test_time_sort_and_precedence(self):
        a, b = self.file('a', b'xxx'), self.file('b')
        os.utime(a, ns=(1700000200000000000, 1700000000000000000))
        os.utime(b, ns=(1700000100000000000, 1700000000000000010))
        self.assertEqual(self.output('-t'), 'b\na\n')
        self.assertEqual(self.output('-tu'), 'a\nb\n')
        self.assertEqual(self.output('-St'), 'b\na\n')
        self.assertEqual(self.output('-tS'), 'a\nb\n')
        self.assertEqual(self.output('-tr'), 'a\nb\n')
        self.assertEqual(self.output('-tu', 'b', 'a'), 'a\nb\n')

    def test_change_time(self):
        a = self.file('a')
        self.file('b')
        time.sleep(0.02)
        a.chmod(0o600)
        self.assertEqual(self.output('-tc'), 'a\nb\n')
        self.assertEqual(self.output('-tuc'), self.output('-tc'))
        self.assertEqual(self.output('-tcu'), self.output('-tu'))

    def test_long_fields_and_inode(self):
        path = self.file('sample', b'hello')
        path.chmod(0o640)
        os.utime(path, (1700000000, 1700000000))
        fields = self.output('-in', 'sample').split()
        info = path.stat()
        self.assertEqual(fields[:6], [str(info.st_ino), '-rw-r-----', '1',
                                     str(info.st_uid), str(info.st_gid), '5'])
        self.assertEqual(fields[6:], ['Nov', '14', '22:13', 'sample'])

    def test_long_numeric_precedence(self):
        self.file('a')
        self.assertEqual(self.output('-ln', 'a'), self.output('-n', 'a'))
        self.assertEqual(self.output('-nl', 'a'), self.output('-l', 'a'))

    def test_time_fields_and_timezone(self):
        path = self.file('a')
        os.utime(path, (1700003600, 1700000000))
        self.assertIn('Nov 14 23:13', self.output('-lu', 'a'))
        self.assertIn('Nov 14 22:13', self.output('-l', 'a'))
        self.assertIn('Nov 15 05:13', self.output('-l', 'a', env={'TZ': 'UTC-7'}))

    def test_human_and_k_precedence(self):
        self.file('a', b'x' * 1536)
        self.assertEqual(self.output('-lh', 'a').split()[4], '1.5K')
        self.assertEqual(self.output('-lhk', 'a').split()[4], '1536')
        self.assertEqual(self.output('-lkh', 'a').split()[4], '1.5K')
        self.assertEqual(self.output('-lk', 'a').split()[4], '1536')

    def test_block_units_and_sparse_file(self):
        path = self.file('a', b'x' * 9000)
        blocks = path.stat().st_blocks
        self.assertEqual(self.output('-s', 'a'), f'{blocks} a\n')
        self.assertEqual(self.output('-sk', 'a'), f'{(blocks + 1) // 2} a\n')
        self.assertEqual(self.output('-s', 'a', env={'BLOCKSIZE': '4096'}),
                         f'{(blocks * 512 + 4095) // 4096} a\n')
        self.assertEqual(self.output('-s', 'a', env={'BLOCKSIZE': '1K'}),
                         self.output('-sk', 'a'))
        self.assertEqual(self.output('-s', 'a', env={'BLOCKSIZE': '-1'}), f'{blocks} a\n')
        with path.open('wb') as stream:
            stream.truncate(10 * 1024 * 1024)
        self.assertEqual(int(self.output('-s', 'a').split()[0]), path.stat().st_blocks)

    def test_totals_pipe_and_terminal(self):
        path = self.file('a', b'x' * 6000)
        blocks = path.stat().st_blocks
        self.assertEqual(self.output('-s'), f'{blocks} a\n')
        self.assertEqual(self.tty_output('-s'), f'total {blocks}\n{blocks} a\n')
        self.assertTrue(self.output('-l').startswith(f'total {blocks}\n'))
        self.assertTrue(self.output('-lk').startswith(f'total {(blocks + 1) // 2}\n'))
        self.assertTrue(self.output('-lh').splitlines()[0].endswith('K'))

    def test_directory_and_recursive_precedence(self):
        (self.root / 'd').mkdir()
        self.file('d/a')
        (self.root / 'd/sub').mkdir()
        self.file('d/sub/b')
        self.assertEqual(self.output('-d', 'd'), 'd\n')
        recursive = 'd:\na\nsub\n\nd/sub:\nb\n'
        self.assertEqual(self.output('-R', 'd'), recursive)
        self.assertEqual(self.output('-Rd', 'd'), 'd\n')
        self.assertEqual(self.output('-dR', 'd'), recursive)
        self.assertEqual(self.output('-aR', 'd').count('d/sub:'), 1)

    def test_symlinks_and_broken_operands(self):
        self.file('a')
        (self.root / 'd').mkdir()
        self.file('d/inside')
        os.symlink('a', self.root / 'link')
        os.symlink('absent', self.root / 'broken')
        os.symlink('d', self.root / 'dirlink')
        self.assertEqual(self.output('broken'), 'broken\n')
        self.assertEqual(self.output('dirlink'), 'inside\n')
        self.assertEqual(self.output('-d', 'dirlink'), 'dirlink\n')
        self.assertIn('link -> a', self.output('-l', 'link'))
        self.assertIn('broken -> absent', self.output('-l'))
        self.assertIn('dirlink -> d', self.output('-l', 'dirlink'))
        self.assertEqual(self.output('-F', 'dirlink'), 'dirlink@\n')
        os.symlink('..', self.root / 'd/parent')
        self.assertNotIn('d/parent:', self.output('-R', 'd'))

    def test_classification_and_special_files(self):
        self.file('executable').chmod(0o755)
        self.file('regular')
        (self.root / 'directory').mkdir()
        os.mkfifo(self.root / 'fifo')
        os.symlink('regular', self.root / 'link')
        with socket.socket(socket.AF_UNIX) as sock:
            # macOS limits Unix socket paths; chdir is scoped to this setup.
            previous = os.getcwd()
            try:
                os.chdir(self.root)
                sock.bind('socket')
            finally:
                os.chdir(previous)
            self.assertEqual(self.output('-F'),
                             'directory/\nexecutable*\nfifo|\nlink@\nregular\nsocket=\n')
        self.assertEqual(self.output('-F', '/dev/null'), '/dev/null\n')
        self.assertTrue(self.output('-l', '/dev/null').startswith('c'))
        dev = os.stat('/dev/null').st_rdev
        self.assertIn(f'{os.major(dev)}, {os.minor(dev)}', self.output('-l', '/dev/null'))

    def test_special_permission_bits(self):
        path = self.file('a')
        for mode, expected in [(0o4755, '-rwsr-xr-x'), (0o4644, '-rwSr--r--'),
                               (0o2755, '-rwxr-sr-x'), (0o2644, '-rw-r-Sr--'),
                               (0o1755, '-rwxr-xr-t'), (0o1644, '-rw-r--r-T')]:
            path.chmod(mode)
            self.assertEqual(self.output('-l', 'a').split()[0], expected)

    def test_quoting_and_terminal_default(self):
        self.file('a\nb\tc')
        self.assertEqual(self.output(), 'a\nb\tc\n')
        self.assertEqual(self.output('-q'), 'a?b?c\n')
        self.assertEqual(self.output('-qw'), 'a\nb\tc\n')
        self.assertEqual(self.output('-wq'), 'a?b?c\n')
        self.assertEqual(self.tty_output(), 'a?b?c\n')
        self.assertEqual(self.tty_output('-w'), 'a\nb\tc\n')

    def test_invalid_bytes(self):
        name = os.fsencode(self.root) + b'/bad\xff'
        try:
            descriptor = os.open(name, os.O_CREAT | os.O_WRONLY, 0o600)
        except OSError as error:
            if error.errno in (errno.EILSEQ, errno.EPERM, errno.EOPNOTSUPP):
                self.skipTest('filesystem rejects non-UTF-8 filename bytes')
            raise
        os.close(descriptor)
        self.assertEqual(self.run_ls('-w').stdout, b'bad\xff\n')
        self.assertEqual(self.output('-q'), 'bad?\n')

    def test_utf8_printable(self):
        self.file('caf\u00e9')
        locale_name = 'UTF-8' if sys.platform == 'darwin' else 'C.UTF-8'
        self.assertEqual(self.output('-q', env={'LC_ALL': locale_name}), 'caf\u00e9\n')

    def test_mixed_operands_and_headers(self):
        self.file('f')
        (self.root / 'b').mkdir()
        (self.root / 'a').mkdir()
        self.file('a/x')
        self.assertEqual(self.output('b', 'f', 'a'), 'f\n\na:\nx\n\nb:\n')
        self.assertEqual(self.output('-d', 'b', 'f', 'a'), 'a\nb\nf\n')

    def test_errors_and_double_dash(self):
        self.file('-a')
        self.assertEqual(self.output('--', '-a'), '-a\n')
        self.assertNotEqual(self.run_ls('-Z', success=False).returncode, 0)
        result = self.run_ls('absent', '--', success=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'absent', result.stderr)
        result = self.run_ls('absent', '-a', success=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(result.stdout, b'-a\n')

    @unittest.skipIf(os.geteuid() == 0, 'root bypasses permission checks')
    def test_permission_error(self):
        directory = self.root / 'private'
        directory.mkdir()
        directory.chmod(0)
        try:
            self.assertNotEqual(self.run_ls('private', success=False).returncode, 0)
        finally:
            directory.chmod(0o700)

    def test_large_directory_and_deep_tree(self):
        for i in range(600):
            self.file(f'f{599 - i:04}')
        self.assertEqual(self.output().splitlines(), [f'f{i:04}' for i in range(600)])
        directory = self.root / 'tree'
        directory.mkdir()
        for _ in range(120):
            directory /= 'd'
            directory.mkdir()
        self.assertEqual(self.output('-R', 'tree').count(':\n'), 121)


if __name__ == '__main__':
    unittest.main(verbosity=2)
