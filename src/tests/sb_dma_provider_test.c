/* SPDX-License-Identifier: MIT
 * 原源独立期望：count-trace01的Fraction源点格、SB16.DIG状态literal和
 * 官方普通C6 block/ACK/FillUp关系。无设备、GUI、guest或wall clock。
 */
#include "sb_dma_provider.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line%d: %s\n",__LINE__,#x); return 1; } } while(0)
typedef struct sink {
    fd2_sb_dma_provider *sb;
    uint8_t *ring;
    size_t expected_cursor, reads, add_calls, fill_calls;
    int fail_add, fail_fill, fill_read;
    int fill_modes[16];
} sink;
static uint8_t ring[FD2_SB_RING];
static const fd2_sb_point point = { 10, 0.125, 0.75, 0 };
static int add(void *user, const uint8_t *data, size_t n)
{
    sink *s = (sink *)user;
    size_t i;
    s->add_calls++;
    for (i=0;i<n;i++) {
        if (data[i] != s->ring[(s->expected_cursor+i)%FD2_SB_RING]) return -1;
    }
    s->expected_cursor = (s->expected_cursor+n)%FD2_SB_RING;
    s->reads += n;
    return s->fail_add ? -1 : 0;
}
static int fill(void *user)
{
    sink *s = (sink *)user;
    int n = s->fill_read;
    if (s->fill_calls < 16) s->fill_modes[s->fill_calls] = s->sb->mode;
    s->fill_calls++;
    s->fill_read = 0;
    if (s->fail_fill) return -1;
    return n ? fd2_sb_dma_generate(s->sb,(size_t)n) : 0;
}
static void setup(fd2_sb_dma_provider *s, sink *u)
{
    size_t i;
    for(i=0;i<sizeof(ring);i++) ring[i]=(uint8_t)((i*13u+7u)&255u);
    memset(u,0,sizeof(*u)); u->sb=s; u->ring=ring;
    if(fd2_sb_dma_init(s,1,u,add,fill)) abort();
}
static int start(fd2_sb_dma_provider *s) { return fd2_sb_dma_start(s,ring,sizeof(ring),&point); }
static int first_end(fd2_sb_dma_provider *s)
{
    return fd2_sb_dma_generate(s,1024) || fd2_sb_dma_generate(s,1024);
}
static int fresh_and_c6(void)
{
    fd2_sb_dma_provider s; sink u; int16_t half = -9;
    setup(&s,&u);
    CHECK(s.driver_half==0 && !s.left && !s.end.pending);
    CHECK(start(&s)==0);
    CHECK(u.fill_calls==2 && u.fill_modes[0]==FD2_SB_MASKED && u.fill_modes[1]==FD2_SB_MASKED);
    CHECK(s.left==2048 && s.mode==FD2_SB_DMA && s.driver_state==3);
    CHECK(fd2_sb_dma_generate(&s,24)==0 && s.cursor==24 && s.left==2024);
    CHECK(!s.end.pending && !s.irq_pending);
    CHECK(fd2_sb_dma_read_half(&s,&half)==0 && half==0);
    /* min66不是每次Generate的强制尾。 */
    CHECK(fd2_sb_dma_generate(&s,1024)==0);
    CHECK(fd2_sb_dma_generate(&s,940)==0 && s.left==60);
    CHECK(!s.end.pending);
    CHECK(fd2_sb_dma_generate(&s,1)==0 && s.left==59 && !s.end.pending);
    CHECK(fd2_sb_dma_generate(&s,500)==0 && s.cursor==2048);
    CHECK(s.left==2048 && s.irq_pending && s.driver_half==0 && s.block_ends==1);
    CHECK(fd2_sb_dma_deliver_irq(&s)==0 && s.driver_half==1 && !s.irq_pending);
    CHECK(u.reads==2048);
    return 0;
}
static int unacked_second_block(void)
{
    fd2_sb_dma_provider s; sink u; size_t old;
    setup(&s,&u); CHECK(start(&s)==0 && first_end(&s)==0 && first_end(&s)==0);
    CHECK(s.mode==FD2_SB_ACK && s.cursor==0 && s.left==2048 && s.block_ends==2);
    old=u.reads; CHECK(fd2_sb_dma_generate(&s,24)==0 && u.reads==old);
    CHECK(fd2_sb_dma_deliver_irq(&s)==0);
    CHECK(s.mode==FD2_SB_DMA && s.driver_half==1 && u.fill_calls==3);
    CHECK(u.fill_modes[2]==FD2_SB_ACK);
    return 0;
}
static int pause_and_resume(void)
{
    fd2_sb_dma_provider s; sink u; size_t fills;
    setup(&s,&u); CHECK(start(&s)==0);
    CHECK(fd2_sb_dma_request_pause(&s)==0 && s.driver_state==2 && s.mode==FD2_SB_DMA);
    /* next cue在IRQ前只取消待暂停，不重program，DMA前缀仍同源。 */
    CHECK(start(&s)==0 && s.driver_state==3 && u.fill_calls==2);
    CHECK(fd2_sb_dma_request_pause(&s)==0 && first_end(&s)==0);
    u.fill_read=7;
    CHECK(fd2_sb_dma_deliver_irq(&s)==0);
    CHECK(s.mode==FD2_SB_PAUSED && s.left==2041 && s.cursor==2055);
    CHECK(s.saved_next_half==2 && s.driver_half==-1 && s.driver_state==0);
    fills=u.fill_calls;
    CHECK(start(&s)==0 && s.driver_half==1 && s.driver_state==3 && s.left==2041);
    CHECK(s.mode==FD2_SB_DMA && s.cursor==2055 && !s.end.pending);
    CHECK(u.fill_calls==fills+2 && u.fill_modes[fills]==FD2_SB_PAUSED && u.fill_modes[fills+1]==FD2_SB_MASKED);
    return 0;
}
static int short_resume_and_event(int finish_by_mixer)
{
    fd2_sb_dma_provider s; sink u; uint64_t serial; uint32_t bits;
    fd2_sb_point p = point;
    setup(&s,&u); CHECK(start(&s)==0 && first_end(&s)==0);
    /* 外部迟交付首IRQ；第二块尚未结束，故未进入ACK停机。 */
    CHECK(fd2_sb_dma_generate(&s,1024)==0 && fd2_sb_dma_generate(&s,964)==0);
    CHECK(s.left==60 && s.irq_pending && !s.end.pending);
    CHECK(fd2_sb_dma_request_pause(&s)==0 && fd2_sb_dma_deliver_irq(&s)==0);
    CHECK(s.left==60 && s.mode==FD2_SB_PAUSED);
    p.servicing_event=1;
    CHECK(fd2_sb_dma_start(&s,ring,sizeof(ring),&p)==0);
    CHECK(s.end.pending && s.end.count==60 && s.end.tick==10);
    memcpy(&bits,&s.end.delay_ms,sizeof(bits));
    CHECK(bits==0x402e2650u);
    CHECK(s.end.index==0.75+(double)s.end.delay_ms);
    serial=s.end.serial;
    CHECK(fd2_sb_dma_generate(&s,20)==0 && s.left==40 && s.end.pending);
    if(finish_by_mixer) {
        CHECK(fd2_sb_dma_generate(&s,40)==0 && !s.end.pending && s.irq_pending);
        CHECK(fd2_sb_dma_dispatch_end(&s,serial)==FD2_SB_H_EVENT);
    } else {
        CHECK(fd2_sb_dma_dispatch_end(&s,serial)==0);
        CHECK(s.irq_pending && s.left==2048 && !s.end.pending);
    }
    CHECK(s.cursor==0 && s.block_ends==2 && u.reads==4096);
    return 0;
}
static int first_error_prefix(void)
{
    fd2_sb_dma_provider s; sink u; fd2_sb_dma_provider before;
    setup(&s,&u); u.fail_fill=1;
    CHECK(start(&s)==FD2_SB_H_FILLUP && s.mode==FD2_SB_MASKED && s.left==0);
    before=s; CHECK(fd2_sb_dma_generate(&s,10)==FD2_SB_H_FILLUP && !memcmp(&s,&before,sizeof(s)));
    setup(&s,&u); CHECK(start(&s)==0); u.fail_add=1;
    CHECK(fd2_sb_dma_generate(&s,24)==FD2_SB_H_SINK);
    CHECK(s.cursor==24 && s.dma_reads==24 && s.left==2048 && !s.irq_pending);
    before=s; CHECK(fd2_sb_dma_request_pause(&s)==FD2_SB_H_SINK && !memcmp(&s,&before,sizeof(s)));
    return 0;
}
static int borrowed_ring_and_clamp(void)
{
    fd2_sb_dma_provider s; sink u; size_t i;
    setup(&s,&u); CHECK(start(&s)==0);
    CHECK(fd2_sb_dma_generate(&s,9999)==0 && s.dma_reads==1024 && s.left==1024);
    for(i=1024;i<2048;i++) ring[i]=0;
    CHECK(fd2_sb_dma_generate(&s,1024)==0 && u.reads==2048);
    CHECK(fd2_sb_dma_deliver_irq(&s)==0 && first_end(&s)==0);
    CHECK(s.cursor==0 && s.dma_reads==4096);
    CHECK(fd2_sb_dma_deliver_irq(&s)==0 && fd2_sb_dma_generate(&s,24)==0 && s.cursor==24);
    return 0;
}
static int source_request_tape(const char *path)
{
    fd2_sb_dma_provider s; sink u; FILE *f; unsigned long request; unsigned calls=0;
    setup(&s,&u); CHECK(start(&s)==0);
    f=fopen(path,"r"); CHECK(f!=NULL);
    while(fscanf(f,"%lu",&request)==1) {
        CHECK(fd2_sb_dma_generate(&s,(size_t)request)==0); calls++;
        if(calls==92) CHECK(s.dma_reads==2031 && s.left==17 && !s.irq_pending);
    }
    CHECK(!ferror(f) && fclose(f)==0);
    CHECK(calls==94 && s.dma_reads==2053 && s.left==2043 && s.irq_pending && s.driver_half==0);
    return 0;
}
int main(int argc, char **argv)
{
    CHECK(argc==2);
    CHECK(!fresh_and_c6()); CHECK(!unacked_second_block());
    CHECK(!pause_and_resume()); CHECK(!short_resume_and_event(0));
    CHECK(!short_resume_and_event(1)); CHECK(!first_error_prefix());
    CHECK(!borrowed_ring_and_clamp()); CHECK(!source_request_tape(argv[1]));
    puts("PASS 8 SB provider families; no device/guest/clock read; no mixed-output claim");
    return 0;
}
