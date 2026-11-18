/* S5有限cinematic叶：真实原资源、App页前缀/全页和Game/RNG/失败守卫。
 * 无窗口、无宿主时钟，不执行原版程序。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene_cinematic.h"
#include "field_info.h"
#include "portrait.h"

#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return -1;} } while(0)
static uint32_t rng(void *userdata) {(*(unsigned *)userdata)++;return 7;}

static int page_checks(fd2_archive *archive) {
    fd2_vga *vga=calloc(1,sizeof(*vga)),*before=malloc(sizeof(*before));
    fd2_scene_cinematic *scope=NULL;
    CHECK(vga && before && fd2_scene_cinematic_open(&scope,vga,NULL,archive)==0);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    for(size_t i=0;i<64000;i++)vga->framebuffer[i]=(uint8_t)(i*13u+7u);
    memset(vga->palette,7,768);memset(vga->dac,99,768);
    *before=*vga;
    CHECK(fd2_scene_cinematic_page_alloc(scope,0,64000)==-1);
    CHECK(fd2_scene_cinematic_page_alloc(scope,1,64001)==-1);
    CHECK(fd2_scene_cinematic_page_alloc(scope,1,128000)==0);
    CHECK(fd2_scene_cinematic_page_alloc(scope,1,128000)==-1);
    CHECK(fd2_scene_cinematic_copy(scope,0,1,0,160,320,200,320,640)==0);
    const uint8_t *data;size_t size;
    CHECK(fd2_scene_cinematic_pixels(scope,1,&data,&size)==0 && size==128000);
    for(size_t row=0;row<200;row++)CHECK(!memcmp(data+row*640+160,vga->framebuffer+row*320,320));
    uint8_t *saved=malloc(size);CHECK(saved);memcpy(saved,data,size);
    CHECK(fd2_scene_cinematic_copy(scope,0,1,0,128000,320,200,320,640)==-1);
    CHECK(fd2_scene_cinematic_copy(scope,1,1,0,1,320,200,640,640)==-1);
    CHECK(!memcmp(saved,data,size) && !memcmp(before,vga,sizeof(*vga)));free(saved);
    CHECK(fd2_scene_cinematic_work_clear(scope,0xa7)==0);
    CHECK(fd2_scene_cinematic_copy(scope,0,5,0,0,320,200,320,320)==0);
    uint8_t *page=malloc(FD2_FIELD_PAGE_SIZE);CHECK(page);
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,page,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(!memcmp(page,vga->framebuffer,64000));
    for(size_t i=64000;i<FD2_FIELD_PAGE_SIZE;i++)CHECK(page[i]==0xa7);
    CHECK(fd2_scene_cinematic_work_clear(scope,0)==0);
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,page,FD2_FIELD_PAGE_SIZE,456)==0);
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)CHECK(page[i]==0);free(page);
    CHECK(fd2_scene_cinematic_palette_restore(scope)==-1);
    CHECK(fd2_scene_cinematic_palette_validate(scope,57)==0);
    CHECK(fd2_scene_cinematic_palette_validate(scope,34)==-1);
    CHECK(!memcmp(before,vga,sizeof(*vga)));
    CHECK(fd2_scene_cinematic_palette_save(scope)==0);
    CHECK(fd2_scene_cinematic_palette_load(scope,34)==-1);
    CHECK(!memcmp(vga->palette,before->palette,768));
    CHECK(fd2_scene_cinematic_palette_load(scope,57)==0);
    CHECK(!memcmp(vga->dac,before->dac,768));
    CHECK(fd2_scene_cinematic_palette_restore(scope)==0);
    CHECK(!memcmp(before,vga,sizeof(*vga)));
    CHECK(fd2_scene_cinematic_shake_frame(scope,2)==-1);
    CHECK(fd2_scene_cinematic_shake_frame(scope,0)==0);
    CHECK(fd2_scene_cinematic_shake_frame(scope,1)==0);
    fd2_scene_cinematic_close(&scope);CHECK(!scope);
    fd2_field_page_destroy(vga->field_page);free(before);free(vga);return 0;
}

static int sheet_checks(fd2_archive *archive) {
    fd2_vga *vga=calloc(1,sizeof(*vga));fd2_scene_cinematic *scope=NULL;
    CHECK(vga && fd2_scene_cinematic_open(&scope,vga,NULL,archive)==0);
    CHECK(fd2_scene_cinematic_page_alloc(scope,1,64000)==0);
    CHECK(fd2_scene_cinematic_page_clear(scope,1,83)==0);
    CHECK(fd2_scene_cinematic_sheet_open(scope,0,34)==0);
    size_t count,intro;CHECK(fd2_scene_cinematic_sheet_info(scope,0,&count,&intro)==0 && count==101);
    CHECK(fd2_scene_cinematic_sheet_blit(scope,0,100,1,0,0,320)==0);
    const uint8_t *data;size_t size;CHECK(fd2_scene_cinematic_pixels(scope,1,&data,&size)==0);
    uint8_t *saved=malloc(size);CHECK(saved);memcpy(saved,data,size);
    CHECK(fd2_scene_cinematic_sheet_blit(scope,0,101,1,0,0,320)==-1);
    CHECK(fd2_scene_cinematic_sheet_open(scope,0,SIZE_MAX)==-1);
    CHECK(fd2_scene_cinematic_sheet_info(scope,0,&count,&intro)==0 && count==101);
    CHECK(!memcmp(saved,data,size));free(saved);
    CHECK(fd2_scene_cinematic_sheet_close(scope,0)==0);
    CHECK(fd2_scene_cinematic_sheet_open(scope,1,54)==0);
    CHECK(fd2_scene_cinematic_sheet_info(scope,1,&count,&intro)==0 && count>=109);
    CHECK(fd2_scene_cinematic_sheet_open(scope,2,58)==0);
    CHECK(fd2_scene_cinematic_sheet_info(scope,2,&count,&intro)==0 && count>=20);
    /* 仅shared disabled早门的合成binding；不借legacy enabled假称正式门。
     * Audio sentinel不得被sync／资源／stop解引用，无设备／真实IRQ。 */
    fd2_pcm_driver driver={0};fd2_audio_dig_init(&driver.dig);
    driver.audio=(fd2_audio *)(uintptr_t)1;driver.dig.attached=1;driver.dig.enabled=0;
    fd2_pcm_player player={0};CHECK(fd2_pcm_player_init_shared(&player,&driver,0,11025)==0);
    fd2_field_audio audio={0};audio.player=&player;
    fd2_field_audio saved_audio=audio;fd2_pcm_player saved_player=player;fd2_pcm_driver saved_driver=driver;
    CHECK(fd2_scene_cinematic_audio_open(scope,81)==-1);
    CHECK(fd2_scene_cinematic_audio_open(scope,88)==0);
    CHECK(fd2_scene_cinematic_audio_play(scope,&audio,0)==-1);
    CHECK(fd2_scene_cinematic_audio_play(scope,&audio,1)==0);
    CHECK(fd2_scene_cinematic_audio_close(scope,&audio)==0);
    CHECK(!memcmp(&audio,&saved_audio,sizeof(audio)) && !memcmp(&player,&saved_player,sizeof(player)) &&
          !memcmp(&driver,&saved_driver,sizeof(driver)));
    CHECK(fd2_field_audio_play_borrowed_bank(&audio,NULL,1,1)==0);
    CHECK(fd2_field_audio_play_borrowed_bank(&audio,NULL,-1,1)==0);
    CHECK(fd2_field_audio_play_borrowed_bank(&audio,NULL,-2,1)==-1);
    CHECK(!memcmp(&audio,&saved_audio,sizeof(audio)) && !memcmp(&player,&saved_player,sizeof(player)) &&
          !memcmp(&driver,&saved_driver,sizeof(driver)));
    fd2_scene_cinematic_close(&scope);free(vga);return 0;
}

