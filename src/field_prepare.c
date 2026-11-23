/* G3准备纯领域：field_deployment_select_execute @0x2af28（原sub_2AF28）、
 * persistent_roster_selection_reorder @0x2b777（原sub_2B777）、2b439/2b843；
 * field_church_revive_execute @0x2a43e（原sub_2A43E）、field_church_promote_execute
 * @0x2aa00（原sub_2AA00）、field_promotion_candidates_build @0x2ae0e（原sub_2AE0E）
 * 及field_growth_stat_present_apply @0x1e529（原sub_1E529）/2ac7d。
 * 来源为G1完整Domain/strict LE读集，G3另核30byte选择位/共享招募尾。
 * 函数只保原字段写点，服务编排/资源预检/等待/取消归准备冷定义及Play。
 * G3规范命名（完整Domain函数/xref/原资源依据，非ABI推断）：
 * field_revive_candidates_build @0x2a07a（原登记 sub_2A07A）。
 * field_unit_item_slot_find_prefix @0x2aedb（原登记 sub_2AEDB）。
 * field_deployment_selection_count @0x2b749（原登记 sub_2B749）。
 * persistent_roster_text_front @0x2b843（原登记 sub_2B843）。
 */
#include "field_prepare.h"

#include <string.h>
#include "field_item.h"
#include "field_prepare_item.h"
#include "field_unit_base.h"
#include "settings.h"
#include "static_data/preparation.inc"

int fd2_field_prepare_stage_is_notown(size_t stage) {
    return stage<sizeof(k_prepare_no_town) && k_prepare_no_town[stage]!=0;
}

int fd2_field_prepare_stage_is_valid(size_t stage) {
    return stage>0u && stage<sizeof(k_prepare_no_town) &&
        (fd2_field_prepare_stage_is_notown(stage) || fd2_field_stage_prepare_record_get(stage));
}

static int state_valid(const fd2_save_manual_state *state) {
    return state && state->unit_count<=FD2_SAVE_MAX_UNITS;
}
static int unit_read(const fd2_save_manual_state *state,size_t index,fd2_field_unit *unit) {
    if (!state_valid(state) || !unit || index>=state->unit_count) return -1;
    memcpy(unit,state->unit_area+index*FD2_SAVE_UNIT_RECORD_SIZE,sizeof(*unit));return 0;
}
static void unit_write(fd2_save_manual_state *state,size_t index,const fd2_field_unit *unit) {
    memcpy(state->unit_area+index*FD2_SAVE_UNIT_RECORD_SIZE,unit,sizeof(*unit));
}
static uint16_t word_read(const uint8_t *p) { return (uint16_t)(p[0]|(uint16_t)p[1]<<8); }
static void word_write(uint8_t *p,uint16_t value) { p[0]=(uint8_t)value;p[1]=(uint8_t)(value>>8); }

