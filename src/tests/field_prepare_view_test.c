/* G3商品／库存与装备收件纯页面：272d0/27738/2825b/28632完整源合同。
 * 用真实FDOTHER/FDTXT/FDICON，固定原物品记录与坐标合成完整FB期望；
 * 没有窗口/输入/等待/RNG，不将这些像素自测说成原版运行对照。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_prepare_view.h"
#include "field_prepare_item.h"

#define CHECK(x) do {if(!(x)){fprintf(stderr,"prepare_view:%d: %s\n",__LINE__,#x);return -1;}} while(0)

static fd2_field_prepare_assets assets;
static fd2_archive other;

static void base_page(fd2_vga *vga) {
    memset(vga,0x5a,sizeof(*vga));
    fd2_map_sprite_blit(vga,&assets.service.roster_page,5,112,-1);
}

static int expected_name(fd2_vga *vga,size_t fragment,int x,int y,uint8_t color) {
    const uint8_t *tokens;size_t count;
    CHECK(fd2_text_entry_get_fragment(&assets.text,fragment,&tokens,&count)==0);
    for(size_t i=0;i<count;i++) {
        uint16_t token=fd2_text_token_at(tokens,i);if(token==0xffffu)return 0;
        CHECK(token<assets.font.glyph_count);
        fd2_font_draw_glyph(vga,&assets.font,token,x+16*(int)i,y,color,76,-1);
    }
    return -1;
}

static void fixed_digits(fd2_vga *vga,const char *number,size_t bank,int x,int y) {
    for(size_t i=0;number[i];i++)
        fd2_map_sprite_blit(vga,&assets.info.digits[bank][number[i]-'0'],x+6*(int)i,y,0);
}

/* 独立按原RLE写点合成4e29c/4e22a/4e1a6，不借view的流转换或cache投影。 */
static void source_sprite(fd2_vga *vga,size_t frame,int x,int y,unsigned style) {
    size_t at=assets.sprites.offsets[frame];
    for(int row=0;row<24;row++)for(int col=0;col<24;) {
        uint8_t command=assets.sprites.data[at++];int n=(command&63)+1;
        if(command>=192) {
            for(int k=0;k<n;k++)if(style==2 && y+row<VGA_H)
                vga->framebuffer[(y+row)*320+x+col+k]=73;
        } else {
            uint8_t value=command<128 ? assets.sprites.data[at++]:0;
            int span=(command&192)==64 ? 2*n:n;
            for(int k=0;k<span;k++) {
                if((command&192)==64 && !(k&1))continue;
                uint8_t color=command>=128 ? assets.sprites.data[at++]:value;
                if(style==1)color=(uint8_t)((color&7)+24);
                if(y+row<VGA_H)vga->framebuffer[(y+row)*320+x+col+k]=color;
            }
        }
        col+=(command&192)==64 ? 2*n:n;
    }
}

static int test_original_product_families_full_pixels(void) {
    const struct {uint8_t id;size_t icon;int effect;const char *value,*buy,*sell;} cases[]={
        {20,0,5,"020","00080","00060"},{128,1,6,"002","00050","00037"},
        {102,2,7,"500","01000","00750"},{206,2,8,"080","01000","00750"},
        {88,2,-1,NULL,"05000","03750"}
    };
    fd2_vga *actual=malloc(sizeof(*actual)),*expected=malloc(sizeof(*expected));CHECK(actual&&expected);
    fd2_field_prepare_assets before=assets;
    for(size_t c=0;c<sizeof(cases)/sizeof(cases[0]);c++) for(int sell=0;sell<2;sell++) {
        base_page(actual);base_page(expected);
        CHECK(fd2_field_prepare_draw_items(actual,&assets,&cases[c].id,1,0,0,sell)==0);
        fd2_map_sprite_blit(expected,&assets.info.detail_icons[cases[c].icon],10,119,-1);
        CHECK(expected_name(expected,(size_t)cases[c].id+181,38,122,201)==0);
        if(cases[c].effect>=0) {
            fd2_map_sprite_blit(expected,&assets.info.detail_icons[cases[c].effect],105,121,-1);
            fixed_digits(expected,cases[c].value,1,128,121);
        } else fd2_map_sprite_blit(expected,&assets.info.digit_overflow[0],105,123,0);
        fd2_map_sprite_blit(expected,&assets.service.price_icon,105,131,-1);
        fixed_digits(expected,sell ? cases[c].sell:cases[c].buy,2,114,131);
        CHECK(!memcmp(actual,expected,sizeof(*actual)) && !memcmp(&assets,&before,sizeof(assets)));
    }
    free(expected);free(actual);return 0;
}

