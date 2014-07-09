/* TYPE_X86_ACCEL interface
 */
#ifndef HW_I386_ACCEL_H
#define HW_I386_ACCEL_H

#include "hw/accel.h"
#include "qemu/typedefs.h"

#define TYPE_X86_ACCEL "x86-accel"

#define X86_ACCEL_CLASS(klass) \
     OBJECT_CLASS_CHECK(X86AccelClass, (klass), TYPE_X86_ACCEL)
#define X86_ACCEL_GET_CLASS(obj) \
     OBJECT_GET_CLASS(X86AccelClass, (obj), TYPE_X86_ACCEL)
#define X86_ACCEL(obj) \
     INTERFACE_CHECK(X86Accel, (obj), TYPE_X86_ACCEL)


typedef struct X86Accel {
    /* <private> */
    Object Parent;
} X86Accel;

/**
 * HotplugDeviceClass:
 *
 * Interface that may be implemented by target-specific accelerator
 * classes.
 *
 * @cpu_post_init: Hook called after creation of X86CPU objects. Can be used,
 *                 to change CPUID data to match accelerator-specific
 *                 requirements.
 */
typedef struct X86AccelClass {
    /* <private> */
    InterfaceClass parent;
    /* <public> */
    void (*cpu_post_init)(AccelState *accel, X86CPU *cpu, Error **errp);
} X86AccelClass;


#endif
