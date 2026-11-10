/* 11cac/4e22a工作页写掩码与投影边界；合成RLE独立标出写/不写域，
 * 不用截图或清零解码图作为透明历史的预期。 */
#include "field_page.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void test_write_mask_and_history(void) {
    fd2_field_page *page = fd2_field_page_create();
    assert(page);
    uint8_t previous[8] = {11,12,13,14,15,16,17,18};
    /* skip1; opaque0/9; 隔点RUN写第4/6列；末列skip。 */
    const uint8_t rle[] = {0xc0,0x81,0,9,0x41,7,0xc0};
    assert(fd2_field_page_write(page,71,71,8,1,previous,8,8) == 0);
    assert(fd2_field_page_blit_rle(page,71,71,8,1,rle,sizeof(rle),NULL) == 0);
    uint8_t actual[8];
    assert(fd2_field_page_read(page,71,71,8,1,actual,8,8) == 0);
    const uint8_t expected[] = {11,0,9,14,7,16,7,18};
    assert(memcmp(actual,expected,8) == 0);
    uint8_t lut[256];
    for (size_t i=0;i<256;i++) lut[i]=(uint8_t)(255-i);
    assert(fd2_field_page_blit_rle(page,71,71,8,1,rle,sizeof(rle),lut) == 0);
    assert(fd2_field_page_read(page,71,71,8,1,actual,8,8) == 0);
    const uint8_t remapped[] = {11,255,246,14,248,16,248,18};
    assert(memcmp(actual,remapped,8) == 0);
    fd2_field_page *copy=fd2_field_page_clone(page);
    assert(copy && fd2_field_page_blit_rle(copy,71,71,8,1,rle,sizeof(rle),NULL)==0);
    assert(fd2_field_page_read(page,71,71,8,1,actual,8,8)==0);
    assert(memcmp(actual,remapped,8)==0);
    fd2_field_page_destroy(copy);
    /* 下一次整条SKIP保留上一次写出的有定义历史，包括opaque0的映色。 */
    const uint8_t skip[] = {0xc7};
    assert(fd2_field_page_blit_rle(page,71,71,8,1,skip,1,NULL) == 0);
    assert(fd2_field_page_read(page,71,71,8,1,actual,8,8) == 0);
    assert(memcmp(actual,remapped,8) == 0);
    fd2_field_page_destroy(page);
}

static void test_malformed_is_atomic(void) {
    fd2_field_page *page=fd2_field_page_create();
    uint8_t before[8]={81,82,83,84,85,86,87,88}, actual[8];
    assert(page && fd2_field_page_write(page,2,2,4,2,before,8,4)==0);
    /* 第一行可解，第二行截断：不得留下第一行的写。 */
    const uint8_t truncated[]={3,1,0x83,2};
    const uint8_t overrun[]={4,1,3,1};
    const uint8_t trailer[]={3,1,3,1,0xff};
    const uint8_t *invalid[]={truncated,overrun,trailer};
    const size_t sizes[]={sizeof(truncated),sizeof(overrun),sizeof(trailer)};
    for(size_t i=0;i<3;i++) {
        assert(fd2_field_page_blit_rle(page,2,2,4,2,invalid[i],sizes[i],NULL)==-1);
        assert(fd2_field_page_read(page,2,2,4,2,actual,8,4)==0);
        assert(memcmp(actual,before,8)==0);
    }
    assert(fd2_field_page_validate_rle(0,1,trailer,sizeof(trailer))==-1);
    assert(fd2_field_page_validate_rle(4097,1,trailer,sizeof(trailer))==-1);
    assert(fd2_field_page_blit_rle(NULL,0,0,4,2,trailer,sizeof(trailer),NULL)==-1);
    fd2_field_page_destroy(page);
}

static void test_projection_and_scratch(void) {
    fd2_field_page *page=fd2_field_page_create();
    static uint8_t owner[FD2_FIELD_PAGE_SIZE];
    static uint8_t display[320*200], before[320*200];
    for(size_t i=0;i<sizeof(owner);i++) owner[i]=(uint8_t)(i*17u+i/456u);
    assert(page && fd2_field_page_write(page,0,0,456,336,owner,sizeof(owner),456)==0);
    memset(display,203,sizeof(display));
    /* up-follow末phase源点y48；证明读的是同页边缘历史，不重建camera图。 */
    assert(fd2_field_page_project(page,72,48,display,sizeof(display))==0);
    for(int y=0;y<200;y++) for(int x=0;x<320;x++) {
        uint8_t expected=x>=4&&x<316&&y>=4&&y<196 ?
            owner[(size_t)(y-4+48)*456+(size_t)(x-4+72)] : 203;
        assert(display[y*320+x]==expected);
    }
    memcpy(before,display,sizeof(before));
    assert(fd2_field_page_project(page,145,48,display,sizeof(display))==-1);
    assert(fd2_field_page_project(page,72,145,display,sizeof(display))==-1);
    assert(fd2_field_page_project(page,72,48,display,sizeof(display)-1)==-1);
    assert(memcmp(before,display,sizeof(before))==0);
    uint8_t scratch[16]; memset(scratch,99,sizeof(scratch));
    assert(fd2_field_page_read(page,72,72,3,2,scratch,sizeof(scratch),8)==0);
    assert(scratch[3]==99&&scratch[7]==99&&scratch[11]==99);
    scratch[1]=231;
    assert(fd2_field_page_write(page,72,72,3,2,scratch,sizeof(scratch),8)==0);
    uint8_t got[6];
    assert(fd2_field_page_read(page,72,72,3,2,got,6,3)==0);
    assert(got[1]==231 && got[4]==owner[73u*456u+73u]);
    assert(fd2_field_page_write(page,0,0,3,2,scratch,10,8)==-1);
    assert(fd2_field_page_read(page,INT_MAX,0,3,2,scratch,16,8)==-1);
    assert(fd2_field_page_read(page,0,INT_MAX,3,2,scratch,16,8)==-1);
    assert(fd2_field_page_read(page,0,0,3,2,scratch,16,SIZE_MAX)==-1);
    fd2_field_page_destroy(page);
}