static int test_item_scroll_and_atomic_errors(void) {
    const uint8_t ids[7]={20,21,22,23,24,25,26};
    fd2_vga *vga=malloc(sizeof(*vga)),*before=malloc(sizeof(*before)),*base=malloc(sizeof(*base));CHECK(vga&&before&&base);
    base_page(vga);base_page(base);
    CHECK(fd2_field_prepare_draw_items(vga,&assets,ids,7,2,6,0)==0);
    for(int y=171;y<198;y++)CHECK(!memcmp(vga->framebuffer+y*320+158,base->framebuffer+y*320+158,147));
    *before=*vga;
    CHECK(fd2_field_prepare_draw_items(vga,&assets,ids,7,4,6,0)==-1);
    CHECK(fd2_field_prepare_draw_items(vga,&assets,ids,7,1,2,0)==-1);
    CHECK(fd2_field_prepare_draw_items(vga,&assets,ids,7,2,0,0)==-1);
    uint8_t bad[2]={20,255};CHECK(fd2_field_prepare_draw_items(vga,&assets,bad,2,0,0,0)==-1);
    uint8_t *saved=assets.info.digits[2][1].pixels;assets.info.digits[2][1].pixels=NULL;
    CHECK(fd2_field_prepare_draw_items(vga,&assets,ids,1,0,0,0)==-1);assets.info.digits[2][1].pixels=saved;
    size_t glyphs=assets.font.glyph_count;assets.font.glyph_count=1;
    CHECK(fd2_field_prepare_draw_items(vga,&assets,ids,1,0,0,0)==-1);assets.font.glyph_count=glyphs;
    CHECK(!memcmp(vga,before,sizeof(*vga)));
    CHECK(fd2_field_prepare_draw_items(NULL,&assets,ids,1,0,0,0)==-1);
    CHECK(fd2_field_prepare_draw_items(vga,NULL,ids,1,0,0,0)==-1);
    CHECK(fd2_field_prepare_draw_items(vga,&assets,NULL,1,0,0,0)==-1);
    CHECK(fd2_field_prepare_draw_items(vga,&assets,ids,0,0,0,0)==-1);
    free(base);free(before);free(vga);return 0;
}

static void set_word(fd2_field_unit *unit,size_t at,uint16_t value) {
    uint8_t *raw=(uint8_t *)unit;raw[at]=(uint8_t)value;raw[at+1]=(uint8_t)(value>>8);
}

static void preview_unit(fd2_field_unit *unit) {
    memset(unit,0,sizeof(*unit));uint8_t *raw=(uint8_t *)unit;
    unit->unit_id=0;unit->text_id=0;unit->attack=500;unit->defense=0;unit->accuracy=97;unit->evasion=99;
    for(size_t i=0;i<8;i++){raw[10+2*i]=0x80;raw[11+2*i]=255;}
    raw[10]=raw[12]=0x40;raw[11]=20;raw[13]=128;
    raw[0x22]=raw[0x23]=raw[0x24]=3;
    set_word(unit,0x37,10);set_word(unit,0x39,5);set_word(unit,0x3e,7);
}

