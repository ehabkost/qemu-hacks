/* Device reset handler function registration, used by qdev and other code */
#ifndef QDEV_RESET_H
#define QDEV_RESET_H

typedef void QEMUResetHandler(void *opaque);

void qemu_register_reset(QEMUResetHandler *func, void *opaque);
void qemu_unregister_reset(QEMUResetHandler *func, void *opaque);
void qemu_devices_reset(void);

#endif /* QDEV_RESET_H */
