/*
 * QEMU PC System Emulator
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <glib.h>

#include "hw/hw.h"
#include "hw/loader.h"
#include "hw/i386/pc.h"
#include "hw/i386/apic.h"
#include "hw/i386/smbios.h"
#include "hw/pci/pci.h"
#include "hw/pci/pci_ids.h"
#include "hw/usb.h"
#include "net/net.h"
#include "hw/boards.h"
#include "hw/ide.h"
#include "sysemu/kvm.h"
#include "hw/kvm/clock.h"
#include "sysemu/sysemu.h"
#include "hw/sysbus.h"
#include "hw/cpu/icc_bus.h"
#include "sysemu/arch_init.h"
#include "sysemu/blockdev.h"
#include "hw/i2c/smbus.h"
#include "hw/xen/xen.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "hw/acpi/acpi.h"
#include "cpu.h"
#ifdef CONFIG_XEN
#  include <xen/hvm/hvm_info_table.h>
#endif

#define MAX_IDE_BUS 2

static const int ide_iobase[MAX_IDE_BUS] = { 0x1f0, 0x170 };
static const int ide_iobase2[MAX_IDE_BUS] = { 0x3f6, 0x376 };
static const int ide_irq[MAX_IDE_BUS] = { 14, 15 };

/* PC hardware initialisation */
static void pc_init1(MachineState *machine)
{
    PCMachineClass *pcc = PC_MACHINE_GET_CLASS(machine);
    bool pci_enabled = pcc->pci_enabled;
    bool kvmclock_enabled = pcc->kvmclock_enabled;
    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *system_io = get_system_io();
    int i;
    ram_addr_t below_4g_mem_size, above_4g_mem_size;
    PCIBus *pci_bus;
    ISABus *isa_bus;
    PCII440FXState *i440fx_state;
    int piix3_devfn = -1;
    qemu_irq *cpu_irq;
    qemu_irq *gsi;
    qemu_irq *i8259;
    qemu_irq *smi_irq;
    GSIState *gsi_state;
    DriveInfo *hd[MAX_IDE_BUS * MAX_IDE_DEVS];
    BusState *idebus[MAX_IDE_BUS];
    ISADevice *rtc_state;
    ISADevice *floppy;
    MemoryRegion *ram_memory;
    MemoryRegion *pci_memory;
    MemoryRegion *rom_memory;
    DeviceState *icc_bridge;
    FWCfgState *fw_cfg = NULL;
    PcGuestInfo *guest_info;

    if (xen_enabled() && xen_hvm_init(&ram_memory) != 0) {
        fprintf(stderr, "xen hardware virtual machine initialisation failed\n");
        exit(1);
    }

    icc_bridge = qdev_create(NULL, TYPE_ICC_BRIDGE);
    object_property_add_child(qdev_get_machine(), "icc-bridge",
                              OBJECT(icc_bridge), NULL);

    pc_cpus_init(machine->cpu_model, icc_bridge);

    if (kvm_enabled() && kvmclock_enabled) {
        kvmclock_create();
    }

    /* Check whether RAM fits below 4G (leaving 1/2 GByte for IO memory).
     * If it doesn't, we need to split it in chunks below and above 4G.
     * In any case, try to make sure that guest addresses aligned at
     * 1G boundaries get mapped to host addresses aligned at 1G boundaries.
     * For old machine types, use whatever split we used historically to avoid
     * breaking migration.
     */
    if (machine->ram_size >= 0xe0000000) {
        ram_addr_t lowmem = pcc->gigabyte_align ? 0xc0000000 : 0xe0000000;
        above_4g_mem_size = machine->ram_size - lowmem;
        below_4g_mem_size = lowmem;
    } else {
        above_4g_mem_size = 0;
        below_4g_mem_size = machine->ram_size;
    }

    if (pci_enabled) {
        pci_memory = g_new(MemoryRegion, 1);
        memory_region_init(pci_memory, NULL, "pci", UINT64_MAX);
        rom_memory = pci_memory;
    } else {
        pci_memory = NULL;
        rom_memory = system_memory;
    }

    guest_info = pc_guest_info_init(below_4g_mem_size, above_4g_mem_size);

    guest_info->has_acpi_build = pcc->has_acpi_build;

    guest_info->has_pci_info = pcc->has_pci_info;
    guest_info->isapc_ram_fw = !pci_enabled;

    if (pcc->smbios_defaults) {
        MachineClass *mc = MACHINE_GET_CLASS(machine);
        PCMachineClass *pcc = PC_MACHINE_GET_CLASS(mc);
        /* These values are guest ABI, do not change */
        smbios_set_defaults("QEMU", "Standard PC (i440FX + PIIX, 1996)",
                            mc->name, pcc->smbios_legacy_mode);
    }

    /* allocate ram and load rom/bios */
    if (!xen_enabled()) {
        fw_cfg = pc_memory_init(system_memory,
                       machine->kernel_filename, machine->kernel_cmdline,
                       machine->initrd_filename,
                       below_4g_mem_size, above_4g_mem_size,
                       rom_memory, &ram_memory, guest_info);
    }

    gsi_state = g_malloc0(sizeof(*gsi_state));
    if (kvm_irqchip_in_kernel()) {
        kvm_pc_setup_irq_routing(pci_enabled);
        gsi = qemu_allocate_irqs(kvm_pc_gsi_handler, gsi_state,
                                 GSI_NUM_PINS);
    } else {
        gsi = qemu_allocate_irqs(gsi_handler, gsi_state, GSI_NUM_PINS);
    }

    if (pci_enabled) {
        pci_bus = i440fx_init(&i440fx_state, &piix3_devfn, &isa_bus, gsi,
                              system_memory, system_io, machine->ram_size,
                              below_4g_mem_size,
                              above_4g_mem_size,
                              pci_memory, ram_memory);
    } else {
        pci_bus = NULL;
        i440fx_state = NULL;
        isa_bus = isa_bus_new(NULL, system_io);
        no_hpet = 1;
    }
    isa_bus_irqs(isa_bus, gsi);

    if (kvm_irqchip_in_kernel()) {
        i8259 = kvm_i8259_init(isa_bus);
    } else if (xen_enabled()) {
        i8259 = xen_interrupt_controller_init();
    } else {
        cpu_irq = pc_allocate_cpu_irq();
        i8259 = i8259_init(isa_bus, cpu_irq[0]);
    }

    for (i = 0; i < ISA_NUM_IRQS; i++) {
        gsi_state->i8259_irq[i] = i8259[i];
    }
    if (pci_enabled) {
        ioapic_init_gsi(gsi_state, "i440fx");
    }
    qdev_init_nofail(icc_bridge);

    pc_register_ferr_irq(gsi[13]);

    pc_vga_init(isa_bus, pci_enabled ? pci_bus : NULL);

    /* init basic PC hardware */
    pc_basic_device_init(isa_bus, gsi, &rtc_state, &floppy, xen_enabled(),
        0x4);

    pc_nic_init(isa_bus, pci_bus);

    ide_drive_get(hd, MAX_IDE_BUS);
    if (pci_enabled) {
        PCIDevice *dev;
        if (xen_enabled()) {
            dev = pci_piix3_xen_ide_init(pci_bus, hd, piix3_devfn + 1);
        } else {
            dev = pci_piix3_ide_init(pci_bus, hd, piix3_devfn + 1);
        }
        idebus[0] = qdev_get_child_bus(&dev->qdev, "ide.0");
        idebus[1] = qdev_get_child_bus(&dev->qdev, "ide.1");
    } else {
        for(i = 0; i < MAX_IDE_BUS; i++) {
            ISADevice *dev;
            char busname[] = "ide.0";
            dev = isa_ide_init(isa_bus, ide_iobase[i], ide_iobase2[i],
                               ide_irq[i],
                               hd[MAX_IDE_DEVS * i], hd[MAX_IDE_DEVS * i + 1]);
            /*
             * The ide bus name is ide.0 for the first bus and ide.1 for the
             * second one.
             */
            busname[4] = '0' + i;
            idebus[i] = qdev_get_child_bus(DEVICE(dev), busname);
        }
    }

    pc_cmos_init(below_4g_mem_size, above_4g_mem_size, machine->boot_order,
                 floppy, idebus[0], idebus[1], rtc_state);

    if (pci_enabled && usb_enabled(false)) {
        pci_create_simple(pci_bus, piix3_devfn + 2, "piix3-usb-uhci");
    }

    if (pci_enabled && acpi_enabled) {
        I2CBus *smbus;

        smi_irq = qemu_allocate_irqs(pc_acpi_smi_interrupt, first_cpu, 1);
        /* TODO: Populate SPD eeprom data.  */
        smbus = piix4_pm_init(pci_bus, piix3_devfn + 3, 0xb100,
                              gsi[9], *smi_irq,
                              kvm_enabled(), fw_cfg);
        smbus_eeprom_init(smbus, 8, NULL, 0);
    }

    if (pci_enabled) {
        pc_pci_device_init(pci_bus);
    }
}

