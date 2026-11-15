/* 10652/11eee/4eee0/24d22有限资源与CPU终态，真实原FDOTHER作输入。
 * 不启动SDL窗口，不把共享解码器或C自测当成原版GUI/逐写差分。 */
#include "field_background.h"
#include "image.h"
#include "bios_clock.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t actual[312*192];

/* 独立有理数oracle，最大tick65536；不用生产clock函数制造测试输入。 */
static uint64_t tick_ns(uint64_t tick) {
    return (tick*65536u*UINT64_C(1000000000)+1193181u)/1193182u;
}

static void read_view(fd2_field_page *page) {
    assert(fd2_field_page_read(page,72,72,312,192,actual,sizeof(actual),312)==0);
}

static void assert_wave(fd2_field_page *page,const uint8_t *raw,unsigned phase) {
    static const unsigned source_wave[16]={2,3,3,4,4,4,3,3,2,1,1,0,0,0,1,1};
    read_view(page);
    for(size_t y=0;y<192;y++)for(size_t x=0;x<312;x++)
        assert(actual[y*312+x]==raw[4+y*320+x+source_wave[(y+phase)&15u]]);
}

static void test_wave_tick_and_rebuild(const fd2_archive *other) {
    const uint8_t *raw;size_t length;
    assert(fd2_archive_get(other,15,&raw,&length)==0&&length==64004);
    fd2_field_page *page=fd2_field_page_create();assert(page);
    assert(fd2_field_background_rebuild(page,other,9)==0);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(1),1)==0);
    assert_wave(page,raw,0);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(1),1)==0);
    assert_wave(page,raw,0);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(2),1)==0);
    assert_wave(page,raw,1);
    fd2_field_page *copy=fd2_field_page_clone(page);assert(copy);
    assert(fd2_field_background_draw(copy,72,72,0,0,0,0,tick_ns(3),1)==0);
    assert_wave(copy,raw,2);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(2),1)==0);
    assert_wave(page,raw,1); /* work指针/phase不可浅别名。 */
    fd2_field_page_destroy(copy);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(163),0)==0);
    assert_wave(page,raw,1); /* 只读重画不推进独立539F8/539FC。 */
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,0,1)==0);
    assert_wave(page,raw,2); /* 原不同tick一次，无追赶。 */
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(163),1)==0);
    assert_wave(page,raw,3);
    assert(fd2_field_background_rebuild(page,other,24)==0);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(164),1)==0);
    assert_wave(page,raw,4); /* 10652不清App phase。 */
    fd2_archive empty={0};
    assert(fd2_field_background_rebuild(page,&empty,28)==-1);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(164),1)==0);
    assert_wave(page,raw,4); /* 坏新资源不得丢旧owner。 */
    assert(fd2_field_background_draw(page,-1,72,0,0,0,0,tick_ns(165),1)==-1);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(165),1)==0);
    assert_wave(page,raw,5); /* 坏目标不得提前推进phase。 */
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(32767),1)==0);
    assert_wave(page,raw,6);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(32768),1)==0);
    assert_wave(page,raw,7);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(65536),1)==0);
    assert_wave(page,raw,8);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,0,1)==0);
    assert_wave(page,raw,8); /* 完整低WORD回绕后是同tick。 */
    assert(fd2_field_background_rebuild(page,other,33)==-1);
    fd2_field_page_destroy(page);
}

static void test_parallax_and_no_background(const fd2_archive *other) {
    fd2_image top={0},bottom={0};
    assert(fd2_image_decode_entry(&top,other,16)==0);
    assert(fd2_image_decode_entry(&bottom,other,17)==0);
    fd2_field_page *page=fd2_field_page_create();assert(page);
    assert(fd2_field_background_rebuild(page,other,17)==0);
    /* 原3*cameraX+panX/2，2*cameraY+panY/3；正负除法均向0。 */
    assert(fd2_field_background_draw(page,72,72,3,7,5,-5,tick_ns(1),1)==0);
    read_view(page);
    for(size_t y=0;y<192;y++)for(size_t x=0;x<312;x++) {
        size_t sy=y+13,sx=x+11;
        const fd2_image *image=sy<113?&top:&bottom;
        if(sy>=113)sy-=113;
        assert(actual[y*312+x]==image->pixels[sy*462+sx]);
    }
    uint8_t before[sizeof(actual)];memcpy(before,actual,sizeof(actual));
    assert(fd2_field_background_draw(page,72,72,100,100,0,0,tick_ns(1),1)==-1);
    read_view(page);assert(!memcmp(actual,before,sizeof(actual)));
    assert(fd2_field_background_rebuild(page,other,0)==0);
    assert(fd2_field_background_draw(page,72,72,100,100,0,0,tick_ns(1),1)==0);
    read_view(page);assert(!memcmp(actual,before,sizeof(actual)));
    fd2_image_free(&top);fd2_image_free(&bottom);fd2_field_page_destroy(page);
}

