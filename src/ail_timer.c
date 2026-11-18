/* 3ea8e/3ec17/3ebb4的游戏所用累计关系：音乐120Hz、音效200Hz。
 * 保持共同base和取整结果；没有独立硬件槽、pending或IRQ状态。 */
#include "ail_timer.h"
#include <string.h>

void fd2_ail_timer_fresh_init(fd2_ail_timer *t) {
    if(!t)return;
    memset(t,0,sizeof *t);
    t->base_us=FD2_AIL_TIMER_BIOS_US;t->interval_count=65536u;
}
int fd2_ail_timer_install(fd2_ail_timer *t,uint32_t service,
        fd2_ail_timer_callback callback,void *user) {
    if(!t || service>=FD2_AIL_TIMER_SLOTS || !callback || t->slots[service].callback)return -1;
    fd2_ail_timer_slot *slot=&t->slots[service];
    slot->period_us=service==FD2_AIL_TIMER_MDI?8333u:5000u;
    slot->accumulator_us=0;slot->callback=callback;slot->user=user;
    if(slot->period_us<t->base_us) {
        t->base_us=slot->period_us;
        /* 原整数换算保留为兼容时基，不建立可编程PIT。 */
        t->interval_count=(uint32_t)((uint64_t)t->base_us*10000u/8380u);
        for(unsigned i=0;i<FD2_AIL_TIMER_SLOTS;i++)t->slots[i].accumulator_us=0;
        t->bios_accumulator_us=0;
    }
    return 0;
}
int fd2_ail_timer_release(fd2_ail_timer *t,uint32_t service) {
    if(!t || (service!=FD2_AIL_TIMER_INVALID && service>=FD2_AIL_TIMER_SLOTS))return -1;
    if(service!=FD2_AIL_TIMER_INVALID) {
        t->slots[service].callback=NULL;t->slots[service].user=NULL;
    }
    return 0;
}
int fd2_ail_timer_step(fd2_ail_timer *t) {
    if(!t)return -1;
    unsigned due[FD2_AIL_TIMER_SLOTS]={0};
    for(unsigned i=0;i<FD2_AIL_TIMER_SLOTS;i++) {
        fd2_ail_timer_slot *slot=&t->slots[i];
        if(slot->callback) {
            slot->accumulator_us+=t->base_us;
            if(slot->accumulator_us>=slot->period_us) {
                slot->accumulator_us-=slot->period_us;due[i]=1;
            }
        }
    }
    t->bios_accumulator_us+=t->base_us;
    int bios_due=t->bios_accumulator_us>=FD2_AIL_TIMER_BIOS_US;
    if(bios_due)t->bios_accumulator_us-=FD2_AIL_TIMER_BIOS_US;
    for(unsigned i=0;i<FD2_AIL_TIMER_SLOTS;i++)
        if(due[i])t->slots[i].callback(t->slots[i].user);
    return bios_due;
}
