/* B5只读表回归：sub_4E809 @0x4e809（原登记中性名）的DEC stage取行，
 * music_track_play @0x25977/field_turn_cycle_run @0x1a30b的两张音乐表。
 * 原表来自strict LE与独立Domain读集，不等于城镇/音乐运行验收。 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "field.h"

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; \
} } while (0)

int main(int argc, char **argv) {
    uint8_t prepare[26 * 31], music[60];
    FILE *input = fopen(argc > 1 ? argv[1] : "original_game/FD2.EXE", "rb");
    CHECK(input != NULL);
    /* file offsets只作此已核EXE的独立原字节对照，生产query不读EXE。 */
    CHECK(fseek(input, 0x7b3a1L, SEEK_SET) == 0);
    CHECK(fread(prepare, 1, sizeof(prepare), input) == sizeof(prepare));
    CHECK(fseek(input, 0x76e77L, SEEK_SET) == 0);
    CHECK(fread(music, 1, sizeof(music), input) == sizeof(music));
    CHECK(fclose(input) == 0);
    for (size_t stage = 1; stage <= 26; stage++) {
        const uint8_t *row = fd2_field_stage_prepare_record_get(stage);
        CHECK(row && memcmp(row, prepare + (stage - 1u) * 31u, 31) == 0);
        CHECK(row == fd2_field_stage_prepare_record_get(stage));
    }
    CHECK(fd2_field_stage_prepare_record_get(1)[2] == 0x54);
    CHECK(fd2_field_stage_prepare_record_get(2)[0] == 2);
    CHECK(fd2_field_stage_prepare_record_get(26)[2] == 0x63);
    CHECK(fd2_field_stage_prepare_record_get(0) == NULL);
    CHECK(fd2_field_stage_prepare_record_get(27) == NULL);
    CHECK(fd2_field_stage_prepare_record_get(29) == NULL);
    CHECK(fd2_field_stage_prepare_record_get(SIZE_MAX) == NULL);
    for (size_t stage = 0; stage < 30; stage++) {
        const fd2_field_stage_music *row = fd2_field_stage_music_get(stage);
        CHECK(row && row->primary == music[stage] && row->alternate == music[30u + stage]);
        CHECK(row == fd2_field_stage_music_get(stage));
    }
    CHECK(fd2_field_stage_music_get(0)->primary == 19 && fd2_field_stage_music_get(0)->alternate == 12);
    CHECK(fd2_field_stage_music_get(1)->primary == 19 && fd2_field_stage_music_get(1)->alternate == 12);
    CHECK(fd2_field_stage_music_get(29)->primary == 8 && fd2_field_stage_music_get(29)->alternate == 8);
    CHECK(fd2_field_stage_music_get(30) == NULL && fd2_field_stage_music_get(SIZE_MAX) == NULL);
    puts("field_stage_tables: all806 preparation bytes/all60 music bytes + boundaries passed (no GUI)");
    return 0;
}
