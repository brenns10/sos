# -*- coding: utf-8 -*-
"""
Testing the block device layer
"""
import re

import pytest


BLKS = [
    'the first sector',
    'i am data in the second sector',
]


@pytest.fixture
def disk(tmpdir):
    diskfile = tmpdir.join('disk')
    with diskfile.open(mode='wb') as f:
        def writeblk(datastr):
            f.write(datastr + b'\0' * (512 - len(datastr)))
        writeblk(BLKS[0].encode('ascii'))
        writeblk(BLKS[1].encode('ascii'))

    yield diskfile


@pytest.fixture
def blkvm(raw_vm, disk):
    raw_vm.start(diskimg=str(disk))
    raw_vm.read_until(raw_vm.prompt)
    raw_vm.cmd('exit')
    yield raw_vm


@pytest.fixture
def devname(blkvm):
    match = re.search(r'blk: registered device "(.*)"', blkvm.full_output)
    assert match
    yield match.group(1)


def test_blk_count(blkvm, devname):
    res = blkvm.cmd(f'blkstatus {devname}')
    assert 'block count: 2\n' in res


def test_readp(blkvm, devname):
    res = blkvm.cmd(f'blkread {devname} 0')
    assert f'result: "{BLKS[0]}"' in res
    res = blkvm.cmd(f'blkread {devname} 1')
    assert f'result: "{BLKS[1]}"' in res


def test_read_out_of_bounds(blkvm, devname):
    res = blkvm.cmd(f'blkread {devname} 2')
    assert 'ERROR' in res
