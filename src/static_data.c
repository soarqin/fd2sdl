/* 冷流程定义：来源由data/static/manifest.json绑定原已确认C/IDA依据。
 * 同一份节点供Game、Scene、预检及unit preview消费，不复制可编辑真值。
 */
#include "static_data.h"
#include "static_data/scene_flows.inc"
#include "static_data/stage_flows.inc"
#include "static_data/progress_flows.inc"
#include "static_data/loot_flows.inc"
#include "static_data/magic_flows.inc"
#include "static_data/preparation_flows.inc"

const fd2_static_flow *fd2_static_flow_lookup(const char *domain, const char *id) {
    if (!domain || !id) return NULL;
    if (!strcmp(domain, "scene")) return fd2_static_flow_find(k_scene_flows, FD2_SCENE_FLOW_COUNT, id);
    if (!strcmp(domain, "stage")) return fd2_static_flow_find(k_stage_flows, FD2_STAGE_FLOW_COUNT, id);
    if (!strcmp(domain, "progress")) return fd2_static_flow_find(k_progress_flows, FD2_PROGRESS_FLOW_COUNT, id);
    if (!strcmp(domain, "loot")) return fd2_static_flow_find(k_loot_flows, FD2_LOOT_FLOW_COUNT, id);
    if (!strcmp(domain, "magic")) return fd2_static_flow_find(k_magic_flows, FD2_MAGIC_FLOW_COUNT, id);
    if (!strcmp(domain, "preparation")) return fd2_static_flow_find(k_preparation_flows, FD2_PREPARATION_FLOW_COUNT, id);
    return NULL;
}
