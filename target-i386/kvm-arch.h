#ifndef _QEMU_I386_KVM_ARCH
#define _QEMU_I386_KVM_ARCH

/* arch-specific definitions for KVM for target-i386 */


/** This struct carries arch-specific KVMState data for target-i386
 */
struct kvm_arch_state {
    /* cpuid bits that are supported by the host */
    struct kvm_cpuid2 *host_supported_cpuid;
};

#endif /* _QEMU_I386_KVM_ARCH */
