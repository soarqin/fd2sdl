/* 准备商品／库存及装备收件预览的纯页面。
 * 来源：sub_272D0 @0x272d0、sub_27738 @0x27738、sub_27F4A @0x27f4a、
 * sub_2825B @0x2825b、sub_28632 @0x28632、sub_2810B @0x2810b、
 * sub_2A0C2 @0x2a0c2、sub_2B4FB @0x2b4fb；名称不代表ABI猜测。
 * 只改当前页的原widget写集；初底由27d33/27738等开页caller显式建立。
 * 不重贴roster底板，4e29c stride2孔须保留上一相；输入/滑页仍归prepare owner。
 * G3规范命名（完整Domain函数/xref/原资源依据，非ABI推断）：
 * field_shop_items_draw @0x272d0（原登记 sub_272D0）。
 * field_prepare_members_draw @0x2810b（原登记 sub_2810B）。
 * field_shop_equipment_recipients_draw @0x2825b（原登记 sub_2825B）。
 * field_revive_candidates_draw @0x2a0c2（原登记 sub_2A0C2）。
 * field_promotion_candidates_draw @0x2a694（原登记 sub_2A694）。
 * field_deployment_selection_draw @0x2b4fb（原登记 sub_2B4FB）。
 */
#include "field_prepare_view.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "field_item.h"
#include "field_prepare_item.h"
#include "field_prepare.h"
#include "static_data/preparation.inc"

typedef struct { const uint8_t *tokens;size_t count; } view_name;

static int image_size(const fd2_image *image,int width,int height) {
    return image && image->pixels && image->width==width && image->height==height;
}

static int name_get(const fd2_field_prepare_assets *a,size_t fragment,view_name *name) {
    size_t length;const uint8_t *tokens;
    if (!a->font.glyphs || !a->font.glyph_count || a->font.glyph_count>a->font.size/32u ||
        fd2_text_entry_get_fragment(&a->text,fragment,&tokens,&length)!=0) return -1;
    for (size_t i=0;i<length;i++) {
        uint16_t token=fd2_text_token_at(tokens,i);
        if (token==0xffffu) {name->tokens=tokens;name->count=i;return 0;}
        if (token>=a->font.glyph_count || i>(size_t)INT_MAX/16u) return -1;
    }
    return -1;
}

static void name_draw(fd2_vga *vga,const fd2_field_prepare_assets *a,
        const view_name *name,int x,int y,int selected) {
    for (size_t i=0;i<name->count;i++)
        fd2_font_draw_glyph(vga,&a->font,fd2_text_token_at(name->tokens,i),x+(int)i*16,y,
            selected ? 201u:205u,76,-1);
}

static int base_valid(const fd2_vga *vga,const fd2_field_prepare_assets *a) {
    return vga && a && a->ready && a->info.ready;
}

int fd2_field_prepare_cached_sprite_id(const fd2_save_manual_state *state,
        size_t raw_index,uint8_t *sprite_id) {
    if (!state || !sprite_id || !state->unit_count || state->unit_count>FD2_SAVE_MAX_UNITS ||
        raw_index>=state->unit_count) return -1;
    uint8_t ids[FD2_SAVE_MAX_UNITS];size_t count=0;
    for (size_t raw=0;raw<state->unit_count;raw++) {
        uint8_t id=state->unit_area[raw*80u+7u];size_t i=0;
        while (i<count && ids[i]!=id) i++;
        if (i==count) ids[count++]=id;
    }
    /* 11019分配0x32a00B，目录预留1920B；这里只初始化12*count个offset。
     * 超count不是已证malloc外读，而是原未初始化目录；仅省该sprite绘制。 */
    if (raw_index>=count) return 0;
    *sprite_id=ids[raw_index];return 1;
}

static int cached_sprite(const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,size_t raw,uint8_t *id) {
    if (!a->sprite_cache_ready) return fd2_field_prepare_cached_sprite_id(state,raw,id);
    if (!state || !id || !state->unit_count || state->unit_count>FD2_SAVE_MAX_UNITS ||
        raw>=state->unit_count || a->sprite_cache_count>FD2_SAVE_MAX_UNITS) return -1;
    if (raw>=a->sprite_cache_count) return 0;
    *id=a->sprite_cache_ids[raw];return 1;
}

