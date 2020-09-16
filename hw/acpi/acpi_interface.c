#include "qemu/osdep.h"
#include "hw/acpi/acpi_dev_interface.h"
#include "qemu/module.h"

void acpi_send_event(DeviceState *dev, AcpiEventStatusBits event)
{
    AcpiDeviceIfClass *adevc = ACPI_DEVICE_IF_GET_CLASS(dev);
    if (adevc->send_event) {
        AcpiDeviceIf *adev = ACPI_DEVICE_IF(dev);
        adevc->send_event(adev, event);
    }
}

OBJECT_DEFINE_TYPE_EXTENDED(acpi_dev_if_info,
                            void, AcpiDeviceIfClass,
                            ACPI_DEVICE_IF, INTERFACE)
