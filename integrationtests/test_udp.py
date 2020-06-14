# -*- coding: utf-8 -*-
"""
Integration tests for the UDP socket implementation
"""
import re
import socket

import pytest


@pytest.fixture
def sk():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.bind(('0.0.0.0', 5051))
    try:
        yield sock
    finally:
        sock.close()


def test_full(vm, sk):
    res = vm.cmd('socket')
    fildes = int(re.search(r'socket\(\) = (\d+)', res).group(1))

    vm.cmd(f'connect {fildes} 10.0.2.2 {sk.getsockname()[1]}')
    vm.cmd(f'send {fildes} ABAB_CDCD_EFEF')
    data = sk.recv(4096)

    assert data == b'ABAB_CDCD_EFEF\0'