static int sprite_frame(const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,size_t raw,unsigned phase,size_t *frame) {
    uint8_t id=0;int cached=cached_sprite(a,state,raw,&id);
    if (cached<0) return -1;
    *frame=SIZE_MAX;
    if (cached) {
        if (!a->sprites.decoded_frames || (size_t)id*12u+11u>=a->sprites.frame_count) return -1;
        *frame=(size_t)id*12u+(phase==3u ? 1u:phase);
    }
    return 0;
}

/* sub_4E29C @0x4e29c：只有RLE skip填73，stride-2空位仍保底图。
 * 将这唯一差异投影成等长RUN73，再复用现有严格RLE decoder；不把全部
 * 解码0当skip，也不改FDICON cache。mode0普通4e22a，1灰阶4e1a6，2填73。 */
static int sprite_draw(fd2_vga *vga,const fd2_map_sprite_bank *bank,
        size_t frame,int x,int y,unsigned mode) {
    if (frame==SIZE_MAX) return 0;
    if (!bank || !bank->data || !bank->offsets || frame>=bank->frame_count ||
        bank->offsets[frame]>=bank->size || mode>2u ||
        x<0 || x>=VGA_W || y<0 || y>=VGA_H) return -1;
    uint8_t stream[24*24*2],pixels[24*24];size_t at=bank->offsets[frame],used=0;
    for (int row=0;row<24;row++) {
        for (int col=0;col<24;col++) pixels[24*row+col]=
            y+row<VGA_H && x+col<VGA_W ? vga->framebuffer[(y+row)*VGA_STRIDE+x+col]:0;
        for (int col=0;col<24;) {
            if (at>=bank->size) return -1;
            uint8_t command=bank->data[at++];size_t count=(command&63u)+1u;
            size_t span=(command&192u)==64u ? count*2u:count;
            if (span>(size_t)(24-col) || used>=sizeof(stream)) return -1;
            stream[used++]=mode==2u && command>=192u ? (uint8_t)(count-1u):command;
            if (command>=192u) {
                if (mode==2u) {if(used>=sizeof(stream))return -1;stream[used++]=73;}
            } else {
                size_t payload=(command&192u)==128u ? count:1u;
                if (payload>bank->size-at || payload>sizeof(stream)-used) return -1;
                for (size_t i=0;i<payload;i++) {
                    uint8_t value=bank->data[at++];
                    stream[used++]=mode==1u ? (uint8_t)((value&7u)+24u):value;
                }
            }
            col+=(int)span;
        }
    }
    if (fd2_image_decode_rle_pixels(pixels,24,24,stream,used)!=0) return -1;
    for (int row=0;row<24 && y+row<VGA_H;row++)
        for (int col=0;col<24 && x+col<VGA_W;col++)
            vga->framebuffer[(y+row)*VGA_STRIDE+x+col]=pixels[24*row+col];
    return 0;
}

