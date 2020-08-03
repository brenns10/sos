# -*- coding: utf-8 -*-
"""
Testing FAT filesystem implementation
"""
import re
import subprocess

import pytest


def make_empty_disk(fileobj, size):
    subprocess.check_call([
        'dd', 'if=/dev/zero', 'of=' + str(fileobj), 'bs=1M',
        'count={}'.format(size),
    ])


def add_file(tmpdir, diskfile, dest, contents):
    to_add = tmpdir.join('to_add')
    with to_add.open(mode='w') as f:
        f.write(contents)
    subprocess.check_call([
        'mcopy', '-i', str(diskfile), str(to_add), dest,
    ])
    to_add.remove()


def add_dir(diskfile, name):
    subprocess.check_call(['mmd', '-i', str(diskfile), name])


@pytest.fixture
def f12disk(tmpdir):
    diskfile = tmpdir.join('disk')
    make_empty_disk(diskfile, 4)
    subprocess.check_call(['mformat', '-i', str(diskfile)])
    add_file(tmpdir, diskfile, '::/FILE1.TXT', 'the first file')
    add_dir(diskfile, '::/DIR')
    add_file(tmpdir, diskfile, '::/DIR/FILE2.TXT', 'the second file')
    yield diskfile


@pytest.fixture
def fatvm(raw_vm, f12disk):
    raw_vm.start(diskimg=str(f12disk))
    raw_vm.read_until(raw_vm.prompt)
    raw_vm.cmd('exit')
    yield raw_vm


@pytest.fixture
def devname(fatvm):
    match = re.search(r'blk: registered device "(.*)"', fatvm.full_output)
    assert match
    yield match.group(1)


def test_mount(fatvm, devname):
    output = fatvm.cmd(f'fat init {devname}')
    assert 'We determined fstype' in output


def parse_ls(output):
    return [
        tuple(line.split(' ', maxsplit=1))
        for line in output.strip().split('\r\n')[1:]
    ]


@pytest.fixture
def mountvm(fatvm, devname):
    fatvm.cmd(f'fat init {devname}')
    return fatvm


def test_list_root(mountvm):
    output = mountvm.cmd('fs ls /')
    entries = parse_ls(output)
    assert ('f', 'FILE1.TXT') in entries
    assert ('d', 'DIR') in entries


def test_list_other(mountvm):
    output = mountvm.cmd('fs ls /DIR')
    entries = parse_ls(output)
    assert ('d', '.') in entries
    assert ('d', '..') in entries
    assert ('f', 'FILE2.TXT') in entries


def test_cat_file_root(mountvm):
    output = mountvm.cmd('fs cat /FILE1.TXT')
    assert 'the first file' in output


def test_cat_file_other(mountvm):
    output = mountvm.cmd('fs cat /DIR/FILE2.TXT')
    assert 'the second file' in output