int fd2_field_prepare_selection_init(uint8_t selected[FD2_FIELD_PREPARE_COMPANION_CAPACITY]) {
    if (!selected) return -1;
    memset(selected,0,FD2_FIELD_PREPARE_COMPANION_CAPACITY);return 0;
}
int fd2_field_prepare_selection_count(size_t roster_count,const uint8_t selected[FD2_FIELD_PREPARE_COMPANION_CAPACITY],size_t *count) {
    if (!selected || !count || !roster_count || roster_count>FD2_SAVE_MAX_UNITS) return -1;
    size_t used=0;
    for (size_t i=0;i+1u<roster_count;i++) { if (selected[i]>1u) return -1;used+=selected[i]; }
    *count=used;return 0;
}
int fd2_field_prepare_selection_step(size_t roster_count,uint8_t selected[FD2_FIELD_PREPARE_COMPANION_CAPACITY],int *cursor,fd2_field_prepare_select_key key) {
    size_t count;
    if (!cursor || roster_count<2u || fd2_field_prepare_selection_count(roster_count,selected,&count)!=0 ||
        *cursor<0 || (size_t)*cursor>=roster_count-1u) return -1;
    int n=(int)roster_count-1,index=*cursor;
    switch (key) {
        case FD2_FIELD_PREPARE_SELECT_LEFT: index=index ? index-1:n-1;break;
        case FD2_FIELD_PREPARE_SELECT_TOGGLE: selected[index]^=1u; /* 原Enter随后按右移。 */
            /* fall through */
        case FD2_FIELD_PREPARE_SELECT_RIGHT: index=index+1==n ? 0:index+1;break;
        case FD2_FIELD_PREPARE_SELECT_UP: if (index<=9) return 0;index-=10;break;
        case FD2_FIELD_PREPARE_SELECT_DOWN: if (index>=n-10) return 0;index+=10;break;
        default:return -1;
    }
    *cursor=index;return 1;
}
int fd2_field_prepare_roster_reorder(fd2_save_manual_state *state,const uint8_t selected[FD2_FIELD_PREPARE_COMPANION_CAPACITY],size_t capacity) {
    size_t used;
    if (!state_valid(state) || (capacity!=16u && capacity!=20u) || state->unit_count<capacity ||
        fd2_field_prepare_selection_count(state->unit_count,selected,&used)!=0 || used!=capacity-1u) return -1;
    uint8_t old[FD2_SAVE_UNIT_TABLE_SIZE];memcpy(old,state->unit_area,sizeof(old));
    size_t out=1;
    for (unsigned chosen=1;chosen<3;chosen++)
        for (size_t i=1;i<state->unit_count;i++)
            if ((selected[i-1]!=0)==(chosen==1u))
                memcpy(state->unit_area+(out++)*FD2_SAVE_UNIT_RECORD_SIZE,old+i*FD2_SAVE_UNIT_RECORD_SIZE,FD2_SAVE_UNIT_RECORD_SIZE);
    return 0;
}
int fd2_field_prepare_prefix_has_text(const fd2_save_manual_state *state,size_t capacity,uint8_t text_id) {
    if (!state_valid(state) || (capacity!=16u && capacity!=20u) || state->unit_count<capacity) return -1;
    for (size_t i=1;i<capacity;i++) if (state->unit_area[i*FD2_SAVE_UNIT_RECORD_SIZE+8u]==text_id) return 1;
    return 0;
}
int fd2_field_prepare_roster_front(fd2_save_manual_state *state,uint8_t text_id) {
    if (!state_valid(state)) return -1;
    /* 2b843扫描全部同伴并反复覆盖found，重复text取最后一项。 */
    size_t found=SIZE_MAX;
    for(size_t i=1;i<state->unit_count;i++)
        if(state->unit_area[i*FD2_SAVE_UNIT_RECORD_SIZE+8u]==text_id)found=i;
    if(found!=SIZE_MAX) {
        uint8_t unit[FD2_SAVE_UNIT_RECORD_SIZE];memcpy(unit,state->unit_area+found*FD2_SAVE_UNIT_RECORD_SIZE,sizeof(unit));
        memmove(state->unit_area+2u*FD2_SAVE_UNIT_RECORD_SIZE,state->unit_area+FD2_SAVE_UNIT_RECORD_SIZE,(found-1u)*FD2_SAVE_UNIT_RECORD_SIZE);
        memcpy(state->unit_area+FD2_SAVE_UNIT_RECORD_SIZE,unit,sizeof(unit));return 1;
    }
    return 0;
}
static int deployment_slot_accepts(size_t stage,size_t active,const fd2_field_unit *unit) {
    for (size_t i=0;i<sizeof(k_prepare_deployment_roster_gate)/sizeof(k_prepare_deployment_roster_gate[0]);i++) {
        const uint8_t *rule=k_prepare_deployment_roster_gate[i];
        if (stage>=rule[0] && stage<=rule[1] && active==rule[2] && unit->text_id!=rule[3]) return 0;
    }
    return 1;
}
int fd2_field_prepare_deployment_project(const fd2_save_manual_state *state,size_t stage,
        const fd2_field_map *map,const fd2_field_metadata *metadata,
        const fd2_field_placements *placements,fd2_field_units *out) {
    if (!state_valid(state) || !state->unit_count || state->stage_id!=stage || !map ||
        !map->cells || map->width<=0 || map->height<=0 || !metadata || !placements || !out ||
        !metadata->unknown_01 || metadata->unknown_01>FD2_FIELD_MAX_UNITS) return -1;
    fd2_field_units projected;fd2_field_units_clear(&projected);projected.count=metadata->unknown_01;
    size_t roster=0;
    for (size_t active=0;active<projected.count;active++) {
        fd2_field_unit *unit=&projected.items[active];
        projected.walk_frames[active]=1;projected.source_template_indices[active]=FD2_FIELD_UNIT_NO_TEMPLATE;
        if (roster>=state->unit_count) {unit->flags=1;continue;}
        fd2_field_unit source;if (unit_read(state,roster,&source)!=0) return -1;
        if (!deployment_slot_accepts(stage,active,&source)) {unit->flags=1;continue;}
        uint8_t x,y;
        if (fd2_field_unit_deployment_position(metadata,placements,roster,&x,&y)!=0 ||
            x>=map->width || y>=map->height) return -1;
        *unit=source;unit->x=x;unit->y=y;unit->direction=unit->frame_phase=0;unit->side=2;
        ((uint8_t *)unit)[0x31]=0xff;memset((uint8_t *)unit+0x22,0,6);
        if (fd2_field_prepare_item_recompute(unit)!=0 || fd2_field_units_sprite_cache_append(&projected,active)!=0) return -1;
        roster++;
    }
    *out=projected;return 0;
}