/**
 * PCI440FXMachineClass;
 *
 * @compat_func: Compat unction to be called before pc_init1()
 */
typedef struct PCI440FXMachineClass {
    /*< private >*/
    PCMachineClass parent_class;

    /*< public >*/
    void (*compat_func)(MachineState *machine);
} PCI440FXMachineClass;

#define TYPE_PC_I440FX_MACHINE "pc-i440fx" TYPE_MACHINE_SUFFIX
#define PC_I440FX_MACHINE_CLASS(klass) \
    OBJECT_CLASS_CHECK(PCI440FXMachineClass, (klass), TYPE_PC_I440FX_MACHINE)
#define PC_I440FX_MACHINE_GET_CLASS(obj) \
    OBJECT_GET_CLASS(PCI440FXMachineClass, (obj), TYPE_PC_I440FX_MACHINE)

static void pc_i440fx_machine_init(MachineState *machine)
{
    PCI440FXMachineClass *piixc = PC_I440FX_MACHINE_GET_CLASS(machine);
    if (piixc->compat_func) {
        piixc->compat_func(machine);
    }
    pc_init1(machine);
}

static void pc_i440fx_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "Standard PC (i440FX + PIIX, 1996)";
    mc->hot_add_cpu = pc_hot_add_cpu;
    mc->init = pc_i440fx_machine_init;
}