static void test_owner_clipping(void) {
    fd2_field_page *page=fd2_field_page_create();
    const uint8_t rle[]={3,7,3,8};
    uint8_t actual[4];
    assert(page && fd2_field_page_blit_rle(page,-2,335,4,2,rle,4,NULL)==0);
    assert(fd2_field_page_read(page,0,335,4,1,actual,4,4)==0);
    assert(actual[0]==7&&actual[1]==7&&actual[2]==0&&actual[3]==0);
    assert(fd2_field_page_blit_rle(page,INT_MAX,INT_MAX,4,2,rle,4,NULL)==0);
    fd2_field_page_destroy(page);
}

static void test_phase_prefix_has_raw_not_view_geometry(void) {
    fd2_field_page *page=fd2_field_page_create();
    static uint8_t input[64000],output[64000];
    for(size_t i=0;i<sizeof(input);i++)input[i]=(uint8_t)(i*7u+i/320u);
    assert(page && fd2_field_page_phase_prefix_write(page,input,sizeof(input))==0);
    assert(fd2_field_page_phase_prefix_read(page,output,sizeof(output))==0);
    assert(!memcmp(input,output,sizeof(input)));
    uint8_t edge[2];
    assert(fd2_field_page_read(page,160,140,2,1,edge,2,2)==0);
    /* 456*140+160=64000，prefix之后字节不受320stride保存页影响。 */
    assert(edge[0]==0&&edge[1]==0);
    assert(fd2_field_page_read(page,159,140,1,1,edge,1,1)==0);
    assert(edge[0]==input[63999]);
    memset(output,33,sizeof(output));
    assert(fd2_field_page_phase_prefix_read(page,output,sizeof(output)-1)==-1);
    assert(output[0]==33&&output[63999]==33);
    assert(fd2_field_page_phase_prefix_write(page,input,sizeof(input)-1)==-1);
    fd2_field_page_destroy(page);
}

static void print_bytes(const uint8_t *bytes,size_t count) {
    putchar('[');
    for(size_t i=0;i<count;i++)printf("%s%u",i?",":"",(unsigned)bytes[i]);
    putchar(']');
}

static void test_base_lut_skip(int emit) {
    uint8_t initial[24*24],stream[24*7],lut[256],actual[24*24];
    static const uint8_t row[]={0xc0,0x81,0,9,0x41,7,0xd0};
    for(size_t i=0;i<sizeof(initial);i++)initial[i]=(uint8_t)(11+i%24%8);
    for(size_t i=0;i<24;i++)memcpy(stream+i*7,row,7);
    for(size_t i=0;i<256;i++)lut[i]=(uint8_t)(255-i);
    for(unsigned kind=0;kind<2;kind++) {
        fd2_field_page *page=fd2_field_page_create();assert(page);
        assert(fd2_field_page_write(page,72,72,24,24,initial,sizeof(initial),24)==0);
        assert((kind?fd2_field_page_blit_rle(page,72,72,24,24,stream,sizeof(stream),lut):
            fd2_field_page_blit_base_lut_24(page,72,72,stream,sizeof(stream),lut))==0);
        assert(fd2_field_page_read(page,72,72,24,24,actual,sizeof(actual),24)==0);
        for(size_t i=0;i<sizeof(actual);i++) {
            size_t x=i%24;
            uint8_t expected=x==1?255:x==2?246:(x==4 || x==6)?248:
                (x==3 || x==5 || kind)?initial[i]:lut[initial[i]];
            assert(actual[i]==expected);
        }
        if(emit) {
            printf("{\"kind\":%u,\"initial\":",kind);print_bytes(initial,sizeof(initial));
            printf(",\"rle\":");print_bytes(stream,sizeof(stream));
            printf(",\"lut\":");print_bytes(lut,sizeof(lut));
            printf(",\"final\":");print_bytes(actual,sizeof(actual));puts("}");
        }
        if(!kind) {
            assert(fd2_field_page_blit_base_lut_24(page,72,72,stream,sizeof(stream)-1,lut)==-1);
            assert(fd2_field_page_blit_base_lut_24(page,72,72,stream,sizeof(stream),NULL)==-1);
            uint8_t unchanged[sizeof(actual)];
            assert(fd2_field_page_read(page,72,72,24,24,unchanged,sizeof(unchanged),24)==0);
            assert(!memcmp(actual,unchanged,sizeof(actual)));
        }
        fd2_field_page_destroy(page);
    }
}