int fd2_field_prepare_revive_candidates(const fd2_save_manual_state *state,uint8_t *actors,size_t capacity,size_t *count) {
    if (!state_valid(state) || !count) return -1;
    uint8_t list[FD2_SAVE_MAX_UNITS];size_t n=0;
    for (size_t i=0;i<state->unit_count;i++) if (state->unit_area[i*FD2_SAVE_UNIT_RECORD_SIZE+5u]&1u) list[n++]=(uint8_t)i;
    if (n>capacity || (n && !actors)) return -1;
    if (n) memcpy(actors,list,n);*count=n;return 0;
}
static int revive_profile_valid(const fd2_field_unit *unit) {
    return unit->movement_profile<sizeof(k_prepare_revive_profile_valid) &&
        k_prepare_revive_profile_valid[unit->movement_profile]!=0;
}
int fd2_field_prepare_revive_price(const fd2_save_manual_state *state,size_t actor_index,uint32_t *price) {
    fd2_field_unit unit;
    if (!price || unit_read(state,actor_index,&unit)!=0 || !(unit.flags&1u) || !revive_profile_valid(&unit)) return -1;
    *price=(uint32_t)k_prepare_revive_coefficients[unit.movement_profile]*unit.level;return 0;
}
int fd2_field_prepare_revive_clear_flags(fd2_save_manual_state *state,size_t actor_index) {
    uint32_t price;if (fd2_field_prepare_revive_price(state,actor_index,&price)!=0) return -1;
    state->unit_area[actor_index*FD2_SAVE_UNIT_RECORD_SIZE+5u]=0;return 0;
}
int fd2_field_prepare_revive_restore_hp(fd2_save_manual_state *state,size_t actor_index) {
    fd2_field_unit unit;
    if (unit_read(state,actor_index,&unit)!=0 || unit.flags || !revive_profile_valid(&unit)) return -1;
    unit.hp=unit.hp_max;unit_write(state,actor_index,&unit);return 0;
}