static TypeInfo pc_i440fx_machine_type_info = {
    .name = TYPE_PC_I440FX_MACHINE,
    .parent = TYPE_PC_MACHINE,
    .class_init = pc_i440fx_machine_class_init,
    .class_size = sizeof(PCI440FXMachineClass),
    .abstract = true,
};

static void pc_i440fx_machine_v2_1_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCMachineClass *pcc = PC_MACHINE_CLASS(oc);
    mc->default_machine_opts = "firmware=bios-256k.bin";
    mc->alias = "pc";
    mc->init = pc_init1;
    mc->is_default = 1;
    mc->name = "pc-i440fx-2.1";
    pcc->pci_enabled = true;
}

static TypeInfo pc_i440fx_machine_v2_1_type_info = {
    .name = "pc-i440fx-2.1" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_i440fx_machine_v2_1_class_init,
};

static void pc_i440fx_machine_v2_0_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCMachineClass *pcc = PC_MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_2_0,
        { /* end of list */ }
    };
    pc_i440fx_machine_v2_1_class_init(oc, data);
    mc->alias = NULL;
    mc->is_default = false;
    mc->name = "pc-i440fx-2.0";
    machine_class_register_compat_props_array(mc, compat_props);
    pcc->smbios_legacy_mode = true;
}

static TypeInfo pc_i440fx_machine_v2_0_type_info = {
    .name = "pc-i440fx-2.0" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_i440fx_machine_v2_0_class_init,
};

static void pc_compat_1_7(MachineState *machine)
{
    x86_cpu_compat_disable_kvm_features(FEAT_1_ECX, CPUID_EXT_X2APIC);
}

static void pc_i440fx_machine_v1_7_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCMachineClass *pcc = PC_MACHINE_CLASS(oc);
    PCI440FXMachineClass *piixc = PC_I440FX_MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_1_7,
        { /* end of list */ }
    };
    pc_i440fx_machine_v2_0_class_init(oc, data);
    mc->default_machine_opts = NULL;
    mc->name = "pc-i440fx-1.7";
    mc->option_rom_has_mr = true;
    machine_class_register_compat_props_array(mc, compat_props);
    pcc->smbios_defaults = false;
    pcc->gigabyte_align = false;
    piixc->compat_func = pc_compat_1_7;
}

