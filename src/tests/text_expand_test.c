/* 15f84有限展开的独立token期望；不绘制、不拥有Game/RNG。 */
#include <stdio.h>
#include <string.h>
#include "text.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "text_expand:%d %s\n", __LINE__, #x); return 1; } } while (0)
static void word(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
int main(void) {
    /* 0动态姓名/旧名、换行、金额、续行；1/2普通姓名；3递归非法；4未知控制；5缺终止。 */
    static const uint16_t fragments[][9] = {
        {20, 0xfffc, 0xfffe, 0xfffb, 0xfffa, 0xfffd, 0xffff},
        {30,31,0xffff}, {40,0xffff}, {0xfffc,0xffff}, {0xffef,0xffff}, {2,3}
    };
    static const size_t lengths[] = {7,3,2,2,2,2};
    uint8_t raw[64] = {0}, saved[64]; size_t offset = 12;
    for (size_t i = 0; i < 6; i++) {
        word(raw + i * 2, (uint16_t)offset);
        for (size_t k = 0; k < lengths[i]; k++, offset += 2) word(raw + offset, fragments[i][k]);
    }
    memcpy(saved, raw, sizeof(raw));
    fd2_text_entry entry = {0}; CHECK(fd2_text_entry_open_mem(&entry, raw, offset) == 0);
    fd2_text_parameters p = {UINT32_MAX,1,2};
    struct { uint32_t a; fd2_text_expanded out; uint32_t b; } guarded = {0};
    guarded.a = 0x12345678u; guarded.b = 0x87654321u;
    CHECK(fd2_text_expand(&entry,100,&p,0,&guarded.out) == 0);
    static const uint16_t expected[] = {20,30,31,0xfffe,40,FD2_TEXT_SAFE_MINUS,1,0xfffd};
    CHECK(guarded.out.count == sizeof(expected)/sizeof(expected[0]) &&
          !memcmp(guarded.out.tokens,expected,sizeof(expected)));
    p.value = 0x80000000u;
    CHECK(fd2_text_expand(&entry,100,&p,0,&guarded.out) == 0);
    static const uint16_t min_digits[] = {FD2_TEXT_SAFE_MINUS,2,1,4,7,4,8,3,6,4,8};
    CHECK(!memcmp(guarded.out.tokens+5,min_digits,sizeof(min_digits)));
    p.new_fragment=3; CHECK(fd2_text_expand(&entry,100,&p,0,&guarded.out) == -1);
    p.new_fragment=SIZE_MAX; CHECK(fd2_text_expand(&entry,100,&p,0,&guarded.out) == -1);
    CHECK(fd2_text_expand(&entry,100,&p,4,&guarded.out) == -1);
    CHECK(fd2_text_expand(&entry,100,&p,5,&guarded.out) == -1);
    CHECK(fd2_text_expand(&entry,30,&p,1,&guarded.out) == -1);
    CHECK(guarded.a == 0x12345678u && guarded.b == 0x87654321u && !memcmp(raw,saved,sizeof(raw)));
    fd2_text_entry_close(&entry);
    /* 容量溢出严格拒绝并保外侧guard，不能吞掉多余提示。 */
    uint8_t long_text[2 + (FD2_TEXT_EXPANDED_CAPACITY+2)*2];
    word(long_text,2);
    for (size_t i=0;i<FD2_TEXT_EXPANDED_CAPACITY+1;i++) word(long_text+2+i*2,1);
    word(long_text+2+(FD2_TEXT_EXPANDED_CAPACITY+1)*2,0xffff);
    CHECK(fd2_text_entry_open_mem(&entry,long_text,sizeof(long_text)) == 0);
    CHECK(fd2_text_expand(&entry,100,&p,0,&guarded.out) == -1 &&
          guarded.a == 0x12345678u && guarded.b == 0x87654321u);
    fd2_text_entry_close(&entry);
    puts("text_expand: nested names/signed values/waits/invalid recursion and capacity passed");
    return 0;
}