static int credits_checks(fd2_archive *archive) {
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*screen=malloc(sizeof(*screen));
    fd2_scene_cinematic *scope=NULL;unsigned calls=0;
    CHECK(game && before && vga && screen);
    CHECK(fd2_archive_open(&game->fdtxt_archive,"original_game/FDTXT.DAT")==0);
    CHECK(fd2_archive_open(&game->dato_archive,"original_game/DATO.DAT")==0);
    CHECK(fd2_archive_open(&game->figani_archive,"original_game/FIGANI.DAT")==0);
    CHECK(fd2_text_entry_open_entry(&game->ui_text,&game->fdtxt_archive,0)==0);
    CHECK(fd2_font_open_fdother(&game->font,archive,4)==0);
    CHECK(fd2_field_info_assets_open(&game->info_assets,archive)==0);
    game->stage=26;game->roster_count=2;game->units.count=2;
    game->attack_rng=rng;game->attack_rng_userdata=&calls;
    for(size_t i=0;i<2;i++) {
        game->units.items[i].side=2;game->units.items[i].unit_id=(uint8_t)i;
        game->units.items[i].text_id=(uint8_t)i;game->units.items[i].movement_profile=1;
    }
    *before=*game;
    CHECK(fd2_scene_cinematic_open(&scope,vga,game,archive)==0);
    CHECK(fd2_scene_cinematic_page_alloc(scope,1,128000)==0);
    CHECK(fd2_scene_cinematic_page_alloc(scope,2,64000)==0);
    CHECK(fd2_scene_cinematic_page_alloc(scope,3,64000)==0);
    CHECK(fd2_scene_cinematic_page_alloc(scope,4,224000)==0);
    CHECK(fd2_scene_cinematic_credits_roll_text(scope)==0);
    CHECK(fd2_scene_cinematic_credits_assets_open(scope)==0);
    CHECK(fd2_scene_cinematic_image_blit(scope,56,2,0,0,320)==0);
    CHECK(fd2_scene_cinematic_credits_actor(scope,2)==-1);
    CHECK(!memcmp(game,before,sizeof(*game)) && calls==0);
    CHECK(fd2_scene_cinematic_credits_actor(scope,1)==0);
    CHECK(fd2_scene_cinematic_credits_intro(scope,9)==-1);
    for(int phase=8;phase>=0;phase--)CHECK(fd2_scene_cinematic_credits_intro(scope,(unsigned)phase)==0);
    CHECK(fd2_scene_cinematic_credits_intro_finish(scope)==0);
    CHECK(fd2_scene_cinematic_credits_idle_reset(scope)==0);
    CHECK(fd2_scene_cinematic_credits_card_prepare(scope)==0);
    CHECK(fd2_scene_cinematic_credits_blink_reset(scope)==0);*screen=*vga;
    CHECK(fd2_scene_cinematic_credits_card(scope,0)==0 && calls==1);
    CHECK(!memcmp(game,before,sizeof(*game)) && !memcmp(vga,screen,sizeof(*vga)));
    const uint8_t *pixels;size_t bytes;
    CHECK(fd2_scene_cinematic_pixels(scope,1,&pixels,&bytes)==0);
    fd2_image portrait={0};
    CHECK(fd2_portrait_decode_frame(&portrait,&game->dato_archive,0,0)==0);
    for(int y=0;y<portrait.height;y++)for(int x=0;x<portrait.width;x++)
        CHECK(pixels[(size_t)(y+10)*320u+(size_t)(x+8)]==
            portrait.pixels[(size_t)y*(size_t)portrait.width+(size_t)x]);
    fd2_image_free(&portrait);
    uint8_t *saved=malloc(bytes);CHECK(saved);memcpy(saved,pixels,bytes);
    CHECK(fd2_scene_cinematic_credits_card(scope,220)==-1 && calls==1);
    CHECK(!memcmp(saved,pixels,bytes));free(saved);
    for(unsigned frame=1;frame<50;frame++)CHECK(fd2_scene_cinematic_credits_card(scope,frame)==0);
    CHECK(calls==2 && !memcmp(game,before,sizeof(*game)));
    CHECK(fd2_scene_cinematic_credits_skip_set(scope,1)==0);
    CHECK(fd2_scene_cinematic_credits_actor_close(scope)==0);
    int skip=0;CHECK(fd2_scene_cinematic_credits_skip_get(scope,&skip)==0 && skip==1);
    CHECK(fd2_scene_cinematic_credits_skip_set(scope,2)==-1);
    CHECK(fd2_scene_cinematic_credits_actor(scope,0)==0);
    CHECK(fd2_scene_cinematic_credits_idle_reset(scope)==0);
    CHECK(fd2_scene_cinematic_credits_card_prepare(scope)==0);
    CHECK(fd2_scene_cinematic_credits_card(scope,220)==0);
    CHECK(fd2_scene_cinematic_credits_card(scope,439)==0);
    CHECK(fd2_scene_cinematic_credits_card(scope,440)==-1);
    CHECK(game->stage==26 && !memcmp(game,before,sizeof(*game)));
    CHECK(fd2_scene_cinematic_credits_actor_close(scope)==0);
    CHECK(fd2_scene_cinematic_credits_assets_close(scope)==0);
    fd2_scene_cinematic_close(&scope);free(screen);free(vga);free(before);
    fd2_field_info_assets_close(&game->info_assets);fd2_text_entry_close(&game->ui_text);
    fd2_archive_close(&game->figani_archive);fd2_archive_close(&game->dato_archive);
    fd2_archive_close(&game->fdtxt_archive);free(game);return 0;
}

