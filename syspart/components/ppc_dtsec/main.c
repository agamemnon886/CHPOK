#include "DTSEC_NET_DEV_gen.h"

#ifdef __PPC__
int fm_eth_send(struct fm_eth *fm_eth, void *buf, int len);
void dtsec_init(DTSEC_NET_DEV *self);
int fm_eth_recv(DTSEC_NET_DEV *self);
#endif /* PPC */

// DTSEC is real hardware (Freescale/NXP QorIQ) only; on other archs
// (e.g. x86-qemu, which uses VIRTIO_NET_DEV instead) this component
// is compiled but never instantiated, so these are no-ops.

ret_t dtsec_send_frame(DTSEC_NET_DEV *self, char *buffer, size_t size, size_t max_back_step)
{
    printf("DTSEC %s\n", __func__);

#ifdef __PPC__
    fm_eth_send(self->state.dev_state.current_fm, buffer, size);
#endif
    return 0;
}

ret_t dtsec_flush_send(DTSEC_NET_DEV *self)
{
    //Empty
    return EOK;
}


void dtsec_component_init(DTSEC_NET_DEV *self)
{
#ifdef __PPC__
    dtsec_init(self);
#endif
    printf("DTSEC init\n");
}

void dtsec_receive_activity(DTSEC_NET_DEV *self)
{
#ifdef __PPC__
    fm_eth_recv(self);
#endif
}
