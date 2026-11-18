/* 准备服务的private有限领域叶；在prepare_ui与资源/UI helpers之后包含。
 * 来源：2872b/28cbd/28f65、2a43e/2aa00/2ac7d、2af28/2b777/2b843。
 * 分支、写点顺序、等待和取消均在preparation冷图；此处只借唯一manual
 * owner。每次读写80B用memcpy；给自己物品时按最新记录重读，不能两份覆盖。
 * 预检只用shadow；提交后host退出/晚错误不回滚原版已发生的独立写点。
 * G3规范命名（完整Domain函数/xref/原资源依据，非ABI推断）：
 * field_shop_stock_copy @0x26a0d（原登记 sub_26A0D）。
 * field_prepare_equipment_execute @0x28efe（原登记 sub_28EFE）。
 * field_deployment_required_member_check @0x2b439（原登记 sub_2B439）。
 */
#ifndef FD2_FIELD_PREPARE_SERVICE_FLOW_H
#define FD2_FIELD_PREPARE_SERVICE_FLOW_H

#include "field_item.h"
#include "static_data.h"
#include "text.h"

static int prepare_domain_read(const fd2_save_manual_state *state,int actor,fd2_field_unit *unit) {
    if (!state || !unit || state->unit_count>FD2_SAVE_MAX_UNITS || actor<0 || (size_t)actor>=state->unit_count) return -1;
    memcpy(unit,state->unit_area+(size_t)actor*FD2_SAVE_UNIT_RECORD_SIZE,sizeof(*unit));return 0;
}
static void prepare_domain_write(fd2_save_manual_state *state,int actor,const fd2_field_unit *unit) {
    memcpy(state->unit_area+(size_t)actor*FD2_SAVE_UNIT_RECORD_SIZE,unit,sizeof(*unit));
}
static int prepare_domain_equipment(fd2_field_unit *unit,int source,int selected) {
    size_t count=fd2_field_unit_inventory_count(unit);
    if ((source!=0 && source!=1) || !count || (source==1 && (selected<0 || (size_t)selected>=count))) return -1;
    return fd2_field_prepare_item_equip(unit,source==0 ? count-1u:(size_t)selected);
}
/* 未来cache/头像只读预检；这里绝不提前重建live目录或改record+2。 */
static int prepare_domain_identity_resources(prepare_ui *ui,uint8_t id) {
    if (!ui->assets || !ui->assets->ready || !ui->assets->sprites.decoded_frames ||
        (size_t)id*12u+11u>=ui->assets->sprites.frame_count) return -1;
    return prepare_service_notice_ready(ui,id);
}
static int prepare_domain_preflight(prepare_ui *ui,int kind) {
    prepare_service *s=&ui->service;fd2_save_manual_state *state=&ui->context->preparation;
    fd2_field_unit source,destination;uint8_t item;
    if (kind<0 || kind>6) return -1;
    if (kind==5) {
        if ((s->quota!=16u && s->quota!=20u) || state->unit_count<s->quota) return -1;
        if (prepare_service_selection_resources(ui)!=0 || prepare_service_notice_ready(ui,75)!=0) return -1;
        fd2_text_parameters parameters={ui->context->message_number,ui->context->message_new_fragment,ui->context->message_old_fragment};
        if (prepare_service_text_ready(ui,658,&parameters,95,119)!=0) return -1;
        /* 2b439可能在已排序后失败；所有未来动态名字从同一冷图读取，
         * 不把关卡必须成员列表再抄进C，也不等首写后才发现坏文本。 */
        const fd2_static_flow *flow=fd2_static_flow_lookup("preparation","deployment");
        if (!flow || !flow->nodes) return -1;
        uint8_t seen[256]={0};
        for (size_t i=0;i<flow->count;i++) {
            const fd2_static_flow_node *node=&flow->nodes[i];
            if (!node->op || strcmp(node->op,"required_member")) continue;
            if (node->args[0]<0 || node->args[0]>255) return -1;
            unsigned id=(unsigned)node->args[0];if (seen[id]) continue;seen[id]=1;
            parameters.new_fragment=(size_t)id+1u;
            if (prepare_service_text_ready(ui,657,&parameters,95,119)!=0) return -1;
        }
        return 0;
    }
    if (!ui->assets || !ui->assets->ready || !ui->assets->service.ready) return -1;
    if (prepare_domain_read(state,s->member,&source)!=0) return -1;
    if (kind==0) {
        const uint8_t *record=fd2_field_item_record_get(s->product_id);
        if (!record || fd2_field_prepare_item_append(&source,s->product_id)!=1) return -1;
        if (record[0]<32u) {
            fd2_text_parameters parameters={s->price,(size_t)s->product_id+181u,ui->context->message_old_fragment};
            if (s->selector<0 || s->selector>=6 ||
                prepare_service_notice_ready(ui,k_prepare_town_portraits[s->selector])!=0 ||
                prepare_service_text_ready(ui,k_prepare_buy_messages[3][s->selector],&parameters,12,119)!=0 ||
                prepare_domain_equipment(&source,0,0)!=0 || fd2_field_prepare_item_recompute(&source)!=0) return -1;
        }
    } else if (kind==1) {
        if (s->raw_slot<0 || fd2_field_prepare_item_selected_id(&source,(size_t)s->raw_slot,&item)!=0 ||
            !fd2_field_item_record_get(item) || fd2_field_prepare_item_remove(&source,(size_t)s->raw_slot)!=0 ||
            fd2_field_prepare_item_recompute(&source)!=0) return -1;
    } else if (kind==2) {
        if (s->raw_slot<0 || prepare_domain_read(state,s->receiver,&destination)!=0 ||
            fd2_field_prepare_item_give_preflight(&source,(size_t)s->raw_slot,
                s->member==s->receiver ? &source:&destination,&item)!=1) return -1;
    } else if (kind==3) {
        uint32_t price;fd2_save_manual_state shadow=*state;
        if (fd2_field_prepare_revive_price(state,(size_t)s->member,&price)!=0 || price!=s->price ||
            fd2_field_prepare_revive_clear_flags(&shadow,(size_t)s->member)!=0 ||
            fd2_field_prepare_revive_restore_hp(&shadow,(size_t)s->member)!=0) return -1;
    } else if (kind==4) {
        fd2_field_prepare_promotion plan;fd2_save_manual_state shadow=*state;
        if (fd2_field_prepare_promotion_plan(state,(size_t)s->member,&plan)!=0 ||
            fd2_field_prepare_promotion_consume(&shadow,&plan)!=0 ||
            fd2_field_prepare_promotion_identity(&shadow,&plan)!=0) return -1;
        for (unsigned gain=0;gain<6u;gain++)
            if (fd2_field_prepare_promotion_gain_apply(&shadow,&plan,(fd2_field_prepare_gain)gain)!=0) return -1;
        if (prepare_domain_read(&shadow,s->member,&source)!=0 || fd2_field_prepare_item_recompute(&source)!=0) return -1;
        prepare_domain_write(&shadow,s->member,&source);
        if (fd2_field_prepare_promotion_finalize(&shadow,&plan)!=0 || prepare_domain_identity_resources(ui,plan.target_unit_id)!=0) return -1;
        fd2_text_parameters parameters={0,(size_t)source.movement_profile+150u,ui->context->message_old_fragment};
        if (prepare_service_text_ready(ui,595,&parameters,95,119)!=0) return -1;
        unsigned line=1;
        for (unsigned gain=0;gain<6u;gain++) {
            uint16_t delta;if (fd2_field_prepare_promotion_gain_value(&plan,(fd2_field_prepare_gain)gain,&delta)!=0) return -1;
            if (!delta) continue;
            if (gain<5u && line==3u) line=2;
            parameters.value=delta;
            if (prepare_service_text_ready(ui,gain<5u ? 490u+gain:596u,&parameters,95,119+19*(int)line)!=0) return -1;
            if (gain<5u) line++;
        }
        s->promotion=plan;
        return prepare_service_promotion_prepare(ui);
    } else {
        if (s->raw_slot<0 || fd2_field_prepare_item_selected_id(&source,(size_t)s->raw_slot,&item)!=0 ||
            fd2_field_item_equip_compatibility(source.movement_profile,item)!=1 ||
            prepare_domain_equipment(&source,1,s->raw_slot)!=0 || fd2_field_prepare_item_recompute(&source)!=0) return -1;
    }
    return 0;
}