static int test_preview_domain_is_distinct_and_readonly(void) {
    fd2_field_unit unit;preview_unit(&unit);fd2_field_unit before=unit;
    int32_t result[4]={-1,-1,-1,-1};const int32_t weapon[4]={38,7,97,7},armor[4]={30,15,97,7};
    CHECK(fd2_field_prepare_item_preview_stats(&unit,21,result)==0 && !memcmp(result,weapon,sizeof(result)));
    CHECK(fd2_field_prepare_item_preview_stats(&unit,129,result)==0 && !memcmp(result,armor,sizeof(result)));
    CHECK(!memcmp(&unit,&before,sizeof(unit))); /* 状态3不会触发1.15/15，旧同组装备不叠。 */
    set_word(&unit,0x37,(uint16_t)-100);
    CHECK(fd2_field_prepare_item_preview_stats(&unit,21,result)==0 && result[0]==-72);
    int32_t saved[4];memcpy(saved,result,sizeof(saved));
    ((uint8_t *)&unit)[14]=0xc0;((uint8_t *)&unit)[15]=255;before=unit;
    CHECK(fd2_field_prepare_item_preview_stats(&unit,21,result)==-1 && !memcmp(result,saved,sizeof(result)));
    CHECK(!memcmp(&unit,&before,sizeof(unit)));
    CHECK(fd2_field_prepare_item_preview_stats(NULL,21,result)==-1);
    CHECK(fd2_field_prepare_item_preview_stats(&unit,255,result)==-1);
    CHECK(fd2_field_prepare_item_preview_stats(&unit,21,NULL)==-1);
    return 0;
}

static int test_preview_row_full_pixels_and_cache(void) {
    fd2_save_manual_state *state=calloc(1,sizeof(*state));CHECK(state);state->unit_count=1;
    fd2_field_unit unit;preview_unit(&unit);memcpy(state->unit_area,&unit,80);
    fd2_save_manual_state before=*state;uint8_t raw=0;
    fd2_vga *vga=malloc(sizeof(*vga)),*expected=malloc(sizeof(*expected));CHECK(vga&&expected);
    base_page(vga);base_page(expected);
    CHECK(fd2_field_prepare_draw_equipment_recipients(vga,&assets,state,&raw,1,0,0,21,3)==0);
    source_sprite(expected,1,14,117,2);
    CHECK(expected_name(expected,1,40,121,201)==0);
    const char *old[4]={"500","000","097","099"},*next[4]={"038","007","097","007"};
    const size_t banks[4]={2,1,0,2};
    const int label_x[4]={122,122,196,196},ys[4]={120,129,120,129};
    const int old_x[4]={137,137,214,214},new_x[4]={165,165,242,242},arrow_x[4]={157,157,234,234};
    for(size_t i=0;i<4;i++) {
        fd2_map_sprite_blit(expected,&assets.service.preview_labels[i],label_x[i],ys[i],0);
        fixed_digits(expected,old[i],banks[i],old_x[i],ys[i]);fixed_digits(expected,next[i],banks[i],new_x[i],ys[i]);
        fd2_map_sprite_blit(expected,&assets.service.preview_arrow,arrow_x[i],ys[i]+1,0);
    }
    CHECK(!memcmp(vga,expected,sizeof(*vga)) && !memcmp(state,&before,sizeof(*state)));
    state->unit_count=3;memcpy(state->unit_area+80,&unit,80);unit.unit_id=4;memcpy(state->unit_area+160,&unit,80);
    uint8_t sprite=0xff;
    CHECK(fd2_field_prepare_cached_sprite_id(state,0,&sprite)==1 && sprite==0);
    CHECK(fd2_field_prepare_cached_sprite_id(state,1,&sprite)==1 && sprite==4);
    sprite=0xff;CHECK(fd2_field_prepare_cached_sprite_id(state,2,&sprite)==0 && sprite==0xff);
    raw=2;base_page(vga);
    CHECK(fd2_field_prepare_draw_equipment_recipients(vga,&assets,state,&raw,1,0,0,21,1)==0);
    base_page(expected);
    for(int y=117;y<141;y++)CHECK(!memcmp(vga->framebuffer+y*320+14,expected->framebuffer+y*320+14,24));
    *expected=*vga;uint8_t bad=3;
    CHECK(fd2_field_prepare_draw_equipment_recipients(vga,&assets,state,&bad,1,0,0,21,1)==-1);
    CHECK(fd2_field_prepare_draw_equipment_recipients(vga,&assets,state,&raw,1,0,0,21,4)==-1);
    CHECK(fd2_field_prepare_draw_equipment_recipients(vga,&assets,state,&raw,1,0,0,255,1)==-1);
    CHECK(!memcmp(vga,expected,sizeof(*vga)));
    CHECK(fd2_field_prepare_cached_sprite_id(NULL,0,&sprite)==-1);
    CHECK(fd2_field_prepare_cached_sprite_id(state,3,&sprite)==-1);
    CHECK(fd2_field_prepare_cached_sprite_id(state,0,NULL)==-1);
    free(expected);free(vga);free(state);return 0;
}

