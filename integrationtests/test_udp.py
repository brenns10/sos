# -*- coding: utf-8 -*-
"""
Integration tests for the UDP socket implementation
"""
import itertools
import re
import select
import socket
import time

import pytest


SOCKET_RE = re.compile(r'socket\(\) = (\d+)')
PORT_ITER = itertools.cycle(range(5000, 5100))


def recvfrom_timeout(sk, timeout=2):
    r, _, _ = select.select([sk], [], [], timeout)
    if r:
        return sk.recvfrom(4096)
    else:
        raise TimeoutError('Timed out reading from socket')


@pytest.fixture
def net_vm(raw_vm):
    raw_vm.start()
    raw_vm.read_until('netif is configured')
    yield raw_vm


@pytest.fixture
def sk():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.bind(('0.0.0.0', next(PORT_ITER)))
    try:
        yield sock
    finally:
        sock.close()


def test_udp_send_recv_nowait(net_vm, sk):
    """
    Ideally we'd test send and recv separately. But unfortunately the QEMU
    network stack basically does NAT, so we won't be able to send a message to
    the OS until it has sent us one. Thus we test send and recv together.
    """
    res = net_vm.cmd('socket')
    fildes = int(SOCKET_RE.search(res).group(1))

    net_vm.cmd(f'connect {fildes} 10.0.2.2 {sk.getsockname()[1]}')
    net_vm.cmd(f'send {fildes} ABAB_CDCD_EFEF')
    data, addr = recvfrom_timeout(sk)
    assert data == b'ABAB_CDCD_EFEF\0'

    sk.sendto(b'Hello from the test harness!\0', addr)
    time.sleep(0.1)  # sorry :(
    res = net_vm.cmd(f'recv {fildes}')
    assert 'Hello from the test harness!' in res


def test_udp_send_recv_wait(net_vm, sk):
    res = net_vm.cmd('socket')
    fildes = int(SOCKET_RE.search(res).group(1))

    net_vm.cmd(f'connect {fildes} 10.0.2.2 {sk.getsockname()[1]}')
    net_vm.cmd(f'send {fildes} ABAB_CDCD_EFEF')
    data, addr = recvfrom_timeout(sk)
    assert data == b'ABAB_CDCD_EFEF\0'

    net_vm.send_cmd(f'recv {fildes}')
    time.sleep(0.1)  # sorry :(
    sk.sendto(b'Hello from the test harness!\0', addr)
    res = net_vm.read_until('[uk]sh>')
    assert 'Hello from the test harness!' in res
