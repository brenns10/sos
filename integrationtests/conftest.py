# -*- coding: utf-8 -*-
"""
Provides fixtures for integration testing SOS
"""
import os
import queue
import re
import shlex
import subprocess
import sys
import threading
import time

import pytest


def read_thread(f, q, proc, debug=True):
    """
    Thread which constantly waits on stdout of a process, placing everything it
    reads into a queue for processing by another thread. This is because pipes
    are somewhat silly and can't be written to without being read.
    """
    while proc.poll() is None:
        val = os.read(f.fileno(), 4096)
        if debug:
            print(val.decode(), end='')
            sys.stdout.flush()
        if val:
            q.put(val)


class SosVirtualMachine(object):
    """
    Object which lets you run commands on the OS via the virtual machine stdin
    and stdout.
    """

    timeout = 2
    abort = re.compile(r'END OF FAULT REPORT')

    def start(self):
        thisdir = os.path.dirname(__file__)
        kernel = os.path.join(thisdir, '../kernel.bin')
        qemu_cmd = os.environ['QEMU_CMD']
        self.debug = os.environ.get('SOS_DEBUG') == 'true'
        if self.debug:
            print('\n[sos test] DEBUGGING MODE ACTIVE')
            print('[sos test] Use `make gdb` in separate terminal to debug test')
            self.timeout = 120
        cmd = f'{qemu_cmd} -kernel {kernel}'
        self.qemu = subprocess.Popen(
            shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            stdin=subprocess.PIPE, encoding='utf-8')

        self.stdout_queue = queue.SimpleQueue()
        self.stdout_thread = threading.Thread(
            target=read_thread,
            args=(self.qemu.stdout, self.stdout_queue, self.qemu, self.debug)
        )
        self.stdout_thread.start()
        self.full_output = ''
        self.pending_output = None

    def _add_output(self, data, processed_until=None):
        if not processed_until:
            processed_until = len(data)
        self.full_output += data[:processed_until]
        self.pending_output = data[processed_until:].encode('utf-8')

    def _get(self, timeout):
        if self.pending_output:
            tmp = self.pending_output
            self.pending_output = None
            return tmp
        return self.stdout_queue.get(timeout=timeout)

    def read_until(self, pattern, timeout=None):
        """
        Read output until a regex is matched
        """
        if not isinstance(pattern, re.Pattern):
            pattern = re.compile(pattern)
        if timeout is None:
            timeout = self.timeout

        wait_until = time.time() + timeout
        result = ''
        while True:
            time_left = wait_until - time.time()
            if time_left <= 0:
                break
            try:
                data = self._get(time_left)
                result += data.decode('utf-8')
            except queue.Empty:
                pass

            found = pattern.search(result)
            if found:
                self._add_output(result, found.end())
                return result

            found = self.abort.search(result)
            if found and self.debug:
                time.sleep(0.1)  # bad sleep sync to let stdout thread print
                print('\n[sos test] Fault detected! Hit enter when done')
                print('[sos test] debugging to exit the test.')
                input()
            if found:
                self._add_output(result)
                raise Exception(f'Fault encountered waiting:\n{result}')
        self._add_output(result)
        raise TimeoutError(f'Timed out waiting for QEMU response:\n{result}')

    def send_cmd(self, command):
        """
        Send a command, and return immediately without waiting for any output.
        """
        self.qemu.stdin.write(command + '\r\n')
        self.qemu.stdin.flush()

    def cmd(self, command, pattern='[uk]sh>', timeout=None):
        """
        Send a command and wait for a user or kernel shell prompt.
        """
        if timeout is None:
            timeout = self.timeout
        self.send_cmd(command)
        return self.read_until(pattern, timeout=timeout)

    def stop(self):
        self.qemu.kill()
        self.stdout_thread.join()


@pytest.fixture
def raw_vm():
    sos = SosVirtualMachine()
    try:
        sos.start()
        yield sos
    finally:
        sos.stop()


@pytest.fixture
def vm(raw_vm):
    raw_vm.read_until('ush>')
    yield raw_vm