static void roster_fixture(fd2_save_manual_state *state,size_t count) {
    memset(state,0xa5,sizeof(*state));state->unit_count=(uint8_t)count;
    for(size_t i=0;i<count;i++) {
        fd2_field_unit unit={0};unit.unit_id=(uint8_t)i;unit.text_id=(uint8_t)i;
        unit.movement_profile=1;unit.level=(uint8_t)(i+1);unit.side=2;
        unit.flags=1;unit.hp=unit.hp_max=100;unit.mp=unit.mp_max=20;
        unit.attack=20;unit.defense=15;unit.accuracy=80;unit.evasion=50;
        unit.sprite_cache_class=0xff;
        memcpy(state->unit_area+i*80u,&unit,80);
    }
}

static int test_members_and_revive_full_pages(void) {
    fd2_save_manual_state *state=malloc(sizeof(*state)),*before=malloc(sizeof(*before));
    fd2_vga *actual=malloc(sizeof(*actual)),*expected=malloc(sizeof(*expected));CHECK(state&&before&&actual&&expected);
    roster_fixture(state,7);*before=*state;
    for(unsigned phase=0;phase<4;phase++) {
        base_page(actual);base_page(expected);
        CHECK(fd2_field_prepare_draw_members(actual,&assets,state,2,6,phase)==0);
        for(size_t i=0;i<5;i++) {
            int x=132*(int)(i%2),y=26*(int)(i/2);
            source_sprite(expected,(i+2)*12u+(phase==3 ? 1:phase),14+x,117+y,2);
            CHECK(expected_name(expected,i+3,40+x,121+y,i==4 ? 201:205)==0);
        }
        CHECK(!memcmp(actual,expected,sizeof(*actual)) && !memcmp(state,before,sizeof(*state)));
    }
    *expected=*actual;
    CHECK(fd2_field_prepare_draw_members(actual,&assets,state,1,2,0)==-1);
    CHECK(fd2_field_prepare_draw_members(actual,&assets,state,4,6,0)==-1);
    CHECK(fd2_field_prepare_draw_members(actual,&assets,state,2,0,0)==-1);
    CHECK(!memcmp(actual,expected,sizeof(*actual)));
    const uint8_t raws[4]={1,5,3,4};const char *prices[3]={"00600","00400","00500"};
    base_page(actual);base_page(expected);
    CHECK(fd2_field_prepare_draw_revive_candidates(actual,&assets,state,raws,4,1,2,3)==0);
    for(size_t i=0;i<3;i++) {
        int y=26*(int)i;source_sprite(expected,(size_t)raws[i+1]*12u+1u,14,117+y,2);
        CHECK(expected_name(expected,(size_t)raws[i+1]+1u,40,121+y,i==1 ? 201:205)==0);
        CHECK(expected_name(expected,140,130,121+y,i==1 ? 201:205)==0);
        CHECK(expected_name(expected,151,175,121+y,i==1 ? 201:205)==0);
        fd2_map_sprite_blit(expected,&assets.service.price_icon,220,125+y,-1);
        fixed_digits(expected,prices[i],2,228,125+y);
    }
    CHECK(!memcmp(actual,expected,sizeof(*actual)) && !memcmp(state,before,sizeof(*state)));
    CHECK(fd2_field_prepare_draw_revive_candidates(actual,&assets,state,raws,4,2,2,0)==-1);
    uint8_t saved=state->unit_area[5*80u+32u];state->unit_area[5*80u+32u]=0;
    CHECK(fd2_field_prepare_draw_revive_candidates(actual,&assets,state,raws,4,1,2,0)==-1);
    state->unit_area[5*80u+32u]=saved;
    CHECK(!memcmp(actual,expected,sizeof(*actual)));
    free(expected);free(actual);free(before);free(state);return 0;
}