int fd2_field_prepare_draw_items(fd2_vga *vga,const fd2_field_prepare_assets *a,
        const uint8_t *ids,size_t count,size_t first,int selected,int selling) {
    if (!base_valid(vga,a) || !ids || !count || count>INT_MAX || first>=count || (first&1u) ||
        selected< -1 || (selected>=0 && ((size_t)selected<first || (size_t)selected>=count)) ||
        !image_size(&a->service.price_icon,6,8)) return -1;
    size_t rows=count-first;
    if (count<=6u) {if(first) return -1;}
    else if (rows<5u) return -1;
    if (rows>6u) rows=6u;
    if (selected>=0 && (size_t)selected>=first+rows) return -1;
    view_name names[6];const uint8_t *items[6];
    for (size_t i=0;i<rows;i++) {
        items[i]=fd2_field_item_record_get(ids[first+i]);
        if (!items[i] || name_get(a,(size_t)ids[first+i]+181u,&names[i])!=0) return -1;
    }
    for (size_t i=0;i<9u;i++)
        if (!image_size(&a->info.detail_icons[i],i<5u ? 24:21,i<5u ? 20:8)) return -1;
    if (!image_size(&a->info.digit_overflow[0],18,8)) return -1;
    fd2_vga *page=malloc(sizeof(*page));if(!page)return -1;*page=*vga;
    int status=0;
    for (size_t i=0;i<rows && !status;i++) {
        const uint8_t *item=items[i];int x=10+148*(int)(i%2u),y=26*(int)(i/2u);
        size_t icon=item[0]<21u ? 0u:item[0]<32u ? 1u:2u;
        fd2_map_sprite_blit(page,&a->info.detail_icons[icon],x,y+119,-1);
        name_draw(page,a,&names[i],x+28,y+122,selected>=0 && first+i==(size_t)selected);
        int has_effect=1,value=0;size_t effect=0;
        if (item[0]<21u) {effect=5;value=fd2_field_item_i16(item,1);}
        else if (item[0]<32u) {effect=6;value=fd2_field_item_i16(item,5);}
        else if (item[0]==32u && (item[13]==5u || item[13]==11u)) {
            effect=item[13]==5u ? 7u:8u;value=fd2_field_item_i16(item,14);
        } else has_effect=0;
        if (has_effect) {
            fd2_map_sprite_blit(page,&a->info.detail_icons[effect],x+95,y+121,-1);
            status=fd2_field_info_number_draw(page,&a->info,x+118,y+121,value,3,1);
        } else fd2_map_sprite_blit(page,&a->info.digit_overflow[0],x+95,y+123,0);
        fd2_map_sprite_blit(page,&a->service.price_icon,x+95,y+131,-1);
        uint32_t price=fd2_field_item_purchase_price(item);
        if (selling) price=3u*price/4u;
        if (!status) status=fd2_field_info_number_draw(page,&a->info,x+104,y+131,(int32_t)price,5,2);
    }
    if (!status) memcpy(vga->framebuffer,page->framebuffer,sizeof(vga->framebuffer));
    free(page);return status ? -1:0;
}

int fd2_field_prepare_draw_equipment_recipients(fd2_vga *vga,
        const fd2_field_prepare_assets *a,const fd2_save_manual_state *state,
        const uint8_t *raws,size_t count,size_t first,int selected,uint8_t item_id,unsigned phase) {
    if (!base_valid(vga,a) || !state || !raws || !count || count>FD2_SAVE_MAX_UNITS || first>=count ||
        (count<=3u ? first!=0:first>count-3u) || selected< -1 ||
        (selected>=0 && ((size_t)selected<first || (size_t)selected>=count || (size_t)selected>=first+3u)) ||
        phase>3u || !state->unit_count || state->unit_count>FD2_SAVE_MAX_UNITS) return -1;
    for (size_t i=0;i<4u;i++)
        if (!image_size(&a->service.preview_labels[i],i==2u ? 16:13,8)) return -1;
    if (!image_size(&a->service.preview_arrow,7,6)) return -1;
    size_t rows=count-first;if(rows>3u)rows=3u;
    view_name names[3];int32_t next[3][4];fd2_field_unit units[3];size_t frames[3];
    for (size_t i=0;i<rows;i++) {
        size_t raw=raws[first+i];
        if (raw>=state->unit_count) return -1;
        memcpy(&units[i],state->unit_area+raw*80u,80u);
        if (name_get(a,(size_t)units[i].text_id+1u,&names[i])!=0 ||
            fd2_field_prepare_item_preview_stats(&units[i],item_id,next[i])!=0 ||
            sprite_frame(a,state,raw,phase,&frames[i])!=0) return -1;
    }
    fd2_vga *page=malloc(sizeof(*page));if(!page)return -1;*page=*vga;
    int status=0;
    for (size_t i=0;i<rows && !status;i++) {
        int y=117+26*(int)i;
        status=sprite_draw(page,&a->sprites,frames[i],14,y,2);
        if (status) break;
        name_draw(page,a,&names[i],40,y+4,selected>=0 && first+i==(size_t)selected);
        const uint16_t old[4]={units[i].attack,units[i].defense,units[i].accuracy,units[i].evasion};
        for (size_t j=0;j<4 && !status;j++) {
            int x=j<2u ? 122:196,ny=y+(j%2u ? 12:3);
            size_t bank=(int32_t)old[j]==next[i][j] ? 0u:(int32_t)old[j]>=next[i][j] ? 2u:1u;
            fd2_map_sprite_blit(page,&a->service.preview_labels[j],x,ny,0);
            fd2_map_sprite_blit(page,&a->service.preview_arrow,x+(j<2u ? 35:38),ny+1,0);
            status=fd2_field_info_number_draw(page,&a->info,x+(j<2u ? 15:18),ny,old[j],3,bank);
            if (!status) status=fd2_field_info_number_draw(page,&a->info,x+(j<2u ? 43:46),ny,next[i][j],3,bank);
        }
    }
    if (!status) memcpy(vga->framebuffer,page->framebuffer,sizeof(vga->framebuffer));
    free(page);return status ? -1:0;
}