static int prepare_service_domain(prepare_ui *ui,const fd2_static_flow_node *node,unsigned *port) {
    if (!ui || !ui->context || !ui->context->has_preparation || !node || !node->op || !port ||
        ui->context->preparation.unit_count>FD2_SAVE_MAX_UNITS) return -1;
    prepare_service *s=&ui->service;fd2_save_manual_state *state=&ui->context->preparation;
    const char *op=node->op;unsigned next=FD2_FLOW_NEXT;fd2_field_unit unit;int result=0;
    if (!strcmp(op,"service_is_church")) next=s->selector==4 ? FD2_FLOW_YES:FD2_FLOW_NO;
    else if (!strcmp(op,"selector_is")) {
        if (node->args[0]<0 || node->args[0]>5) goto fatal;
        next=s->selector==node->args[0] ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"selected_is")) {
        if (node->args[0]<0 || node->args[0]>3) goto fatal;
        next=s->selected_command==node->args[0] ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"stock_prepare")) {
        const uint8_t *row=fd2_field_stage_prepare_record_get(state->stage_id);size_t offset,limit,count=0;
        if (!row || (s->selector!=1 && s->selector!=3 && s->selector!=5)) goto fatal;
        offset=s->selector==1 ? 3u:s->selector==3 ? 15u:23u;limit=s->selector==1 ? 12u:8u;
        uint8_t stock[12];while (count<limit && row[offset+count]!=255u) {
            if (!fd2_field_item_record_get(row[offset+count])) goto fatal;
            stock[count]=row[offset+count];count++;
        }
        memcpy(s->stock,stock,count);s->stock_count=count;s->product_index=s->product_first=0;
    } else if (!strcmp(op,"product_is_equipment")) {
        const uint8_t *item=fd2_field_item_record_get(s->product_id);if (!item) goto fatal;
        next=item[0]<32u ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"has_money")) next=fd2_field_prepare_money_can_pay(state->gold_or_flags,s->price) ? FD2_FLOW_YES:FD2_FLOW_NO;
    else if (!strcmp(op,"build_candidates")) {
        int kind=node->args[0];size_t count=0;uint8_t candidates[FD2_SAVE_MAX_UNITS],ids[8];
        if (kind==0) {
            const uint8_t *item=fd2_field_item_record_get(s->product_id);if (!item) goto fatal;
            for (size_t actor=0;actor<state->unit_count;actor++) {
                if (prepare_domain_read(state,(int)actor,&unit)!=0) goto fatal;
                int compatible=item[0]<32u ? fd2_field_item_equip_compatibility(unit.movement_profile,s->product_id):1;
                if (compatible<0) goto fatal;if (compatible) candidates[count++]=(uint8_t)actor;
            }
        } else if (kind==1) {
            if (prepare_domain_read(state,s->member,&unit)!=0 || fd2_field_prepare_item_visible_ids(&unit,ids,8u,&count)!=0) goto fatal;
            for (size_t i=0;i<count;i++) if (!fd2_field_item_record_get(ids[i])) goto fatal;
            memcpy(s->visible_items,ids,count);s->item_count=count;s->raw_slot=0;
        } else if (kind==2) result=fd2_field_prepare_revive_candidates(state,candidates,FD2_SAVE_MAX_UNITS,&count);
        else if (kind==3) result=fd2_field_prepare_promotion_candidates(state,candidates,FD2_SAVE_MAX_UNITS,&count);
        else goto fatal;
        if (result) goto fatal;
        if (kind!=1) {memcpy(s->candidates,candidates,count);s->candidate_count=count;}
        s->cursor=0;s->first=0;next=count ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"price_prepare")) {
        uint32_t price;int kind=node->args[0];uint8_t id=s->product_id;
        if (kind==1) {
            if (prepare_domain_read(state,s->member,&unit)!=0 || s->raw_slot<0 ||
                fd2_field_prepare_item_selected_id(&unit,(size_t)s->raw_slot,&id)!=0 || fd2_field_prepare_item_sell_price(id,&price)!=0) goto fatal;
            s->product_id=id;
        } else if (kind==0) {
            const uint8_t *item=fd2_field_item_record_get(id);if (!item) goto fatal;
            price=fd2_field_item_purchase_price(item);
        } else if (kind==2) {if (s->member<0 || fd2_field_prepare_revive_price(state,(size_t)s->member,&price)!=0) goto fatal;}
        else goto fatal;
        s->price=price;ui->context->message_number=price;
    } else if (!strcmp(op,"message_name")) {
        int kind=node->args[0];size_t fragment;
        if (kind==0) {if (!fd2_field_item_record_get(s->product_id)) goto fatal;fragment=(size_t)s->product_id+181u;}
        else if (kind==5) fragment=(size_t)s->required_text+1u;
        else {
            if (prepare_domain_read(state,kind==3 ? s->receiver:s->member,&unit)!=0) goto fatal;
            if (kind==1) fragment=(size_t)unit.unit_id+1u;
            else if (kind==2 || kind==3) fragment=(size_t)unit.text_id+1u;
            else if (kind==4) fragment=(size_t)unit.movement_profile+150u;
            else goto fatal;
        }
        ui->context->message_new_fragment=(uint32_t)fragment;
    } else if (!strcmp(op,"inventory_empty")) {
        /* 1bffe@1c04e..1c058：选择器完成后才检查count0，不预先跳过等待。 */
        if (prepare_domain_read(state,s->member,&unit)!=0) goto fatal;
        next=fd2_field_unit_inventory_count(&unit)==0u ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"inventory_full")) {
        if (node->args[0]<0 || node->args[0]>1 || prepare_domain_read(state,node->args[0] ? s->receiver:s->member,&unit)!=0) goto fatal;
        next=fd2_field_unit_inventory_count(&unit)==8u ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"item_capture")) {
        if (prepare_domain_read(state,s->member,&unit)!=0 || s->raw_slot<0 ||
            fd2_field_prepare_item_selected_id(&unit,(size_t)s->raw_slot,&s->captured_item)!=0 || !fd2_field_item_record_get(s->captured_item)) goto fatal;
    } else if (!strcmp(op,"item_append")) {
        int target=node->args[0],actor=target ? s->receiver:s->member;
        if (target<0 || target>1 || prepare_domain_read(state,actor,&unit)!=0 ||
            fd2_field_prepare_item_append(&unit,target ? s->captured_item:s->product_id)!=1) goto fatal;
        prepare_domain_write(state,actor,&unit);s->irreversible=1;
    } else if (!strcmp(op,"item_remove") || !strcmp(op,"equipment_set") || !strcmp(op,"recompute")) {
        if (prepare_domain_read(state,s->member,&unit)!=0) goto fatal;
        if (!strcmp(op,"item_remove")) result=s->raw_slot<0 ? -1:fd2_field_prepare_item_remove(&unit,(size_t)s->raw_slot);
        else if (!strcmp(op,"equipment_set")) result=prepare_domain_equipment(&unit,node->args[0],s->raw_slot);
        else result=fd2_field_prepare_item_recompute(&unit);
        if (result) goto fatal;prepare_domain_write(state,s->member,&unit);s->irreversible=1;
    } else if (!strcmp(op,"equipment_compatible")) {
        uint8_t id;if (prepare_domain_read(state,s->member,&unit)!=0 || s->raw_slot<0 ||
            fd2_field_prepare_item_selected_id(&unit,(size_t)s->raw_slot,&id)!=0) goto fatal;
        result=fd2_field_item_equip_compatibility(unit.movement_profile,id);if (result<0) goto fatal;
        next=result ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"money_add") || !strcmp(op,"money_sub")) {
        s->previous_money=state->gold_or_flags;int subtract=!strcmp(op,"money_sub");
        result=subtract ? fd2_field_prepare_money_subtract(&state->gold_or_flags,s->price):fd2_field_prepare_money_add(&state->gold_or_flags,s->price);
        if (result) goto fatal;s->irreversible=1;
        result=prepare_service_money_animate(ui,subtract);if (result) goto feedback_error;
    } else if (!strcmp(op,"revive_flags_clear") || !strcmp(op,"revive_hp")) {
        if (s->member<0) goto fatal;
        result=!strcmp(op,"revive_flags_clear") ? fd2_field_prepare_revive_clear_flags(state,(size_t)s->member):fd2_field_prepare_revive_restore_hp(state,(size_t)s->member);
        if (result) goto fatal;s->irreversible=1;
    } else if (!strcmp(op,"promotion_item_consume") || !strcmp(op,"promotion_identity") || !strcmp(op,"promotion_finalize")) {
        if (!strcmp(op,"promotion_item_consume")) result=fd2_field_prepare_promotion_consume(state,&s->promotion);
        else if (!strcmp(op,"promotion_identity")) {result=fd2_field_prepare_promotion_identity(state,&s->promotion);if (!result) s->growth_line=1;}
        else result=fd2_field_prepare_promotion_finalize(state,&s->promotion);
        if (result) goto fatal;s->irreversible=1;
    } else if (!strcmp(op,"growth_select") || !strcmp(op,"movement_prepare")) {
        int gain=!strcmp(op,"movement_prepare") ? 5:node->args[0];uint16_t delta;
        if (gain<0 || gain>5 || fd2_field_prepare_promotion_gain_value(&s->promotion,(fd2_field_prepare_gain)gain,&delta)!=0) goto fatal;
        s->growth_index=(unsigned)gain;s->growth_delta=delta;ui->context->message_number=delta;
    } else if (!strcmp(op,"growth_nonzero")) next=s->growth_delta ? FD2_FLOW_YES:FD2_FLOW_NO;
    else if (!strcmp(op,"growth_commit") || !strcmp(op,"movement_commit")) {
        unsigned gain=!strcmp(op,"movement_commit") ? 5u:s->growth_index;
        if (gain>5u || (!strcmp(op,"growth_commit") && gain>=5u) ||
            fd2_field_prepare_promotion_gain_apply(state,&s->promotion,(fd2_field_prepare_gain)gain)!=0) goto fatal;
        if (gain<5u) s->growth_line++;s->irreversible=1;
    } else if (!strcmp(op,"cache_rebuild")) {
        if (fd2_field_prepare_assets_bind_roster(ui->assets,state)!=0) goto fatal;
    } else if (!strcmp(op,"preflight")) {
        result=prepare_domain_preflight(ui,node->args[0]);if (result) goto feedback_error;
    } else if (!strcmp(op,"quota_set")) {
        if (node->args[0]!=16 && node->args[0]!=20) goto fatal;s->quota=(size_t)node->args[0];
    } else if (!strcmp(op,"deployment_mode")) {
        if (node->args[0]<0 || node->args[0]>1) goto fatal;s->notown_mode=node->args[0];
    } else if (!strcmp(op,"is_notown")) next=s->notown_mode ? FD2_FLOW_YES:FD2_FLOW_NO;
    else if (!strcmp(op,"roster_exceeds_quota")) {
        if (s->quota!=16u && s->quota!=20u) goto fatal;next=state->unit_count>s->quota ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"stage_is") || !strcmp(op,"stage_at_least")) {
        if (node->args[0]<0 || node->args[0]>32) goto fatal;
        int matches=!strcmp(op,"stage_is") ? state->stage_id==node->args[0]:state->stage_id>=node->args[0];next=matches ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"roster_has_text")) {
        if (node->args[0]<0 || node->args[0]>255) goto fatal;int found=0;
        for (size_t i=0;i<state->unit_count;i++) if (state->unit_area[i*80u+8u]==(uint8_t)node->args[0]) found=1;
        next=found ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"required_member")) {
        if (node->args[0]<0 || node->args[0]>255) goto fatal;
        result=fd2_field_prepare_prefix_has_text(state,s->quota,(uint8_t)node->args[0]);if (result<0) goto fatal;
        s->required_text=(uint8_t)node->args[0];next=result ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"prefix_reorder")) {
        if (node->args[0]<0 || node->args[0]>255 || fd2_field_prepare_roster_front(state,(uint8_t)node->args[0])!=1) goto fatal;
        s->irreversible=1;
    } else if (!strcmp(op,"selection_reset")) {
        if (fd2_field_prepare_selection_init(s->selection)!=0) goto fatal;s->cursor=0;
    } else if (!strcmp(op,"selection_toggle")) {
        int cursor=s->cursor;
        if (fd2_field_prepare_selection_step(state->unit_count,s->selection,&cursor,FD2_FIELD_PREPARE_SELECT_TOGGLE)!=1) goto fatal;
        if (prepare_service_selection_sound(ui,7,1)!=0) goto fatal;
    } else if (!strcmp(op,"selection_cursor_next")) {
        if (fd2_field_prepare_selection_step(state->unit_count,s->selection,&s->cursor,FD2_FIELD_PREPARE_SELECT_RIGHT)!=1 ||
            prepare_service_selection_sound(ui,0,0)!=0) goto fatal;
    } else if (!strcmp(op,"selection_full")) {
        size_t count;if ((s->quota!=16u && s->quota!=20u) || fd2_field_prepare_selection_count(state->unit_count,s->selection,&count)!=0) goto fatal;
        next=count==s->quota-1u ? FD2_FLOW_YES:FD2_FLOW_NO;
    } else if (!strcmp(op,"selection_reorder")) {
        if (fd2_field_prepare_roster_reorder(state,s->selection,s->quota)!=0) goto fatal;s->irreversible=1;
    } else if (!strcmp(op,"finish")) {
        if (node->args[0]<-1 || node->args[0]>1) goto fatal;s->done=1;s->result=node->args[0];
    } else return 0;
    *port=next;return 1;
feedback_error:
    if (result==1) s->host_quit=1;
fatal:
    *port=FD2_FLOW_ERROR;return -1;
}

#endif
