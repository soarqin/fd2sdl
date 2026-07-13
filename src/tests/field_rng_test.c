#include <stdio.h>

#include "field_rng.h"

static int expect(uint32_t actual, uint32_t expected, const char *label) {
    if (actual == expected) return 0;
    fprintf(stderr, "field rng test failed: %s got=0x%04x expected=0x%04x\n",
            label, (unsigned)actual, (unsigned)expected);
    return -1;
}

int main(void) {
    fd2_field_rng rng;
    /* DOSBox debugger 在 field_rng_next 第一次执行前捕获 DS:0x27b8
     * 的 loader 初值 0x7a18。 */
    fd2_field_rng_seed(&rng, 0x7a18);
    if (expect(fd2_field_rng_next(&rng), 0x5160, "step 1") != 0 ||
        expect(fd2_field_rng_next(&rng), 0x0ba7, "step 2") != 0 ||
        expect(fd2_field_rng_next(&rng), 0xdddc, "step 3") != 0 ||
        expect(fd2_field_rng_next(&rng), 0x6f83, "step 4") != 0)
        return 1;
    return 0;
}
