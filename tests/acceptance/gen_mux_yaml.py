#!/usr/bin/env python3
#
# Script for generating a yaml file containing variants for all
# machine-types and CPU models for a QEMU binary, to be used
# as input to Avocado's mux_to_yaml plugin.
#
# Copyright (c) 2018 Red Hat, Inc.
#
# Author:
#  Eduardo Habkost <ehabkost@redhat.com>
#
# This work is licensed under the terms of the GNU GPL, version 2 or
# later.  See the COPYING file in the top-level directory.

import argparse
import os
import sys
import yaml

MY_DIR = os.path.dirname(__file__)
sys.path.append(os.path.join(MY_DIR, '../../scripts'))
import qemu

class YamlMux(yaml.YAMLObject):
    """A !mux mapping entry on the multiplexer YAML file

    This class assumes all !mux nodes on the YAML file are mappings.
    This seems to be a requirement of mux_to_yaml, so it should
    work for all valid mux_to_yaml YAML files.
    """
    yaml_tag = u'!mux'

    def __init__(self, value):
        self.value = value

    def __repr__(self):
        return 'YamlMux(%r)' % (self.value)

    @classmethod
    def from_yaml(cls, loader, node):
        # Not sure if !mux nodes must be mappings, but this seems to be the
        # only kind of !mux node supported by mxu_to_yaml
        return YamlMux(loader.construct_mapping(node))

    @classmethod
    def to_yaml(cls, dumper, data):
        # Not sure if !mux nodes must be mappings, but this seems to be the
        # only kind of !mux node supported by mxu_to_yaml
        return dumper.represent_mapping(cls.yaml_tag, data.value)

def gen_mux(variable, values):
    """Generate a !mux mapping for different values of @variable

    The resulting !mux mapping will one have one entry for each value
    in @values.

    >>> gen_mux('x', [1,2,3])
    YamlMux({1: {'x': 1}, 2: {'x': 2}, 3: {'x': 3}})
    """
    return YamlMux(dict((v,{variable:v}) for v in values))

def gen_variant_for_bin(qemu_bin):
    """Gen a variant for a specific QEMU binary"""
    vm = qemu.QEMUMachine(binary=qemu_bin)
    vm.launch()
    cpus = vm.command('query-cpu-definitions')
    machines = vm.command('query-machines')
    target = vm.command('query-target')
    kvm = vm.command('query-kvm')
    vm.shutdown()

    accels = ['tcg']
    if kvm['present']:
        accels.append('kvm')

    return (target['arch'],
            {'qemu_bin': qemu_bin,
             'cpu': gen_mux('cpu', (m['name'] for m in cpus)),
             'machine': gen_mux('machine', (m['name'] for m in machines)),
             'accel': gen_mux('accel', accels)})

def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("qemu_binary", metavar='QEMUBIN',
                        nargs='+', help="Path to QEMU binary")
    args = parser.parse_args()

    mux = {'arch':YamlMux(gen_variant_for_bin(b) for b in args.qemu_binary)}
    print(yaml.dump(mux))

if __name__ == '__main__':
    sys.exit(main(sys.argv))
