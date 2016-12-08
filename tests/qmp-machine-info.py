#!/usr/bin/env python
import sys, os
MY_DIR = os.path.dirname(__file__)
sys.path.append(os.path.join(MY_DIR, '..', 'scripts'))
import qtest
import unittest
import logging
import argparse

logger = logging.getLogger('qemu.tests.machineinfo')

# machines that we can't easily test because they can't run on all hosts:
BLACKLIST = set(['xenpv', 'xenfv'])

# machines known to be broken when using -nodefaults:
NODEFAULTS_BLACKLIST = set([
    'cubieboard',      # segfaults
    'petalogix-ml605', # segfaults
    'or32-sim',        # segfaults
    'virtex-ml507',    # segfaults
    'Niagara',         # segfaults
    'akita',           # "qemu: missing SecureDigital device"
    'borzoi',          # "qemu: missing SecureDigital device"
    'cheetah',         # "qemu: missing SecureDigital device"
    'connex',          # "qemu: missing SecureDigital device"
    'mainstone',       # "qemu: missing SecureDigital device"
    'n800',            # "qemu: missing SecureDigital device"
    'n810',            # "qemu: missing SecureDigital device"
    'spitz',           # "qemu: missing SecureDigital device"
    'sx1',             # "qemu: missing SecureDigital device"
    'sx1-v1',          # "qemu: missing SecureDigital device"
    'terrier',         # "qemu: missing SecureDigital device"
    'tosa',            # "qemu: missing SecureDigital device"
    'verdex',          # "qemu: missing SecureDigital device"
    'z2',              # "qemu: missing SecureDigital device"
])