static TypeInfo pc_i440fx_machine_v1_7_type_info = {
    .name = "pc-i440fx-1.7" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_i440fx_machine_v1_7_class_init,
};

static void pc_i440fx_machine_v1_6_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCMachineClass *pcc = PC_MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_1_6,
        { /* end of list */ }
    };
    pc_i440fx_machine_v1_7_class_init(oc, data);
    mc->name = "pc-i440fx-1.6";
    mc->rom_file_has_mr = false;
    machine_class_register_compat_props_array(mc, compat_props);
    pcc->has_acpi_build = false;
}

static TypeInfo pc_i440fx_machine_v1_6_type_info = {
    .name = "pc-i440fx-1.6" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_i440fx_machine_v1_6_class_init,
};

static void pc_i440fx_machine_v1_5_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_1_5,
        { /* end of list */ }
    };
    pc_i440fx_machine_v1_6_class_init(oc, data);
    mc->name = "pc-i440fx-1.5";
    machine_class_register_compat_props_array(mc, compat_props);
}

static TypeInfo pc_i440fx_machine_v1_5_type_info = {
    .name = "pc-i440fx-1.5" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_i440fx_machine_v1_5_class_init,
};

static void pc_compat_1_4(MachineState *machine)
{
    pc_compat_1_7(machine);
    x86_cpu_compat_set_features("n270", FEAT_1_ECX, 0, CPUID_EXT_MOVBE);
    x86_cpu_compat_set_features("Westmere", FEAT_1_ECX, 0, CPUID_EXT_PCLMULQDQ);
}

static void pc_i440fx_machine_v1_4_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCI440FXMachineClass *piixc = PC_I440FX_MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_1_4,
        { /* end of list */ }
    };
    pc_i440fx_machine_v1_5_class_init(oc, data);
    mc->hot_add_cpu = NULL;
    mc->name = "pc-i440fx-1.4";
    machine_class_register_compat_props_array(mc, compat_props);
    piixc->compat_func = pc_compat_1_4;
}

static TypeInfo pc_i440fx_machine_v1_4_type_info = {
    .name = "pc-i440fx-1.4" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_i440fx_machine_v1_4_class_init,
};

#define PC_COMPAT_1_3 \
        {\
            .driver   = "usb-tablet",\
            .property = "usb_version",\
            .value    = stringify(1),\
        },{\
            .driver   = "virtio-net-pci",\
            .property = "ctrl_mac_addr",\
            .value    = "off",      \
        },{ \
            .driver   = "virtio-net-pci", \
            .property = "mq", \
            .value    = "off", \
        }, {\
            .driver   = "e1000",\
            .property = "autonegotiation",\
            .value    = "off",\
        }

static void pc_compat_1_3(MachineState *machine)
{
    pc_compat_1_4(machine);
    enable_compat_apic_id_mode();
}

static void pc_machine_v1_3_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCI440FXMachineClass *piixc = PC_I440FX_MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_1_3,
        { /* end of list */ }
    };
    pc_i440fx_machine_v1_4_class_init(oc, data);
    mc->name = "pc-1.3";
    machine_class_register_compat_props_array(mc, compat_props);
    piixc->compat_func = pc_compat_1_3;
}

static TypeInfo pc_machine_v1_3_type_info = {
    .name = "pc-1.3" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v1_3_class_init,
};

#define PC_COMPAT_1_2 \
        {\
            .driver   = "nec-usb-xhci",\
            .property = "msi",\
            .value    = "off",\
        },{\
            .driver   = "nec-usb-xhci",\
            .property = "msix",\
            .value    = "off",\
        },{\
            .driver   = "ivshmem",\
            .property = "use64",\
            .value    = "0",\
        },{\
            .driver   = "qxl",\
            .property = "revision",\
            .value    = stringify(3),\
        },{\
            .driver   = "qxl-vga",\
            .property = "revision",\
            .value    = stringify(3),\
        },{\
            .driver   = "VGA",\
            .property = "mmio",\
            .value    = "off",\
        }

static void pc_compat_1_2(MachineState *machine)
{
    pc_compat_1_3(machine);
    x86_cpu_compat_disable_kvm_features(FEAT_KVM, KVM_FEATURE_PV_EOI);
}

