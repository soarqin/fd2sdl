/* 原 main @0x25bf4 的指令0x25d83..0x25db1 启动流回归；oracle 直接抄录 F1
 * build/f1/startup-original-cpu.json（原代码、五组 BIOS 向量，无 stub）。
 * shared_rng_call_states 是每次调用前状态，末项另取 shared_rng_after。
 * 原版真实 CRT/RAM 初态及最终值继承 startup-native02，不代签全 App。 */
#include <stdio.h>

#include "field_rng.h"

static int expect(uint32_t actual, uint32_t expected, const char *label) {
    if (actual == expected) return 0;
    fprintf(stderr, "field rng test failed: %s got=0x%04x expected=0x%04x\n",
            label, (unsigned)actual, (unsigned)expected);
    return -1;
}

static int startup_sequence_test(void) {
    static const uint16_t states[] = {
        0,32932,34240,44704,62881,11692,60933,61643,1788,47236,
        17602,42678,46673,13098,6646,20565,847,39708,22913,19631,
        58910,45459,3386,60020,54339,8891,38525,13449,9454,43029,
        49481,35562,55280,16419,33214,36496,62752,10660,52677,61130,
        63219,14396,17030,38102,10065,47917,23050,20727,2143,50076,
        40322,27825,58927,45595,4474,3189,58444,41731,39097,18025,
        46062,8210,33077,35400,53984,6051,15805,28302,62743,10588,
        52101,56522,26355,47167,17050,38262,11345,58157,39435,20729,
        2159,50204,41346,36017,58920,45539,4026,65140,29764,8896,
        38565,13769,12014,63509,16716,35590,55504,18211,47550,20114,
        62774,10836,54085,6859,22269,14479,17694,43414,52561,60202,
        55795,20539,639,38044,9601,44205,58889,45291,2042,49268,
        33858,41648,38433,12713,3566,61460,324,35524,54976,13987,
        13758,11926,62805,11084,56069,22731,18175,47262,17810,44342,
        59985,54059,6651,20605,1167,42268,43393,52393,58858,45043,
        58,33396,37952,8865,38317,11785,61677,2060,49412,35010,
        50864,46626,12722,3638,62036,4932,6853,22221,14095,14622,
        18838,52566,60242,56115,23099,21119,5279,9629,44429,60681,
        59627,51195,49274,33906,42032,41505,37289,3561,61420,4,
        32964,34496,46752,13730,11702,61013,62283,6908,22661
    };
    fd2_field_rng rng;
    fd2_field_rng_seed(&rng, 0);
    if (expect((uint32_t)(sizeof(states) / sizeof(states[0]) - 1), 198, "oracle count")) return 1;
    for (size_t i = 0; i + 1 < sizeof(states) / sizeof(states[0]); i++) {
        char label[48];
        (void)snprintf(label, sizeof(label), "original startup call %u", (unsigned)i);
        if (expect(rng.state, states[i], label) ||
            expect(fd2_field_rng_next(&rng), states[i + 1], label)) return 1;
    }
    return 0;
}

static int startup_configuration_test(void) {
    static const struct { const char *text; uint16_t seed; } valid[] = {
        {"0", 0}, {"1", 1}, {"65535", 65535}, {"0xffff", 65535},
        {"0177777", 65535}, {"0x7a18", 0x7a18}
    };
    static const char *invalid[] = {
        NULL, "", "invalid", "65536", "0x10000", "-1", "-0", "+1",
        " 1", "1 ", "0x", "08", "7a18", "0xffffx",
        "99999999999999999999999999999999999999"
    };
    fd2_field_rng rng;
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        rng.state = 0x1234;
        if (expect(fd2_field_rng_bootstrap(&rng, valid[i].text), 0, "debug skips boot") ||
            expect(rng.state, valid[i].seed, valid[i].text)) return 1;
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        rng.state = 0x1234;
        if (expect(fd2_field_rng_bootstrap(&rng, invalid[i]), 198, "default boot count") ||
            expect(rng.state, 0x5885, "default boot state")) return 1;
    }
    if (expect(fd2_field_rng_bootstrap(NULL, NULL), 0, "null owner") ||
        expect(fd2_field_rng_next(NULL), 0, "null next")) return 1;
    return 0;
}

int main(void) {
    fd2_field_rng rng;
    /* 旧 stage0 捕获值仍是显式非零递推夹具，不是 loader／通用初态。 */
    fd2_field_rng_seed(&rng, 0x7a18);
    if (expect(fd2_field_rng_next(&rng), 0x5160, "step 1") != 0 ||
        expect(fd2_field_rng_next(&rng), 0x0ba7, "step 2") != 0 ||
        expect(fd2_field_rng_next(&rng), 0xdddc, "step 3") != 0 ||
        expect(fd2_field_rng_next(&rng), 0x6f83, "step 4") != 0)
        return 1;
    return startup_sequence_test() || startup_configuration_test();
}