static void test_choice_fixed_snapshot_writes(void) {
    static uint8_t owner[FD2_FIELD_PAGE_SIZE],actual[FD2_FIELD_PAGE_SIZE],vga[64000];
    for(size_t i=0;i<sizeof(owner);i++)owner[i]=(uint8_t)(i*17u+i/456u);
    for(size_t i=0;i<sizeof(vga);i++)vga[i]=(uint8_t)(i*13u+i/320u+7u);
    fd2_field_page *page=fd2_field_page_create();assert(page);
    for(unsigned bottom=0;bottom<2;bottom++) {
        assert(fd2_field_page_write(page,0,0,456,336,owner,sizeof(owner),456)==0);
        assert((bottom?fd2_field_page_choice_bottom_write(page,vga,sizeof(vga)):
            fd2_field_page_choice_snapshot_write(page,vga,sizeof(vga)))==0);
        assert(fd2_field_page_read(page,0,0,456,336,actual,sizeof(actual),456)==0);
        for(int y=0;y<336;y++)for(int x=0;x<456;x++) {
            int written=bottom?x>=73&&x<383&&y>=180&&y<266:
                x>=68&&x<388&&y>=68&&y<268;
            uint8_t expected=written?vga[(y-68)*320+x-68]:owner[y*456+x];
            assert(actual[y*456+x]==expected);
        }
        assert(fd2_field_page_choice_snapshot_write(page,vga,sizeof(vga)-1)==-1);
        assert(fd2_field_page_choice_bottom_write(page,vga,sizeof(vga)-1)==-1);
        assert(fd2_field_page_choice_bottom_write(page,NULL,sizeof(vga))==-1);
        static uint8_t unchanged[FD2_FIELD_PAGE_SIZE];
        assert(fd2_field_page_read(page,0,0,456,336,unchanged,sizeof(unchanged),456)==0);
        assert(!memcmp(actual,unchanged,sizeof(actual)));
    }
    fd2_field_page_destroy(page);
}

static void test_effect_pixel_copy_and_scaled_view(void) {
    static uint8_t old[FD2_FIELD_PAGE_SIZE],actual[FD2_FIELD_PAGE_SIZE],scaled[64000];
    for(size_t i=0;i<sizeof(old);i++)old[i]=(uint8_t)(i*17u+i/456u);
    for(size_t i=0;i<sizeof(scaled);i++)scaled[i]=(uint8_t)(i*13u+i/320u+7u);
    fd2_field_page *page=fd2_field_page_create(),*copy=fd2_field_page_create();assert(page&&copy);
    assert(fd2_field_page_write(page,0,0,456,336,old,sizeof(old),456)==0);
    assert(fd2_field_page_copy_pixels(copy,page)==0);
    assert(fd2_field_page_read(copy,0,0,456,336,actual,sizeof(actual),456)==0);
    assert(!memcmp(old,actual,sizeof(old)));
    assert(fd2_field_page_scaled_view_write(copy,scaled,sizeof(scaled))==0);
    assert(fd2_field_page_read(copy,0,0,456,336,actual,sizeof(actual),456)==0);
    for(int y=0;y<336;y++)for(int x=0;x<456;x++) {
        uint8_t expected=x>=72&&x<384&&y>=72&&y<264?
            scaled[(y-68)*320+x-68]:old[y*456+x];
        assert(actual[y*456+x]==expected);
    }
    assert(fd2_field_page_scaled_view_write(copy,scaled,sizeof(scaled)-1)==-1);
    assert(fd2_field_page_copy_pixels(NULL,copy)==-1);
    static uint8_t unchanged[FD2_FIELD_PAGE_SIZE];
    assert(fd2_field_page_read(copy,0,0,456,336,unchanged,sizeof(unchanged),456)==0);
    assert(!memcmp(actual,unchanged,sizeof(actual)));
    fd2_field_page_destroy(copy);fd2_field_page_destroy(page);
}

int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--lut-jsonl")) {test_base_lut_skip(1);return 0;}
    test_write_mask_and_history();
    test_malformed_is_atomic();
    test_projection_and_scratch();
    test_owner_clipping();
    test_phase_prefix_has_raw_not_view_geometry();
    test_base_lut_skip(0);
    test_choice_fixed_snapshot_writes();
    test_effect_pixel_copy_and_scaled_view();
    puts("field_page: 8 groups passed");
    return 0;
}