int fd2_field_prepare_draw_members(fd2_vga *vga,const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,size_t first,int selected,unsigned phase) {
    if (!base_valid(vga,a) || !state || !state->unit_count || state->unit_count>FD2_SAVE_MAX_UNITS ||
        phase>3u || first>=state->unit_count || (first&1u) || selected< -1 ||
        (state->unit_count<=6u ? first!=0:first>state->unit_count-5u)) return -1;
    size_t rows=state->unit_count-first;if(rows>6u)rows=6u;
    if (selected>=0 && ((size_t)selected<first || (size_t)selected>=first+rows)) return -1;
    view_name names[6];size_t frames[6];
    for (size_t i=0;i<rows;i++) {
        if (name_get(a,(size_t)state->unit_area[(first+i)*80u+8u]+1u,&names[i])!=0 ||
            sprite_frame(a,state,first+i,phase,&frames[i])!=0) return -1;
    }
    fd2_vga *page=malloc(sizeof(*page));if(!page)return -1;*page=*vga;
    int status=0;
    for (size_t i=0;i<rows && !status;i++) {
        int x=132*(int)(i%2u),y=26*(int)(i/2u);
        status=sprite_draw(page,&a->sprites,frames[i],14+x,117+y,2);
        if (!status) name_draw(page,a,&names[i],40+x,121+y,selected>=0 && first+i==(size_t)selected);
    }
    if (!status) memcpy(vga->framebuffer,page->framebuffer,sizeof(vga->framebuffer));
    free(page);return status ? -1:0;
}

int fd2_field_prepare_draw_revive_candidates(fd2_vga *vga,const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,const uint8_t *raws,size_t count,
        size_t first,int selected,unsigned phase) {
    if (!base_valid(vga,a) || !state || !state->unit_count || state->unit_count>FD2_SAVE_MAX_UNITS ||
        !raws || !count || count>FD2_SAVE_MAX_UNITS || first>=count || phase>3u ||
        (count<=3u ? first!=0:first>count-3u) || selected< -1 ||
        (selected>=0 && ((size_t)selected<first || (size_t)selected>=count || (size_t)selected>=first+3u)) ||
        !image_size(&a->service.price_icon,6,8)) return -1;
    size_t rows=count-first;if(rows>3u)rows=3u;
    view_name names[3][3];size_t frames[3];uint32_t prices[3];
    for (size_t i=0;i<rows;i++) {
        size_t raw=raws[first+i];
        if (raw>=state->unit_count || sprite_frame(a,state,raw,phase,&frames[i])!=0 ||
            fd2_field_prepare_revive_price(state,raw,&prices[i])!=0 || prices[i]>INT32_MAX) return -1;
        const uint8_t *unit=state->unit_area+raw*80u;
        const size_t fragments[3]={(size_t)unit[8]+1u,(size_t)unit[31]+140u,(size_t)unit[32]+150u};
        for (size_t j=0;j<3;j++) if(name_get(a,fragments[j],&names[i][j])!=0)return -1;
    }
    fd2_vga *page=malloc(sizeof(*page));if(!page)return -1;*page=*vga;
    int status=0;
    for (size_t i=0;i<rows && !status;i++) {
        int y=26*(int)i,highlight=selected>=0 && first+i==(size_t)selected;
        status=sprite_draw(page,&a->sprites,frames[i],14,117+y,2);
        for (size_t j=0;j<3 && !status;j++)
            name_draw(page,a,&names[i][j],j==0u ? 40:j==1u ? 130:175,121+y,highlight);
        fd2_map_sprite_blit(page,&a->service.price_icon,220,125+y,-1);
        if (!status) status=fd2_field_info_number_draw(page,&a->info,228,125+y,(int32_t)prices[i],5,2);
    }
    if (!status) memcpy(vga->framebuffer,page->framebuffer,sizeof(vga->framebuffer));
    free(page);return status ? -1:0;
}

