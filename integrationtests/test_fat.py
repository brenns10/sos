# -*- coding: utf-8 -*-
"""
Testing FAT filesystem implementation
"""
import collections
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


def read_file(diskfile, name):
    return subprocess.check_output(['mtype', '-i', str(diskfile), name])


@pytest.fixture
def f12disk(tmpdir):
    diskfile = tmpdir.join('disk')
    make_empty_disk(diskfile, 4)
    subprocess.check_call([
        'mformat', '-i', str(diskfile),
        # 512-byte sector, 4 sectors per cluster. This matches what I've been
        # using from mkfs.vfat - it results in a FAT12 disk with 512-byte
        # sectors. Obviously, I'd like to be very general, but for now getting
        # functionality with one disk geometry and FS type is a good start.
        '-M', '512', '-c', '4',
    ])
    add_file(tmpdir, diskfile, '::/FILE1.TXT', 'the first file')
    add_file(tmpdir, diskfile, '::/EMPTY.TXT', 'a')
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


File = collections.namedtuple('File', ['type', 'name', 'size'])


def parse_ls(output):
    files = {}
    print(repr(output))
    output = output.strip()
    if output.endswith('\r\nksh>'):
        output = output[:-6].strip()
    print(repr(output))
    for line in output.split('\r\n')[1:]:
        print(line)
        typ, name, size_parens = line.split(' ', maxsplit=2)
        # (len=X)
        print(size_parens)
        size = int(size_parens[5:-1])
        assert name not in files
        files[name] = File(typ, name, size)
    return files


def assert_has_file(files, name, typ=None, size=None):
    file_ = files.get(name)
    assert file_
    if typ is not None:
        assert file_.type == typ
    if size is not None:
        assert file_.size == size


@pytest.fixture
def mountvm(fatvm, devname):
    fatvm.cmd(f'fat init {devname}')
    return fatvm


def test_list_root(mountvm):
    output = mountvm.cmd('fs ls /')
    files = parse_ls(output)
    assert len(files) == 3
    assert_has_file(files, 'FILE1.TXT', typ='f')
    assert_has_file(files, 'EMPTY.TXT', typ='f')
    assert_has_file(files, 'DIR', typ='d')


def test_list_other(mountvm):
    output = mountvm.cmd('fs ls /DIR')
    files = parse_ls(output)
    assert_has_file(files, '.', typ='d')
    assert_has_file(files, '..')
    assert_has_file(files, 'FILE2.TXT')


def test_cat_file_root(mountvm):
    output = mountvm.cmd('fs cat /FILE1.TXT')
    assert 'the first file' in output


def test_cat_file_other(mountvm):
    output = mountvm.cmd('fs cat /DIR/FILE2.TXT')
    assert 'the second file' in output


def test_multi_block_file(raw_vm, f12disk):
    # Need to do this test with a raw vm and manually mount, etc, because we
    # will need to "reboot".
    vm = raw_vm

    def boot(stop=True):
        if stop:
            vm.stop()
        vm.start(diskimg=str(f12disk))
        vm.read_until(vm.prompt)

        # Get disk name
        match = re.search(r'blk: registered device "(.*)"', vm.full_output)
        assert match
        devname = match.group(1)

        # Exit userspace and mount disk
        vm.cmd('exit')
        vm.cmd(f'fat init {devname}')

    boot(stop=False)

    # Add contents to the file
    string = '1234567890' * 12
    for _ in range(16):
        vm.cmd(f'fs addline /EMPTY.TXT {string}')

    # import os
    # os.system(f'cp {str(f12disk)} mydisk')
    # assert False

    # At this point, the file is 16 * 121 = 1936 bytes long. Writing one more
    # 121-byte chunk to the file will make it 2057 bytes long, which is just
    # longer than one cluster. The correct behavior is to allocate a new
    # cluster, update the FAT for both clusters, and write to the new cluster.
    # This test ensures that it happens both in-memory, and that the FAT
    # changes are properly written to disk so that on subsequent boots (and via
    # mtools) the file contents are properly stored.
    # TODO: the "empty.txt" file actually has a single character in it. It
    # turns out that an empty file need not have a block allocated for it (and
    # probably should not). As a result, a truly empty file would have a
    # cluster 0, which my code would take quite literally and have a tough time
    # on.
    vm.cmd(f'fs addline /EMPTY.TXT {string}')
    contents = vm.cmd('fs cat /EMPTY.TXT', rmprompt=True).replace('\r\n', '\n')
    expected = 'a' + (string + '\n') * 17
    assert contents == expected

    boot()
    contents = vm.cmd('fs cat /EMPTY.TXT', rmprompt=True).replace('\r\n', '\n')
    assert contents == expected

    contents = read_file(f12disk, '::/EMPTY.TXT').decode('utf-8')
    assert contents == expected
