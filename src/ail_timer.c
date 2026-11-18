/* 原AIL16槽调度：3ea8e IRQ服务与3ec17/3ebe4/3ebb4控制数学。
 * 同一个timer拥有status/period/accumulator/pending；不含CPU/PIC逐头计费。 */
#include "ail_timer.h"
#include <stddef.h>
#include <string.h>

static int handle_slot(uint32_t handle,unsigned *slot) {
    if(handle>60u || (handle&3u))return -1;
    *slot=(unsigned)(handle/4u);return 0;
}
uint32_t fd2_ail_timer_divisor(uint32_t base_us) {
    return base_us<FD2_AIL_TIMER_BIOS_US?
        (uint32_t)(((uint64_t)base_us*UINT64_C(10000))/UINT64_C(8380)):0u;
}
void fd2_ail_timer_defer_enter(fd2_ail_timer *t) {if(t)t->defer_depth+=1u;}
void fd2_ail_timer_defer_leave(fd2_ail_timer *t) {if(t)t->defer_depth-=1u;}

static void timer_rebase(fd2_ail_timer *t) {
    uint32_t base=UINT32_MAX;
    fd2_ail_timer_defer_enter(t);
    for(unsigned i=0;i<FD2_AIL_TIMER_SLOTS;++i)
        if(t->slots[i].status && t->slots[i].period_us<base)base=t->slots[i].period_us;
    if(base!=t->base_us) {
        t->base_us=base;t->pit_divisor=fd2_ail_timer_divisor(base);
        if(t->hooks.program_pit)t->hooks.program_pit(t,t->pit_divisor,t->hooks.user);
        for(unsigned i=0;i<FD2_AIL_TIMER_SLOTS;++i) {
            if(t->hooks.before_acc_clear)t->hooks.before_acc_clear(t,i,t->hooks.user);
            t->slots[i].accumulator_us=0;
        }
    }
    fd2_ail_timer_defer_leave(t);
}
void fd2_ail_timer_fresh_init(fd2_ail_timer *t,const fd2_ail_timer_hooks *hooks) {
    fd2_ail_timer_hooks copy;if(!t)return;
    if(hooks)copy=*hooks;else memset(&copy,0,sizeof copy);
    memset(t,0,sizeof *t);t->hooks=copy;t->base_us=UINT32_MAX;t->slots[15].status=2;
    (void)fd2_ail_timer_set_period(t,60,FD2_AIL_TIMER_BIOS_US);
}
uint32_t fd2_ail_timer_register(fd2_ail_timer *t,fd2_ail_timer_callback callback) {
    if(!t||!callback)return FD2_AIL_TIMER_INVALID;
    uint32_t handle=FD2_AIL_TIMER_INVALID;
    fd2_ail_timer_defer_enter(t);
    for(unsigned i=0;i<FD2_AIL_TIMER_CALLBACK_SLOTS;++i)if(!t->slots[i].status) {
        t->slots[i].status=1;t->slots[i].callback=callback;handle=i*4u;break;
    }
    fd2_ail_timer_defer_leave(t);return handle;
}
int fd2_ail_timer_set_user(fd2_ail_timer *t,uint32_t handle,void *user,void **previous) {
    unsigned slot;if(!t||handle_slot(handle,&slot))return -1;
    fd2_ail_timer_defer_enter(t);void *old=t->slots[slot].user;t->slots[slot].user=user;
    fd2_ail_timer_defer_leave(t);if(previous)*previous=old;return 0;
}
static int timer_start(fd2_ail_timer *t,uint32_t handle,int start) {
    unsigned slot;if(!t||(handle!=FD2_AIL_TIMER_INVALID&&handle_slot(handle,&slot)))return -1;
    fd2_ail_timer_defer_enter(t);
    if(handle!=FD2_AIL_TIMER_INVALID) {
        if(start&&t->slots[slot].status==1)t->slots[slot].status=2;
        else if(!start&&t->slots[slot].status==2)t->slots[slot].status=1;
    }
    fd2_ail_timer_defer_leave(t);return 0;
}
int fd2_ail_timer_start(fd2_ail_timer *t,uint32_t handle) {return timer_start(t,handle,1);}
int fd2_ail_timer_stop(fd2_ail_timer *t,uint32_t handle) {return timer_start(t,handle,0);}
int fd2_ail_timer_release(fd2_ail_timer *t,uint32_t handle) {
    unsigned slot;if(!t||(handle!=FD2_AIL_TIMER_INVALID&&handle_slot(handle,&slot)))return -1;
    fd2_ail_timer_defer_enter(t);
    if(handle!=FD2_AIL_TIMER_INVALID)t->slots[slot].status=0;
    fd2_ail_timer_defer_leave(t);return 0; /* pending/period/user保原写集。 */
}
int fd2_ail_timer_release_all(fd2_ail_timer *t) {
    if(!t)return -1;fd2_ail_timer_defer_enter(t);
    for(unsigned handle=56;;handle-=4u) {
        (void)fd2_ail_timer_release(t,handle);if(!handle)break;
    }
    fd2_ail_timer_defer_leave(t);return 0;
}
int fd2_ail_timer_set_period(fd2_ail_timer *t,uint32_t handle,uint32_t period) {
    unsigned slot;if(!t||handle_slot(handle,&slot))return -1;
    fd2_ail_timer_defer_enter(t);t->slots[slot].period_us=period;t->slots[slot].accumulator_us=0;
    timer_rebase(t);fd2_ail_timer_defer_leave(t);return 0;
}
int fd2_ail_timer_set_frequency(fd2_ail_timer *t,uint32_t handle,uint32_t hz) {
    unsigned slot;if(!t||!hz||handle_slot(handle,&slot))return -1;
    return fd2_ail_timer_set_period(t,handle,UINT32_C(1000000)/hz);
}
int fd2_ail_timer_irq(fd2_ail_timer *t) {
    if(!t)return -1;
    if(t->busy){if(t->hooks.ack_irq)t->hooks.ack_irq(t,t->hooks.user);return 0;}
    t->busy+=1u;const uint32_t base=t->base_us;
    for(unsigned i=0;i<FD2_AIL_TIMER_SLOTS;++i) {
        fd2_ail_timer_slot *slot=&t->slots[i];
        if(slot->status==2u) {
            uint32_t value=slot->accumulator_us+base;
            if(value>=slot->period_us){value-=slot->period_us;slot->pending+=1u;}
            slot->accumulator_us=value;
        }
    }
    if(t->hooks.ack_irq)t->hooks.ack_irq(t,t->hooks.user);
    if(!t->defer_depth||t->defer_depth>=UINT32_C(0x80000000)) {
        for(unsigned i=0;i<FD2_AIL_TIMER_CALLBACK_SLOTS;++i) {
            fd2_ail_timer_slot *slot=&t->slots[i];
            while(slot->pending) {
                slot->pending-=1u;
                if(!slot->callback){t->busy-=1u;return -1;}
                slot->callback(slot->user);
            }
        }
    }
    if(t->slots[15].pending) {
        t->slots[15].pending-=1u;t->busy-=1u;
        if(t->hooks.bios_chain)t->hooks.bios_chain(t,t->hooks.user);
    }else t->busy-=1u;
    return 1;
}
