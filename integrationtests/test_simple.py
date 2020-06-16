# -*- coding: utf-8 -*-
"""
Basic tests of functionality for SOS
"""
import time


def test_main_runs(raw_vm):
    raw_vm.read_until("SOS: Startup")


def test_boots_to_user_shell(raw_vm):
    raw_vm.read_until(r"Stephen's OS \(user shell")


def test_help(vm):
    """
    Tests that the user shell can respond to input and return a help output.
    """
    output = vm.cmd('help')
    # one line for echoed input, one line for new prompt, and then need some
    # help output
    assert len(output.split('\n')) > 2


def test_demo(vm):
    """
    Run the demo command and ensure that all tasks exit and no fault happens.
    """
    count = 10
    vm.send_cmd(f'demo {count}')
    timeout = time.time() + 5
    while count > 0 and time.time() < timeout:
        vm.read_until(r'Process \d+ exited with code 0.')
        count -= 1
    assert count == 0, 'Expect all processes to exit successfully'