static int montage_checks(fd2_archive *archive) {
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*screen=malloc(sizeof(*screen));
    fd2_scene_cinematic *scope=NULL;unsigned calls=0;
    CHECK(game && before && vga && screen && fd2_archive_open(&game->figani_archive,"original_game/FIGANI.DAT")==0);
    game->stage=26;game->units.count=2;game->units.items[0].unit_id=51;game->units.items[0].side=2;
    game->units.items[1].unit_id=103;game->units.items[0].sprite_cache_class=7;game->units.items[1].sprite_cache_class=9;
    game->attack_rng=rng;game->attack_rng_userdata=&calls;
    memset(vga->framebuffer,0xa5,64000);memset(vga->palette,7,768);memset(vga->dac,19,768);
    CHECK(fd2_scene_cinematic_open(&scope,vga,game,archive)==0);*before=*game;*screen=*vga;
    game->stage=29;CHECK(fd2_scene_cinematic_montage_open(scope,51,103,4)==-1);game->stage=26;
    CHECK(fd2_scene_cinematic_montage_open(scope,51,103,4)==0);
    CHECK(fd2_scene_cinematic_montage_open(scope,51,103,4)==-1);
    CHECK(!memcmp(before,game,sizeof(*game)) && !memcmp(screen,vga,sizeof(*vga)) && calls==0);
    CHECK(fd2_scene_cinematic_montage_half(scope,0)==-1);
    CHECK(fd2_scene_cinematic_montage_intro_finish(scope)==-1);
    CHECK(fd2_scene_cinematic_montage_intro(scope,9)==-1 && !memcmp(screen,vga,sizeof(*vga)));
    for(int i=8;i>=0;i--)CHECK(fd2_scene_cinematic_montage_intro(scope,(unsigned)i)==0);
    CHECK(fd2_scene_cinematic_montage_intro_finish(scope)==0);
    CHECK(fd2_scene_cinematic_montage_intro_finish(scope)==-1);
    CHECK(!memcmp(before,game,sizeof(*game)) && calls==0 && !memcmp(screen->palette,vga->palette,768) && !memcmp(screen->dac,vga->dac,768));
    CHECK(fd2_scene_cinematic_montage_half(scope,2)==-1 && fd2_scene_cinematic_montage_half(scope,0)==0);
    size_t count=99;uint8_t marker=99,ticks=99;int last=99;
    CHECK(fd2_scene_cinematic_montage_part_count(scope,&count)==0 && count==5);
    CHECK(fd2_scene_cinematic_montage_part_info(scope,1,&marker,&ticks,&last)==0 && marker==1 && ticks==2 && last);
    marker=ticks=99;last=99;
    CHECK(fd2_scene_cinematic_montage_part_info(scope,22,&marker,&ticks,&last)==-1 && marker==99 && ticks==99 && last==99);
    int yes=99;
    CHECK(fd2_scene_cinematic_montage_rng(scope,101,&yes)==-1 && calls==0 && yes==99);
    CHECK(fd2_scene_cinematic_montage_rng(scope,3,&yes)==0 && calls==1 && !yes);
    CHECK(fd2_scene_cinematic_montage_cue(scope,1)==0 && calls==1);
    CHECK(fd2_scene_cinematic_montage_cue(scope,22)==-1);
    CHECK(fd2_scene_cinematic_montage_hit(scope)==0 && fd2_scene_cinematic_montage_frame(scope,1)==0);
    CHECK(!memcmp(before,game,sizeof(*game)) && !memcmp(screen->palette,vga->palette,768) && !memcmp(screen->dac,vga->dac,768));
    CHECK(fd2_scene_cinematic_montage_half(scope,1)==0);
    CHECK(fd2_scene_cinematic_montage_part_count(scope,&count)==0 && count==13);
    CHECK(fd2_scene_cinematic_montage_part_info(scope,6,&marker,&ticks,&last)==0 && marker==1 && ticks==2 && last);
    CHECK(fd2_scene_cinematic_montage_close(scope)==0 && fd2_scene_cinematic_montage_close(scope)==-1);
    CHECK(fd2_scene_cinematic_montage_frame(scope,0)==-1 && calls==1 && !memcmp(before,game,sizeof(*game)));
    /* 112真实尾duration0保留；不能补一帧/一tick或混成结构坏。 */
    game->stage=30;game->units.items[0].unit_id=39;game->units.items[1].unit_id=112;
    CHECK(fd2_scene_cinematic_montage_open(scope,39,112,22)==0);
    CHECK(fd2_scene_cinematic_montage_intro(scope,0)==0 && fd2_scene_cinematic_montage_intro_finish(scope)==0);
    CHECK(fd2_scene_cinematic_montage_half(scope,1)==0);
    CHECK(fd2_scene_cinematic_montage_part_info(scope,19,&marker,&ticks,&last)==0 && !marker && !ticks && !last);
    fd2_scene_cinematic_close(&scope);CHECK(!scope);
    fd2_archive_close(&game->figani_archive);free(screen);free(vga);free(before);free(game);return 0;
}

int main(void) {
    fd2_archive archive={0};
    if(fd2_archive_open(&archive,"original_game/FDOTHER.DAT")!=0)return 1;
    int result=page_checks(&archive)!=0 || sheet_checks(&archive)!=0 || credits_checks(&archive)!=0 || montage_checks(&archive)!=0;
    fd2_archive_close(&archive);
    if(!result)puts("scene cinematic finite leaves: passed");
    return result?1:0;
}