/* field_prepare_picker_tick_draw @0x27079 mode2（原sub_27079）：
 * 只重画至多3个候选sprite；姓名/价格/装备预览不属于该空等tick。 */
int fd2_field_prepare_draw_candidate_sprites(fd2_vga *vga,const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,const uint8_t *raws,size_t count,
        size_t first,unsigned phase) {
    if(!base_valid(vga,a) || !state || !state->unit_count || state->unit_count>FD2_SAVE_MAX_UNITS ||
        !raws || !count || count>FD2_SAVE_MAX_UNITS || first>=count || phase>3u)return -1;
    size_t rows=count-first;if(rows>3u)rows=3u;size_t frames[3];
    for(size_t i=0;i<rows;i++)
        if(raws[first+i]>=state->unit_count || sprite_frame(a,state,raws[first+i],phase,&frames[i])!=0)return -1;
    fd2_vga *page=malloc(sizeof(*page));if(!page)return -1;*page=*vga;
    int result=0;
    for(size_t i=0;i<rows && !result;i++)
        result=sprite_draw(page,&a->sprites,frames[i],14,117+26*(int)i,2);
    if(!result)memcpy(vga->framebuffer,page->framebuffer,sizeof(vga->framebuffer));
    free(page);return result ? -1:0;
}

/* sub_2A694 @0x2a694 / sub_2A857 @0x2a857：目标职业来自原转职记录，
 * 旧/新ID与特殊物品选择复用领域plan，不借复活价格或造第二候选规则。 */
int fd2_field_prepare_draw_promotion_candidates(fd2_vga *vga,const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,const uint8_t *raws,size_t count,
        size_t first,int selected,unsigned phase) {
    if (!base_valid(vga,a) || !state || !state->unit_count || state->unit_count>FD2_SAVE_MAX_UNITS ||
        !raws || !count || count>FD2_SAVE_MAX_UNITS || first>=count || phase>3u ||
        (count<=3u ? first!=0:first>count-3u) || selected< -1 ||
        (selected>=0 && ((size_t)selected<first || (size_t)selected>=count || (size_t)selected>=first+3u))) return -1;
    size_t rows=count-first;if(rows>3u)rows=3u;
    view_name names[3][4];size_t frames[3];
    for (size_t i=0;i<rows;i++) {
        size_t raw=raws[first+i];fd2_field_prepare_promotion plan;
        if (raw>=state->unit_count || sprite_frame(a,state,raw,phase,&frames[i])!=0 ||
            fd2_field_prepare_promotion_plan(state,raw,&plan)!=0 || plan.target_unit_id<32u ||
            (size_t)plan.target_unit_id-32u>=sizeof(k_prepare_promotion_records)/sizeof(k_prepare_promotion_records[0])) return -1;
        const uint8_t *unit=state->unit_area+raw*80u;
        const size_t fragments[4]={(size_t)unit[8]+1u,(size_t)unit[32]+150u,593u,
            (size_t)k_prepare_promotion_records[plan.target_unit_id-32u][0]+150u};
        for (size_t j=0;j<4;j++) if(name_get(a,fragments[j],&names[i][j])!=0)return -1;
    }
    fd2_vga *page=malloc(sizeof(*page));if(!page)return -1;*page=*vga;
    int status=0;
    const int xs[4]={40,130,175,239};
    for (size_t i=0;i<rows && !status;i++) {
        int y=26*(int)i;status=sprite_draw(page,&a->sprites,frames[i],14,117+y,2);
        for (size_t j=0;j<4 && !status;j++) name_draw(page,a,&names[i][j],xs[j],121+y,
            selected>=0 && first+i==(size_t)selected);
    }
    if (!status) memcpy(vga->framebuffer,page->framebuffer,sizeof(vga->framebuffer));
    free(page);return status ? -1:0;
}

