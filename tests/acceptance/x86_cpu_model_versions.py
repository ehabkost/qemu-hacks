#!/usr/bin/env python
#
# Test basic use cases of query-cpu-model-expansion with versioned CPU models
#
#  Copyright (c) 2019 Red Hat Inc
#
# Author:
#  Eduardo Habkost <ehabkost@redhat.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#


import avocado_qemu

class CPUModelExpansion(avocado_qemu.Test):
    def test_Westmere_pclmulqdq(self):
        """Basic test using Westmere pc-1.4 compat code as example"""
        self.vm.add_args('-S')
        self.vm.set_machine('pc-i440fx-1.4')
        self.vm.launch()

        cpus = dict((m['name'], m) for m in self.vm.command('query-cpu-definitions'))
        # "Westmere" is not a static CPU model because it depends on the machine type
        self.assertFalse(cpus['Westmere']['static'])
        # "Westmere-1.4" is a static CPU model
        self.assertTrue(cpus['Westmere-1.4']['static'])

        # static CPU model expansion of "Westmere" will be "Westmere-1.4",
        # with no extra properties
        expanded = self.vm.command('query-cpu-model-expansion', type='static', model={'name':'Westmere'})
        self.asertEquals(expande['model']['name'], 'Westmere-1.4')
        self.asertEquals(expanded['model'].get('props', {}), {})

        # Check if pclmulqdq has the right value by using full expansion:

        full1 = self.vm.command('query-cpu-model-expansion', type='full', model={'name':'Westmere'})
        self.assertFalse(full1['model']['props']['pclmulqdq'])

        full2 = self.vm.command('query-cpu-model-expansion', type='full', model='Westmere-1.4')
        self.assertFalse(full2['model']['props']['pclmulqdq'])

        full3 = self.vm.command('query-cpu-model-expansion', type='full', model='Westmere-1.5')
        self.assertTrue(full3['model']['props']['pclmulqdq'])
