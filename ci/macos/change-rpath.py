#! /usr/bin/env python3
# pylint: disable=C0103
'Handle dependencies from homebrew'

import argparse
import os
import os.path
import re
import shutil
import sys
import subprocess

def otool_list(f):
    'List depending libraries using otool'
    libs = []
    res = subprocess.run(['otool', '-L', f], capture_output=True, check=True)
    for line in res.stdout.decode('utf-8').split('\n')[1:]:
        lib = line.strip('\t').split(' ')[0]
        if lib:
            libs.append(lib)
    return libs

def int_set_id(f, name):
    'Set ID using install_name_tool'
    subprocess.run(['install_name_tool', '-id', name, f], capture_output=True, check=True)

def int_change(f, old_lib, new_lib):
    'Change the dependent dylib'
    subprocess.run(['install_name_tool', '-change', old_lib, new_lib, f],
                   capture_output=True, check=True)

def resolve_dylib_path(dylib, src):
    'Resolve @loader_path and @rpath'
    ss = dylib.split('/', 1)
    if len(ss) != 2:
        return dylib
    if ss[0] == '@loader_path' or ss[0] == '@rpath':
        cand = os.path.dirname(src) + '/' + ss[1]
        if os.path.exists(cand):
            return cand
    return dylib

class _CopyDependencies:
    'Copy dependency libraries'
    def __init__(self, args):
        self.args = args
        self.include_re = [re.compile(t) for t in args.include_regex]
        self.exclude_re = [re.compile(t) for t in args.exclude_regex]
        self.check_invalid_re = [re.compile(t) for t in args.check_invalid_regex]
        if args.libdir:
            os.makedirs(args.libdir, exist_ok=True)

    def _is_included(self, lib):
        for r in self.exclude_re:
            if r.match(lib):
                return False
        if not self.include_re:
            return True
        for r in self.include_re:
            if r.match(lib):
                return True
        return False

    def _is_invalid(self, lib):
        for r in self.check_invalid_re:
            if r.match(lib):
                return True
        return False

    def copy_dependencies(self, f, src=None):
        'Copy depending libraries to dest'

        if not self.args.libdir:
            raise ValueError('Error: libdir has to be specified to copy depending libraries.')
        libdir = self.args.libdir.rstrip('/') + '/'

        libs = otool_list(f)
        for lib in libs:
            lib = resolve_dylib_path(lib, src if src else f)

            if not self._is_included(lib):
                continue

            if self.args.verbose >= 1:
                sys.stderr.write(f'{f}: Copying {lib}\n')

            base = os.path.basename(lib)
            dest = libdir + base

            if not os.path.exists(dest):
                shutil.copy(lib, dest, follow_symlinks=True)
                int_set_id(dest, f'@loader_path/{base}')
                self.copy_dependencies(dest, lib)

            relpath = os.path.relpath(dest, os.path.dirname(f))
            int_change(f, lib, '@loader_path/' + relpath)

    def check_dependencies(self, f):
        'Check the depending libraries have been copied.'
        libs = otool_list(f)
        unknown = []
        for lib in libs:
            lib_orig = lib
            lib = resolve_dylib_path(lib, f)

            if not self._is_included(lib):
                continue

            if self._is_invalid(lib) or not os.path.exists(lib):
                unknown.append((lib, lib_orig))

        for lib, lib_orig in unknown:
            msg = f'Error: {f} depends {lib}'
            if lib != lib_orig:
                msg += f' ({lib_orig})'
            print(msg)

        if unknown:
            return 1
        return 0

def _get_args():
    parser = argparse.ArgumentParser()

    # Debugging arguments
    parser.add_argument('--list-dylib', action='store_true', default=False,
                        help='List dependencies and exit')
    parser.add_argument('-v', '--verbose', action='count', default=0)
    parser.add_argument('--check', action='store_true', default=False)

    # Control arguments
    parser.add_argument('--libdir', action='store', default=None)
    parser.add_argument('--include-regex', action='append', default=[])
    parser.add_argument('--exclude-regex', action='append', default=[])
    parser.add_argument('--check-invalid-regex', action='append', default=[])
    parser.add_argument('files', nargs='*', default=[])

    return parser.parse_args()

def main():
    'main routine'
    args = _get_args()

    if args.list_dylib:
        for f in args.files:
            print('\n'.join(otool_list(f)))
        return 0

    ctx = _CopyDependencies(args)
    if args.check:
        ret = 0
        for f in args.files:
            if ctx.check_dependencies(f):
                ret = 1
        return ret

    for f in args.files:
        ctx.copy_dependencies(f)

    return 0

if __name__ == '__main__':
    sys.exit(main())
