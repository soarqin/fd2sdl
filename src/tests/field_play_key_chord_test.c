/* driver组合键纯协议；不包含Win32、不启动窗口或改变键态。 */
#include "field_play_key_chord.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr,"key chord check failed at line %d\n",__LINE__); return 1; } } while (0)

int main(void) {
    const char *prefixes[] = {"SHIFT", "CTRL", "ALT"};
    const unsigned modifiers[] = {0x10,0x11,0x12};
    const unsigned bases[] = {84,94,104};
    for (unsigned modifier=0;modifier<3u;modifier++)
        for (unsigned function=1;function<=10u;function++) {
            char name[16];fd2_driver_key_chord chord={0,0,0};
            CHECK(snprintf(name,sizeof(name),"%s_F%u",prefixes[modifier],function)>0);
            CHECK(fd2_driver_key_chord_parse(name,&chord)==0);
            CHECK(chord.modifier_vk==modifiers[modifier]);
            CHECK(chord.function_vk==0x70u+function-1u);
            CHECK(chord.bios_scan_expected==bases[modifier]+function-1u);
        }
    const char *invalid[] = {"","F1","HOST_W","SHIFT_","SHIFT_F","SHIFT_F0","SHIFT_F00",
        "SHIFT_F01","SHIFT_F11","SHIFT_F100","SHIFT_F1x","shift_F1","CTRL_F-1",
        "ALT_F+1","ALT_F 1","ALT_F10_","CTRL_SHIFT_F1","SHIFT_F2\n"};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        fd2_driver_key_chord chord={7,8,9};
        CHECK(fd2_driver_key_chord_parse(invalid[i],&chord)==-1);
        CHECK(chord.modifier_vk==7 && chord.function_vk==8 && chord.bios_scan_expected==9);
    }
    fd2_driver_key_chord chord={7,8,9};
    CHECK(fd2_driver_key_chord_parse(NULL,&chord)==-1);
    CHECK(fd2_driver_key_chord_parse("ALT_F1",NULL)==-1);
    CHECK(chord.modifier_vk==7 && chord.function_vk==8 && chord.bios_scan_expected==9);
    puts("field_play_key_chord: 30合法组合及错误/不写输出通过；非BIOS/GUI实测");
    return 0;
}