static uint32_t source32(const uint8_t *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static int deployment_images(fd2_image *label,fd2_image *cursor) {
    const uint8_t *data;size_t size;
    CHECK(fd2_archive_get(&other,5,&data,&size)==0 && size>6u+138u*4u);
    size_t start=source32(data+6u+137u*4u);CHECK(start<size);
    CHECK(fd2_image_decode_buf(label,data+start,size-start)==0 && label->width==86 && label->height==86);
    CHECK(fd2_archive_get(&other,1,&data,&size)==0 && size>14u);
    start=source32(data+6);size_t end=source32(data+10);CHECK(start<end && end<=size);
    cursor->width=cursor->height=24;cursor->pixels=calloc(24u*24u,1);CHECK(cursor->pixels);
    CHECK(fd2_image_decode_rle_pixels(cursor->pixels,24,24,data+start,end-start)==0);return 0;
}

static int test_deployment_full32_and_atomic(void) {
    fd2_image label={0},cursor_image={0};CHECK(deployment_images(&label,&cursor_image)==0);
    fd2_save_manual_state *state=malloc(sizeof(*state)),*before=malloc(sizeof(*before));
    fd2_vga *actual=malloc(sizeof(*actual)),*expected=malloc(sizeof(*expected));CHECK(state&&before&&actual&&expected);
    roster_fixture(state,32);*before=*state;uint8_t selected[31]={0};selected[0]=selected[30]=1;
    for(unsigned phase=0;phase<4;phase++) {
        memset(actual,0x5a,sizeof(*actual));*expected=*actual;
        CHECK(fd2_field_prepare_draw_deployment(actual,&assets,state,selected,19,30,phase,&label,&cursor_image)==0);
        fd2_map_sprite_blit(expected,&assets.info.detail_top,92,7,-1);
        fd2_map_sprite_blit(expected,&assets.info.detail_bottom,5,94,-1);
        fd2_map_sprite_blit(expected,&label,5,7,0);fixed_digits(expected,"19",0,61,35);fixed_digits(expected,"17",1,61,73);
        fd2_field_unit focus;memcpy(&focus,state->unit_area+31u*80u,80);
        fd2_field_detail_stats_draw(expected,&assets.info,&assets.font,&assets.text,&focus);
        fd2_map_sprite_blit(expected,&cursor_image,23,194,0);
        for(size_t i=0;i<31;i++)source_sprite(expected,(i+1u)*12u+(phase==3u ? 1u:phase),
            23+28*(int)(i%10u),100+30*(int)(i/10u)+(selected[i] ? 3:0),selected[i] ? 0u:1u);
        CHECK(!memcmp(actual,expected,sizeof(*actual)) && !memcmp(state,before,sizeof(*state)));
    }
    selected[0]=2;
    CHECK(fd2_field_prepare_draw_deployment(actual,&assets,state,selected,19,30,0,&label,&cursor_image)==-1);
    selected[0]=1;uint8_t *saved=assets.info.bars[7].pixels;assets.info.bars[7].pixels=NULL;
    CHECK(fd2_field_prepare_draw_deployment(actual,&assets,state,selected,19,30,0,&label,&cursor_image)==-1);
    assets.info.bars[7].pixels=saved;
    CHECK(fd2_field_prepare_draw_deployment(actual,&assets,state,selected,16,30,0,&label,&cursor_image)==-1);
    CHECK(fd2_field_prepare_draw_deployment(actual,&assets,state,selected,19,31,0,&label,&cursor_image)==-1);
    CHECK(!memcmp(actual,expected,sizeof(*actual)));
    /* 重复ID使raw31超初始化cache目录；保持姓名/统计/计数，单省未知sprite。 */
    for(size_t i=0;i<32;i++)state->unit_area[i*80u+7u]=0;
    CHECK(fd2_field_prepare_draw_deployment(actual,&assets,state,selected,19,30,0,&label,&cursor_image)==0);
    fd2_image_free(&cursor_image);fd2_image_free(&label);
    free(expected);free(actual);free(before);free(state);return 0;
}

static int test_promotion_rows_and_explicit_cache(void) {
    fd2_save_manual_state *state=malloc(sizeof(*state)),*before=malloc(sizeof(*before));
    fd2_vga *actual=malloc(sizeof(*actual)),*expected=malloc(sizeof(*expected));CHECK(state&&before&&actual&&expected);
    roster_fixture(state,10);
    for(size_t i=0;i<10u;i++) {
        state->unit_area[i*80u+33u]=20;
        for(size_t j=0;j<8u;j++)state->unit_area[i*80u+10u+2u*j]=0x80;
    }
    state->unit_area[80u+7u]=0;state->unit_area[80u+10u]=0;state->unit_area[80u+11u]=89;
    state->unit_area[9u*80u+10u]=0;state->unit_area[9u*80u+11u]=90;
    const uint8_t raws[3]={0,1,9};const size_t profiles[3]={159,167,171};
    *before=*state;
    for(unsigned phase=0;phase<4u;phase++) {
        base_page(actual);base_page(expected);
        CHECK(fd2_field_prepare_draw_promotion_candidates(actual,&assets,state,raws,3,0,1,phase)==0);
        for(size_t i=0;i<3u;i++) {
            int y=26*(int)i;
            if(i<2u)source_sprite(expected,(i ? 2u*12u:0u)+(phase==3u ? 1u:phase),14,117+y,2);
            CHECK(expected_name(expected,(size_t)raws[i]+1u,40,121+y,i==1u ? 201:205)==0);
            CHECK(expected_name(expected,151,130,121+y,i==1u ? 201:205)==0);
            CHECK(expected_name(expected,593,175,121+y,i==1u ? 201:205)==0);
            CHECK(expected_name(expected,profiles[i],239,121+y,i==1u ? 201:205)==0);
        }
        CHECK(!memcmp(actual,expected,sizeof(*actual)) && !memcmp(state,before,sizeof(*state)));
    }
    state->unit_area[9u*80u+7u]=7;
    CHECK(fd2_field_prepare_draw_promotion_candidates(actual,&assets,state,raws,3,0,1,0)==-1);
    CHECK(!memcmp(actual,expected,sizeof(*actual)));
    /* 原目录在稳定重排/身份写后保持，只有显式bind才重建。 */
    roster_fixture(state,2);CHECK(fd2_field_prepare_assets_bind_roster(&assets,state)==0);
    uint8_t temp[80];memcpy(temp,state->unit_area,80);memcpy(state->unit_area,state->unit_area+80,80);memcpy(state->unit_area+80,temp,80);
    base_page(actual);base_page(expected);
    CHECK(fd2_field_prepare_draw_members(actual,&assets,state,0,0,0)==0);
    source_sprite(expected,0,14,117,2);source_sprite(expected,12,146,117,2);
    CHECK(expected_name(expected,2,40,121,201)==0 && expected_name(expected,1,172,121,205)==0);
    CHECK(!memcmp(actual,expected,sizeof(*actual)) && assets.sprite_cache_ids[0]==0);
    CHECK(fd2_field_prepare_assets_bind_roster(&assets,state)==0 && assets.sprite_cache_ids[0]==1);
    base_page(actual);base_page(expected);
    CHECK(fd2_field_prepare_draw_members(actual,&assets,state,0,0,0)==0);
    source_sprite(expected,12,14,117,2);source_sprite(expected,0,146,117,2);
    CHECK(expected_name(expected,2,40,121,201)==0 && expected_name(expected,1,172,121,205)==0);
    CHECK(!memcmp(actual,expected,sizeof(*actual)));
    assets.sprite_cache_ready=0;assets.sprite_cache_count=0;
    free(expected);free(actual);free(before);free(state);return 0;
}

/* 原4e29c/current VGA的独立字节oracle：source_sprite逐原token写，
 * 不从被测view生成expected；覆盖31号角色(ID30)的stride2历史孔。 */
static int test_current_member_and_candidate_history(void) {
    static const uint8_t ids[5]={0,9,4,30,51},names[5]={0,9,4,30,1};
    static const unsigned phases[5]={0,1,2,1,0};
    fd2_save_manual_state *state=malloc(sizeof(*state)),*before=malloc(sizeof(*before));
    fd2_vga *actual=malloc(sizeof(*actual)),*expected=malloc(sizeof(*expected));CHECK(state&&before&&actual&&expected);
    roster_fixture(state,5);
    for(size_t i=0;i<5u;i++){state->unit_area[80u*i+7u]=ids[i];state->unit_area[80u*i+8u]=names[i];}
    CHECK(fd2_field_prepare_assets_bind_roster(&assets,state)==0);*before=*state;
    base_page(actual);*expected=*actual;
    for(size_t step=0;step<5u;step++) {
        unsigned phase=phases[step];
        for(size_t raw=0;raw<5u;raw++) {
            int x=132*(int)(raw%2u),y=26*(int)(raw/2u);
            source_sprite(expected,(size_t)ids[raw]*12u+phase,14+x,117+y,2);
            CHECK(expected_name(expected,(size_t)names[raw]+1u,40+x,121+y,raw==0u ? 201:205)==0);
        }
        CHECK(fd2_field_prepare_draw_members(actual,&assets,state,0,0,phase)==0 &&
            !memcmp(actual,expected,sizeof(*actual)) && !memcmp(state,before,sizeof(*state)));
        if(step==2u)CHECK(actual->framebuffer[161u*320u+146u]==254u);
        if(step==4u)CHECK(actual->framebuffer[161u*320u+167u]==254u && actual->framebuffer[162u*320u+146u]==254u);
    }
    /* mode2只动三sprite：姓名/钱/预览区域保持任意合法current-page历史。 */
    const uint8_t raws[4]={3,0,4,1};memset(actual,0x5a,sizeof(*actual));*expected=*actual;
    for(size_t step=0;step<5u;step++) {
        unsigned phase=phases[step];
        for(size_t i=0;i<3u;i++)source_sprite(expected,(size_t)ids[raws[i]]*12u+phase,14,117+26*(int)i,2);
        CHECK(fd2_field_prepare_draw_candidate_sprites(actual,&assets,state,raws,4,0,phase)==0 &&
            !memcmp(actual,expected,sizeof(*actual)) && !memcmp(state,before,sizeof(*state)));
    }
    uint8_t bad[1]={5};*expected=*actual;
    CHECK(fd2_field_prepare_draw_candidate_sprites(actual,&assets,state,bad,1,0,0)==-1 &&
        !memcmp(actual,expected,sizeof(*actual)));
    CHECK(fd2_field_prepare_draw_candidate_sprites(actual,&assets,state,raws,4,0,4)==-1 &&
        !memcmp(actual,expected,sizeof(*actual)));
    free(expected);free(actual);free(before);free(state);return 0;
}

int main(void) {
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_prepare_assets_open(&assets,&other)==0);
    for(int shop=1;shop<=5;shop++) {
        if(shop==2)continue;
        CHECK(fd2_field_prepare_assets_open_service(&assets,&other,shop)==0);
        CHECK(test_original_product_families_full_pixels()==0);
        CHECK(test_item_scroll_and_atomic_errors()==0);
        if(shop!=4)CHECK(test_preview_row_full_pixels_and_cache()==0);
        CHECK(test_members_and_revive_full_pages()==0);
    }
    CHECK(test_preview_domain_is_distinct_and_readonly()==0);
    CHECK(test_deployment_full32_and_atomic()==0);
    CHECK(test_promotion_rows_and_explicit_cache()==0);
    CHECK(test_current_member_and_candidate_history()==0);
    fd2_field_prepare_assets_close(&assets);fd2_archive_close(&other);
    puts("field_prepare_view: source layout/preview/cache/atomic headless PASS; no native UI claim");return 0;
}