/* 17fc0只消费统计素材；预检其实际读集，不借临时Game或开新资源owner。
 * detail_border属于portrait外框，2b4fb未调用，不把它们误加为此页前置。 */
static int stats_valid(const fd2_field_prepare_assets *a,const fd2_field_unit *unit) {
    view_name name;
    const size_t fragments[3]={(size_t)unit->text_id+1u,(size_t)unit->race+140u,
        (size_t)unit->movement_profile+150u};
    for (size_t i=0;i<3u;i++) if(name_get(a,fragments[i],&name)!=0)return 0;
    if (!image_size(&a->info.affiliation[unit->side==0],29,24) ||
        !image_size(&a->info.two_digit_overflow,12,8)) return 0;
    for (size_t i=0;i<8u;i++) if(!image_size(&a->info.bars[i],1,5))return 0;
    for (size_t i=0;i<3u;i++) {
        if (unit->detail_status[i] && !image_size(&a->info.status_icons[i],24,20)) return 0;
        if (!image_size(&a->info.digit_overflow[i],i==2u ? 17:18,8)) return 0;
        for (size_t j=0;j<10u;j++) if(!image_size(&a->info.digits[i][j],j==1u ? 5:6,8))return 0;
    }
    return 1;
}

int fd2_field_prepare_draw_deployment(fd2_vga *vga,const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,const uint8_t selected[FD2_SAVE_MAX_UNITS-1u],
        size_t quota,size_t cursor,unsigned phase,const fd2_image *label,const fd2_image *cursor_image) {
    size_t chosen;
    if (!vga || !a || !a->ready || !a->info.ready || !state || state->unit_count<2u ||
        state->unit_count>FD2_SAVE_MAX_UNITS || !selected || cursor>=state->unit_count-1u ||
        (quota!=15u && quota!=19u) || phase>3u ||
        fd2_field_prepare_selection_count(state->unit_count,selected,&chosen)!=0 ||
        !image_size(label,86,86) || !image_size(cursor_image,24,24) ||
        !image_size(&a->info.detail_top,223,86) || !image_size(&a->info.detail_bottom,310,99)) return -1;
    fd2_field_unit focus;memcpy(&focus,state->unit_area+(cursor+1u)*80u,sizeof(focus));
    if (!stats_valid(a,&focus)) return -1;
    size_t frames[FD2_SAVE_MAX_UNITS-1u];
    for (size_t i=0;i+1u<state->unit_count;i++)
        if(sprite_frame(a,state,i+1u,phase,&frames[i])!=0)return -1;
    fd2_vga *page=malloc(sizeof(*page));if(!page)return -1;*page=*vga;
    fd2_map_sprite_blit(page,&a->info.detail_top,92,7,-1);
    fd2_map_sprite_blit(page,&a->info.detail_bottom,5,94,-1);
    fd2_map_sprite_blit(page,label,5,7,0);
    int status=fd2_field_info_number_draw(page,&a->info,61,35,(int32_t)quota,2,0);
    if (!status) status=fd2_field_info_number_draw(page,&a->info,61,73,(int32_t)quota-(int32_t)chosen,2,1);
    if (!status) status=fd2_field_detail_stats_draw(page,&a->info,&a->font,&a->text,&focus);
    if (!status) fd2_map_sprite_blit(page,cursor_image,23+28*(int)(cursor%10u),104+30*(int)(cursor/10u),0);
    for (size_t i=0;i+1u<state->unit_count && !status;i++)
        status=sprite_draw(page,&a->sprites,frames[i],23+28*(int)(i%10u),
            100+30*(int)(i/10u)+(selected[i] ? 3:0),selected[i] ? 0u:1u);
    if (!status) memcpy(vga->framebuffer,page->framebuffer,sizeof(vga->framebuffer));
    free(page);return status ? -1:0;
}