static void pc_machine_v1_2_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCI440FXMachineClass *piixc = PC_I440FX_MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_1_2,
        { /* end of list */ }
    };
    pc_machine_v1_3_class_init(oc, data);
    mc->name = "pc-1.2";
    machine_class_register_compat_props_array(mc, compat_props);
    piixc->compat_func = pc_compat_1_2;
}

static TypeInfo pc_machine_v1_2_type_info = {
    .name = "pc-1.2" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v1_2_class_init,
};

#define PC_COMPAT_1_1 \
        {\
            .driver   = "virtio-scsi-pci",\
            .property = "hotplug",\
            .value    = "off",\
        },{\
            .driver   = "virtio-scsi-pci",\
            .property = "param_change",\
            .value    = "off",\
        },{\
            .driver   = "VGA",\
            .property = "vgamem_mb",\
            .value    = stringify(8),\
        },{\
            .driver   = "vmware-svga",\
            .property = "vgamem_mb",\
            .value    = stringify(8),\
        },{\
            .driver   = "qxl-vga",\
            .property = "vgamem_mb",\
            .value    = stringify(8),\
        },{\
            .driver   = "qxl",\
            .property = "vgamem_mb",\
            .value    = stringify(8),\
        },{\
            .driver   = "virtio-blk-pci",\
            .property = "config-wce",\
            .value    = "off",\
        }

static void pc_machine_v1_1_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_1_1,
        { /* end of list */ }
    };
    pc_machine_v1_2_class_init(oc, data);
    mc->name = "pc-1.1";
    machine_class_register_compat_props_array(mc, compat_props);
}

static TypeInfo pc_machine_v1_1_type_info = {
    .name = "pc-1.1" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v1_1_class_init,
};

#define PC_COMPAT_1_0 \
        PC_COMPAT_1_1,\
        {\
            .driver   = TYPE_ISA_FDC,\
            .property = "check_media_rate",\
            .value    = "off",\
        }, {\
            .driver   = "virtio-balloon-pci",\
            .property = "class",\
            .value    = stringify(PCI_CLASS_MEMORY_RAM),\
        },{\
            .driver   = "apic",\
            .property = "vapic",\
            .value    = "off",\
        },{\
            .driver   = TYPE_USB_DEVICE,\
            .property = "full-path",\
            .value    = "no",\
        }

static void pc_machine_v1_0_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_1_0,
        { /* end of list */ }
    };
    pc_machine_v1_1_class_init(oc, data);
    mc->hw_version = "1.0";
    mc->name = "pc-1.0";
    machine_class_register_compat_props_array(mc, compat_props);
}

static TypeInfo pc_machine_v1_0_type_info = {
    .name = "pc-1.0" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v1_0_class_init,
};

static void pc_machine_v0_15_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    pc_machine_v1_0_class_init(oc, data);
    mc->hw_version = "0.15";
    mc->name = "pc-0.15";
}

static TypeInfo pc_machine_v0_15_type_info = {
    .name = "pc-0.15" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v0_15_class_init,
};

#define PC_COMPAT_0_14 \
        {\
            .driver   = "virtio-blk-pci",\
            .property = "event_idx",\
            .value    = "off",\
        },{\
            .driver   = "virtio-serial-pci",\
            .property = "event_idx",\
            .value    = "off",\
        },{\
            .driver   = "virtio-net-pci",\
            .property = "event_idx",\
            .value    = "off",\
        },{\
            .driver   = "virtio-balloon-pci",\
            .property = "event_idx",\
            .value    = "off",\
        }

static void pc_machine_v0_14_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_0_14, 
        {
            .driver   = "qxl",
            .property = "revision",
            .value    = stringify(2),
        },{
            .driver   = "qxl-vga",
            .property = "revision",
            .value    = stringify(2),
        },
        { /* end of list */ }
    };
    pc_machine_v0_15_class_init(oc, data);
    mc->hw_version = "0.14";
    mc->name = "pc-0.14";
    machine_class_register_compat_props_array(mc, compat_props);
}

static TypeInfo pc_machine_v0_14_type_info = {
    .name = "pc-0.14" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v0_14_class_init,
};

#define PC_COMPAT_0_13 \
        {\
            .driver   = TYPE_PCI_DEVICE,\
            .property = "command_serr_enable",\
            .value    = "off",\
        },{\
            .driver   = "AC97",\
            .property = "use_broken_id",\
            .value    = stringify(1),\
        }

