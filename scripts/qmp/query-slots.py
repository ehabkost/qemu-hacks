#!/usr/bin/env python
# test script to dump slot info in a more human-friendly way

import qmp
import sys

q = qmp.QEMUMonitorProtocol(sys.argv[1])
q.connect()
slots = q.command('query-device-slots')
for slot in slots:
    types = slot.pop('accepted-device-types')
    print 'Slot set for: %s' % (', '.join(types))
    inc = slot.pop('incomplete')
    if inc:
        print '  Incomplete set'
    else:
        count = slot.pop('count')
        print '  Slot count: %d' % (count)
    props = slot.pop('props')
    for k,v in slot.items():
        if type(v) == bool:
            if v:
                v = 'yes'
            else:
                v = 'no'
        elif type(v) == unicode:
            v = str(v)
        else:
            v = repr(v)
        print '  %s: %s' % (k, v)

    print '  valid device_add arguments:'
    for p in props:
        option = p.pop('option')
        values = p.pop('values')
        assert not p # no other field
        valuestr = repr(values)
        if values['type'] == 'list':
            valuestr = ', '.join(str(v) for v in values['data'])
        elif values['type'] == 'int-set':
            ranges = []
            for r in values['data']['ranges']:
                if r['max'] == r['min']:
                    ranges.append('%d' % (r['max']))
                else:
                    ranges.append('%d-%d' % (r['min'], r['max']))
            valuestr = ','.join(ranges)
        else:
            assert False
        print '    %s: %s' % (option, valuestr)