static void assert_scroll(fd2_field_page *page,const fd2_image *source,unsigned amount) {
    read_view(page);
    for(size_t y=0;y<192;y++)for(size_t x=0;x<312;x++)
        assert(actual[y*312+x]==source->pixels[((y+192-amount)%192)*312+x]);
}

static void test_scroll_speed_and_app_persistence(const fd2_archive *other) {
    fd2_image source={0};assert(fd2_image_decode_entry(&source,other,42)==0);
    fd2_field_page *page=fd2_field_page_create();assert(page);
    assert(fd2_field_background_rebuild(page,other,23)==0);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,0,0)==0);
    assert_scroll(page,&source,1); /* 10652必调用24d22(0)，不是首次11eee才转。 */
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(1),1)==0);
    assert_scroll(page,&source,2);
    assert(fd2_field_background_scroll(page,3)==0);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(2),0)==0);
    assert_scroll(page,&source,2); /* 非0setter本身不转。 */
    assert(fd2_field_background_scroll(page,0)==0);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(1),1)==0);
    assert_scroll(page,&source,5); /* 显式24d22不改tick，同tick不再转。 */
    assert(fd2_field_background_rebuild(page,other,0)==0);
    assert(fd2_field_background_rebuild(page,other,23)==0);
    assert(fd2_field_background_draw(page,72,72,0,0,0,0,tick_ns(1),0)==0);
    assert_scroll(page,&source,3); /* 换stage也不清51A10。 */
    fd2_field_page_destroy(page);fd2_image_free(&source);
}

static void test_resource_owner_separate_from_target(const fd2_archive *other) {
    fd2_field_page *owner=fd2_field_page_create(),*first=fd2_field_page_create(),*second=fd2_field_page_create();
    assert(owner&&first&&second);const uint8_t *raw;size_t size;
    assert(fd2_archive_get(other,15,&raw,&size)==0);
    assert(fd2_field_background_rebuild(owner,other,9)==0);
    assert(fd2_field_background_draw_from(owner,first,72,72,0,0,0,0,tick_ns(1),1)==0);
    assert_wave(first,raw,0);uint8_t pixel=1;
    assert(fd2_field_page_read(owner,72,72,1,1,&pixel,1,1)==0 && pixel==0);
    assert(fd2_field_background_draw_from(owner,second,72,72,0,0,0,0,tick_ns(1),1)==0);
    assert_wave(second,raw,0); /* 目的页不重启539F8/539FC。 */
    assert(fd2_field_background_draw_from(owner,first,72,72,0,0,0,0,tick_ns(2),1)==0);
    assert_wave(first,raw,1);
    assert(fd2_field_page_copy_pixels(owner,second)==0); /* 只像素，不能回滚资源clock。 */
    assert(fd2_field_background_draw_from(owner,second,72,72,0,0,0,0,tick_ns(3),1)==0);
    assert_wave(second,raw,2);
    fd2_field_page_destroy(second);fd2_field_page_destroy(first);fd2_field_page_destroy(owner);
}

int main(void) {
    fd2_bios_clock_reset(0,0);
    fd2_archive other={0};assert(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    test_wave_tick_and_rebuild(&other);
    test_parallax_and_no_background(&other);
    test_scroll_speed_and_app_persistence(&other);
    test_resource_owner_separate_from_target(&other);
    fd2_archive_close(&other);puts("field_background: 4 groups passed");return 0;
}