/* 2aedb遍历visible count个原raw槽，不能改成扫描8槽后跳hidden。 */
static int find_item_prefix(const fd2_field_unit *unit,uint8_t item) {
    size_t count=fd2_field_unit_inventory_count(unit);
    for (size_t i=0;i<count;i++) { uint8_t id;if (fd2_field_prepare_item_selected_id(unit,i,&id)!=0) return -1;if (id==item) return (int)i; }
    return -1;
}
static int promotion_plan_valid(const fd2_field_prepare_promotion *plan) {
    if (!plan || plan->actor_index>=FD2_SAVE_MAX_UNITS || plan->old_unit_id>=18u || plan->old_unit_id==7u ||
        plan->target_unit_id<32u || plan->target_unit_id>=68u || plan->special_slot< -1 || plan->special_slot>=8) return 0;
    if (plan->target_unit_id==(uint8_t)(plan->old_unit_id+32u)) return plan->special_slot==-1;
    return plan->special_slot>=0 && (plan->target_unit_id==(uint8_t)(plan->old_unit_id+50u) ||
        (plan->old_unit_id==9u && plan->target_unit_id==52u));
}
int fd2_field_prepare_promotion_plan(const fd2_save_manual_state *state,size_t actor_index,fd2_field_prepare_promotion *plan) {
    fd2_field_unit unit;
    if (!plan || unit_read(state,actor_index,&unit)!=0 || unit.level<20u || unit.unit_id>=18u || unit.unit_id==7u) return -1;
    fd2_field_prepare_promotion out={(uint8_t)actor_index,unit.unit_id,(uint8_t)(unit.unit_id+32u),-1};
    int slot=find_item_prefix(&unit,k_prepare_special_items[unit.unit_id]);
    if (slot>=0) out.target_unit_id=(uint8_t)(unit.unit_id+50u);
    if (unit.unit_id==9u && find_item_prefix(&unit,90)>=0) out.target_unit_id=52;
    if (out.target_unit_id>=50u) {
        slot=find_item_prefix(&unit,out.target_unit_id==52u ? 90:k_prepare_special_items[unit.unit_id]);
        /* 原52分支一律查询90；缺失将remove(-1)，不开放无定义槽写。 */
        if (slot<0) return -1;
        out.special_slot=(int8_t)slot;
    }
    *plan=out;return 0;
}
int fd2_field_prepare_promotion_candidates(const fd2_save_manual_state *state,uint8_t *actors,size_t capacity,size_t *count) {
    if (!state_valid(state) || !count) return -1;
    uint8_t list[FD2_SAVE_MAX_UNITS];size_t n=0;
    for (size_t i=0;i<state->unit_count;i++) {
        const uint8_t *u=state->unit_area+i*FD2_SAVE_UNIT_RECORD_SIZE;
        if (u[0x21]>=20u && u[7]<18u && u[7]!=7u) list[n++]=(uint8_t)i;
    }
    if (n>capacity || (n && !actors)) return -1;
    if (n) memcpy(actors,list,n);*count=n;return 0;
}
int fd2_field_prepare_promotion_consume(fd2_save_manual_state *state,const fd2_field_prepare_promotion *plan) {
    fd2_field_prepare_promotion current;
    if (!promotion_plan_valid(plan) || fd2_field_prepare_promotion_plan(state,plan->actor_index,&current)!=0 ||
        current.old_unit_id!=plan->old_unit_id || current.target_unit_id!=plan->target_unit_id || current.special_slot!=plan->special_slot) return -1;
    if (plan->special_slot<0) return 0;
    fd2_field_unit unit;if (unit_read(state,plan->actor_index,&unit)!=0 ||
        fd2_field_prepare_item_remove(&unit,(size_t)plan->special_slot)!=0) return -1;
    unit_write(state,plan->actor_index,&unit);return 0;
}
int fd2_field_prepare_promotion_identity(fd2_save_manual_state *state,const fd2_field_prepare_promotion *plan) {
    fd2_field_unit unit;
    if (!promotion_plan_valid(plan) || unit_read(state,plan->actor_index,&unit)!=0 ||
        unit.unit_id!=plan->old_unit_id || unit.level<20u) return -1;
    unit.unit_id=plan->target_unit_id;unit.movement_profile=k_prepare_promotion_records[plan->target_unit_id-32u][0];
    unit_write(state,plan->actor_index,&unit);return 0;
}
int fd2_field_prepare_promotion_gain_value(const fd2_field_prepare_promotion *plan,
        fd2_field_prepare_gain kind, fd2_field_combat_rng_fn rng, void *rng_data, uint16_t *value) {
    if (!promotion_plan_valid(plan) || !value || kind<FD2_FIELD_PREPARE_GAIN_ATTACK || kind>FD2_FIELD_PREPARE_GAIN_MOVEMENT) return -1;
    const uint8_t *growth=fd2_field_unit_growth_record_get(plan->target_unit_id);if (!growth) return -1;
    *value=kind==FD2_FIELD_PREPARE_GAIN_MOVEMENT ? k_prepare_promotion_records[plan->target_unit_id-32u][1]:
        fd2_field_growth_delta(growth + 2u*(unsigned)kind, FD2_GROWTH_RANDOM, rng, rng_data);
    return 0;
}
static int promoted_unit_read(const fd2_save_manual_state *state,const fd2_field_prepare_promotion *plan,fd2_field_unit *unit) {
    return promotion_plan_valid(plan) && unit_read(state,plan->actor_index,unit)==0 && unit->unit_id==plan->target_unit_id &&
        unit->movement_profile==k_prepare_promotion_records[plan->target_unit_id-32u][0] && unit->level>=20u ? 0:-1;
}
int fd2_field_prepare_promotion_gain_apply(fd2_save_manual_state *state,const fd2_field_prepare_promotion *plan,
        fd2_field_prepare_gain kind, uint16_t value) {
    static const uint8_t offsets[]={0x37,0x39,0x3e,0x42,0x46};
    uint16_t upper;fd2_field_unit unit;
    if (fd2_field_prepare_promotion_gain_value(plan,kind,NULL,NULL,&upper)!=0 ||
        value > upper || promoted_unit_read(state,plan,&unit)!=0) return -1;
    if (kind==FD2_FIELD_PREPARE_GAIN_MOVEMENT) unit.movement_points=(uint8_t)(unit.movement_points+value);
    else { uint8_t *p=(uint8_t *)&unit+offsets[(unsigned)kind];word_write(p,(uint16_t)(word_read(p)+value)); }
    unit_write(state,plan->actor_index,&unit);return 0;
}
int fd2_field_prepare_promotion_finalize(fd2_save_manual_state *state,const fd2_field_prepare_promotion *plan) {
    fd2_field_unit unit;if (promoted_unit_read(state,plan,&unit)!=0) return -1;
    unit.level=1;unit.experience=0;unit.hp=unit.hp_max;unit.mp=unit.mp_max;
    unit_write(state,plan->actor_index,&unit);return 0;
}