static void pc_compat_0_13(MachineState *machine)
{
    x86_cpu_compat_disable_kvm_features(FEAT_KVM, KVM_FEATURE_PV_EOI);
    enable_compat_apic_id_mode();
}

static void pc_machine_v0_13_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCMachineClass *pcc = PC_MACHINE_CLASS(oc);
    PCI440FXMachineClass *piixc = PC_I440FX_MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_0_13,
        /*FIXME: why the heck is the following outside PC_COMPAT_0_13? */
        {
            .driver   = "virtio-9p-pci",
            .property = "vectors",
            .value    = stringify(0),
        },{
            .driver   = "VGA",
            .property = "rombar",
            .value    = stringify(0),
        },{
            .driver   = "vmware-svga",
            .property = "rombar",
            .value    = stringify(0),
        },
        { /* end of list */ }
    };
    pc_machine_v0_14_class_init(oc, data);
    mc->hw_version = "0.13";
    mc->name = "pc-0.13";
    machine_class_register_compat_props_array(mc, compat_props);
    pcc->kvmclock_enabled = false;
    pcc->has_acpi_build = false;
    piixc->compat_func = pc_compat_0_13;
}

static TypeInfo pc_machine_v0_13_type_info = {
    .name = "pc-0.13" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v0_13_class_init,
};

#define PC_COMPAT_0_12 \
        {\
            .driver   = "virtio-serial-pci",\
            .property = "max_ports",\
            .value    = stringify(1),\
        },{\
            .driver   = "virtio-serial-pci",\
            .property = "vectors",\
            .value    = stringify(0),\
        },{\
            .driver   = "usb-mouse",\
            .property = "serial",\
            .value    = "1",\
        },{\
            .driver   = "usb-tablet",\
            .property = "serial",\
            .value    = "1",\
        },{\
            .driver   = "usb-kbd",\
            .property = "serial",\
            .value    = "1",\
        }

static void pc_machine_v0_12_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_0_12,
        /*FIXME: why this is outside PC_COMPAT_0_12? */
        {
            .driver   = "VGA",
            .property = "rombar",
            .value    = stringify(0),
        },{
            .driver   = "vmware-svga",
            .property = "rombar",
            .value    = stringify(0),
        },
        { /* end of list */ }
    };
    /*FIXME: v0_13 class_init has additional props. why? */
    pc_machine_v0_13_class_init(oc, data);
    mc->hw_version = "0.12";
    mc->name = "pc-0.12";
    machine_class_register_compat_props_array(mc, compat_props);
}

static TypeInfo pc_machine_v0_12_type_info = {
    .name = "pc-0.12" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v0_12_class_init,
};

#define PC_COMPAT_0_11 \
        {\
            .driver   = "virtio-blk-pci",\
            .property = "vectors",\
            .value    = stringify(0),\
        },{\
            .driver   = TYPE_PCI_DEVICE,\
            .property = "rombar",\
            .value    = stringify(0),\
        }

static void pc_machine_v0_11_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        PC_COMPAT_0_11,
        /*FIXME: not-reused props again. why? */
        {
            .driver   = "ide-drive",
            .property = "ver",
            .value    = "0.11",
        },{
            .driver   = "scsi-disk",
            .property = "ver",
            .value    = "0.11",
        },
        { /* end of list */ }
    };
    /*FIXME: v0_12 class_init has additional props. why? */
    pc_machine_v0_12_class_init(oc, data);
    mc->hw_version = "0.11";
    mc->name = "pc-0.11";
    machine_class_register_compat_props_array(mc, compat_props);
}

static TypeInfo pc_machine_v0_11_type_info = {
    .name = "pc-0.11" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v0_11_class_init,
};