class QueryMachinesTest(unittest.TestCase):
    def setUp(self):
        self.vm = None

    def tearDown(self):
        if self.vm is not None:
            self.vm.shutdown()

    def walkQOMTree(self, vm, path):
        """Walk QOM tree recusrively, starting at path"""
        children = vm.qmp('qom-list', path=path)['return']
        for c in children:
            logging.debug('walking %s. child: %s', path, c)
            if not c['type'].startswith('child<'):
                continue

            cp = '%s/%s' % (path, c['name'])
            yield cp

            for gc in self.walkQOMTree(vm, cp):
                yield gc

    def findAllBuses(self, vm):
        """Find all bus objects in the QOM tree"""
        r = vm.qmp('qom-list-types', implements='bus')
        bus_types = set([b['name'] for b in r['return']])
        for cp in self.walkQOMTree(vm, '/machine'):
            t = vm.qmp('qom-get', path=cp, property='type')['return']
            if t in bus_types:
                dt = vm.qmp('qom-get', path=cp, property='accepted-device-types').get('return')
                yield dict(path=cp, type=t, accepted_device_types=dt)

    def checkBuses(self, machine, extra_args=[], strict_mode=False):
        """Validate 'supported-device-types' on 'query-machines'"""
        if machine['name'] in BLACKLIST:
            self.skipTest("machine %s on BLACKLIST" % (machine['name']))

        if not machine.has_key('always-available-buses'):
            self.skipTest('machine %s has no always-available-buses field' %
                          (machine['name']))

        args = ['-S', '-machine', machine['name']]
        args.extend(extra_args)
        logger.debug('QEMU args: %s', ' '.join(args))
        self.vm = vm = qtest.QEMUQtestMachine(args=args, logging=False)
        vm.launch()
        found_buses = set()
        for b in machine['always-available-buses']:
            bus_id = b['bus-id']
            btype = vm.qmp('qom-get', path=bus_id, property='type').get('return')
            self.assertEquals(btype, b['bus-type'], "bus-type mismatch for %s" % (bus_id))
            devtypes = vm.qmp('qom-get', path=bus_id, property='accepted-device-types').get('return')
            self.assertEquals(set(devtypes), set(b['accepted-device-types']), "device-type msimatch for %s" % (bus_id))

            found_buses.add(bus_id)

        all_buses = list(self.findAllBuses(vm))
        missing_buses = []
        for b in all_buses:
            full_path = b['path']
            short_name = full_path.split('/')[-1]
            if full_path in found_buses or short_name in found_buses:
                found_buses.discard(full_path)
                found_buses.discard(short_name)
                logger.debug("bus %s was found", full_path)
                continue
            missing_buses.append(full_path)

        if found_buses:
            self.fail("Unexpected inconsistency: some buses were found using qom-get, but not on the device tree: %r", found_buses)

        if missing_buses:
            logger.info("missing buses on machine %s: %s",
                        machine['name'], ' '.join(missing_buses))
            if strict_mode:
                self.fail("missing buses: %s" % (' '.join(missing_buses)))

    def xmachineTestDefaultBuses(self, machine):
        self.checkBuses(machine, [], False)

    def xmachineTestNodefaultsBuses(self, machine):
        if machine['name'] in NODEFAULTS_BLACKLIST:
            self.skipTest("machine %s on NODEFAULTS_BLACKLIST" % (machine['name']))

        self.checkBuses(machine, ['-nodefaults'], True)

    def getSubtypes(self, implements):
        """Get full list of typenames of subtypes of @implements"""
        types = self.vm.command('qom-list-types', implements=implements)
        return [st['name'] for st in types]

    def typeImplements(self, t, i):
        """Returns True if type @t implements type @i"""
        stypes = self.getSubtypes(i)
        return t in stypes

    def validateBus(self, bus, slots):
        """Check if the bus identified by the slot matches the information returned
        for the slot.

        TODO: check if it is really a bus
        TODO: check if accepted-device-types matches accepted-device-types
              property in the bus
        """

        ##we could do this:
        #bustype = self.vm.command('qom-get', path=bus, property='type')
        #self.assertTrue(self.typeImplements(bustype, 'bus'))
        ## but the bus _name_ (accepted by qbus_find()) does not necessarily matches the bus _path_

        pass

    def checkSlotProps(self, slots):
        """check if all properties on "props" are valid properties
        that appear on device-list-properties for all accepted device types
        """
        types_to_check = {}
        buses_to_check = {}
        for slot in slots:
            if slot['props'].has_key('bus'):
                bus = slot['props']['bus']
                buses_to_check.setdefault(bus, []).append(slot)

            for t in slot['accepted-device-types']:
                types_to_check.setdefault(t, set()).update(slot['props'].keys())

        for bus,slots in buses_to_check.items():
            self.validateBus(bus, slots)

        for t, props in types_to_check.items():
            props.discard('bus') # 'bus' is handled by device_add directly
            for st in self.vm.command('qom-list-types', implements=t, abstract=False):
                dprops = self.vm.command('device-list-properties', typename=st['name'])
                dpropnames = set([p['name'] for p in dprops])
                for p in props:
                    self.assertIn(p, dpropnames)

    #def checkSlotDevices(self, slots):
    #    """Check if all plugged devices are on the QOM tree and are of the right type"""
    #    for slot in slots:
    #        for d in slot['devices']:
    #            dtype = self.vm.command('qom-get', path=d, property='type')
    #            self.assertTrue(any(self.typeImplements(dtype, t) for t in slot['accepted-device-types']))

    def checkAvailableField(self, slots):
        for slot in slots:
            if slot.has_key('max-devices') and len(slot['devices']) >= slot['max-devices']:
                self.assertFalse(slot['available'])

    def checkSlotInfo(self, args):
        #TODO:
        # * check if -device works with at least one device type
        # * check if query-hotpluggable-cpus matches what's in query-device-slots
        # * check if accepted-device-types match the property on the bus
        # * check if available=false if len(devices) >= max-devices
        # * check if all plugged devices are really in the QOM tree
        self.vm = qtest.QEMUQtestMachine(args=args, logging=False)
        self.vm.launch()

        slots = self.vm.command('query-device-slots')
        self.checkSlotProps(slots)
        #self.checkSlotDevices(slots)
        self.checkAvailableField(slots)

    def machineTestSlotInfo(self, machine):
        #TODO:
        # * check if -device works with at least one device type
        # * check if query-hotpluggable-cpus matches what's in query-device-slots
        # * check if accepted-device-types match the property on the bus
        # * check if available=false if len(devices) >= max-devices
        # * check if all plugged devices are really in the QOM tree
        if machine['name'] in BLACKLIST:
            self.skipTest("machine %s on BLACKLIST" % (machine['name']))

        args = ['-S', '-machine', machine['name']]
        self.checkSlotInfo(args)

        # run an additional test run with q35-chipset.cfg, when testing
        # q35. It will create extra buses and slots
        if machine['name'] == 'q35':
            cfg = os.path.join(MY_DIR, '..', 'docs', 'q35-chipset.cfg')
            args.extend(['-readconfig', cfg])
            self.checkSlotInfo(args)

    @classmethod
    def addMachineTest(klass, method_name, machine):
        """Dynamically add a testMachine_<arch>_<name>_<machine> method to the class"""
        method = getattr(klass, method_name)
        def testMachine(self):
            return method(self, machine)
        machine_name = machine['name'].replace('-', '_').replace('.', '_')
        method_name = 'test_%s_%s_%s' % (method_name, machine['arch'], machine_name)
        setattr(klass, method_name, testMachine)
        return method_name


    @classmethod
    def discoverMachines(klass, binary):
        """Run query-machines

        This method is run before test cases are started, so we
        can dynamically add test cases for each machine supported
        by the binary.
        """
        vm = qtest.QEMUQtestMachine(binary=binary, args=['-S', '-machine', 'none'], logging=False)
        vm.launch()
        try:
            arch = vm.qmp('query-target')['return']['arch']
            machines = vm.qmp('query-machines')['return']
            for m in machines:
                m['arch'] = arch
        finally:
            vm.shutdown()
        return machines

    @classmethod
    def addMachineTests(klass, binary):
        """Dynamically add test methods for each machine found on QEMU binary

        Look for all methods with "machineTest" prefix, and add
        custom test methods that will test them, for each machine-type
        found on QEMU binary 'binary'.
        """
        method_names = unittest.loader.getTestCaseNames(klass, prefix='machineTest')
        machines = klass.discoverMachines(binary)
        for machine in machines:
            for mname in method_names:
                klass.addMachineTest(mname, machine)


if os.getenv('QTEST_QEMU_BINARY'):
    QueryMachinesTest.addMachineTests(os.getenv('QTEST_QEMU_BINARY'))

if __name__ == '__main__':
    if os.getenv('QTEST_LOG_LEVEL'):
        logging.basicConfig(level=int(os.getenv('QTEST_LOG_LEVEL')))
    else:
        logging.basicConfig(level=logging.WARN)
    unittest.main()