static void pc_machine_v0_10_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        {
            .driver   = "virtio-blk-pci",
            .property = "class",
            .value    = stringify(PCI_CLASS_STORAGE_OTHER),
        },{
            .driver   = "virtio-serial-pci",
            .property = "class",
            .value    = stringify(PCI_CLASS_DISPLAY_OTHER),
        },{
            .driver   = "virtio-net-pci",
            .property = "vectors",
            .value    = stringify(0),
        },{
            .driver   = "ide-drive",
            .property = "ver",
            .value    = "0.10",
        },{
            .driver   = "scsi-disk",
            .property = "ver",
            .value    = "0.10",
        },
        { /* end of list */ }
    };
    /*FIXME: additional props */
    pc_machine_v0_11_class_init(oc, data);
    mc->hw_version = "0.10";
    mc->name = "pc-0.10";
    machine_class_register_compat_props_array(mc, compat_props);
}

static TypeInfo pc_machine_v0_10_type_info = {
    .name = "pc-0.10" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_I440FX_MACHINE,
    .class_init = pc_machine_v0_10_class_init,
};

static void pc_init_isa(MachineState *machine)
{
    if (!machine->cpu_model) {
        machine->cpu_model = "486";
    }
    x86_cpu_compat_disable_kvm_features(FEAT_KVM, KVM_FEATURE_PV_EOI);
    enable_compat_apic_id_mode();
    pc_init1(machine);
}

static void isapc_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    PCMachineClass *pcc = PC_MACHINE_CLASS(oc);

    static GlobalProperty compat_props[] = {
        { /* end of list */ }
    };
    mc->desc = "ISA-only PC";
    mc->init = pc_init_isa;
    mc->max_cpus = 1;
    mc->hot_add_cpu = NULL;
    mc->name = "isapc";
    machine_class_register_compat_props_array(mc, compat_props);
    pcc->smbios_defaults = false;
    pcc->has_acpi_build = false;
}

static TypeInfo isapc_machine_type_info = {
    .name = "isapc" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_MACHINE,
    .class_init = isapc_machine_class_init,
};

#ifdef CONFIG_XEN
static void pc_xen_hvm_init(MachineState *machine)
{
    PCIBus *bus;

    pc_init1(machine);

    bus = pci_find_primary_bus();
    if (bus != NULL) {
        pci_create_simple(bus, -1, "xen-platform");
    }
}

static void xenfv_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    static GlobalProperty compat_props[] = {
        /* xenfv has no fwcfg and so does not load acpi from QEMU.
         * as such new acpi features don't work.
         */
        {
            .driver   = "PIIX4_PM",
            .property = "acpi-pci-hotplug-with-bridge-support",
            .value    = "off",
        },
        { /* end of list */ }
    };
    mc->desc = "Xen Fully-virtualized PC";
    mc->init = pc_xen_hvm_init;
    mc->max_cpus = HVM_MAX_VCPUS;
    mc->default_machine_opts = "accel=xen";
    mc->hot_add_cpu = pc_hot_add_cpu;
    mc->name = "xenfv";
    machine_class_register_compat_props_array(mc, compat_props);
}

static TypeInfo xenfv_machine_type_info = {
    .name = "xenfv" TYPE_MACHINE_SUFFIX,
    .parent = TYPE_PC_MACHINE,
    .class_init = xenfv_machine_class_init,
};
#endif

static void pc_machine_init(void)
{
    type_register_static(&pc_i440fx_machine_type_info);
    type_register_static(&pc_i440fx_machine_v2_1_type_info);
    type_register_static(&pc_i440fx_machine_v2_0_type_info);
    type_register_static(&pc_i440fx_machine_v1_7_type_info);
    type_register_static(&pc_i440fx_machine_v1_6_type_info);
    type_register_static(&pc_i440fx_machine_v1_5_type_info);
    type_register_static(&pc_i440fx_machine_v1_4_type_info);
    type_register_static(&pc_machine_v1_3_type_info);
    type_register_static(&pc_machine_v1_2_type_info);
    type_register_static(&pc_machine_v1_1_type_info);
    type_register_static(&pc_machine_v1_0_type_info);
    type_register_static(&pc_machine_v0_15_type_info);
    type_register_static(&pc_machine_v0_14_type_info);
    type_register_static(&pc_machine_v0_13_type_info);
    type_register_static(&pc_machine_v0_12_type_info);
    type_register_static(&pc_machine_v0_11_type_info);
    type_register_static(&pc_machine_v0_10_type_info);
    type_register_static(&isapc_machine_type_info);
#ifdef CONFIG_XEN
    type_register_static(&xenfv_machine_type_info);
#endif
}

machine_init(pc_machine_init);
