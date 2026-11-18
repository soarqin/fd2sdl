#ifndef FD2_TEST_C11_HOST_OBSERVER_H
#define FD2_TEST_C11_HOST_OBSERVER_H
/* 2185f/21a9e→21548→1c7c6的正常写点；只模拟既有宿主输出合同。
 * prefix是同次调用已达状态的关系oracle，不是资源/伤害模型的expected。
 * 0继续/skip，1HOST，-1真实ERROR；FINISH错误与HOST本身分账。 */
typedef struct {
    fd2_field_game *owner, *prefix;
    uint8_t id;
    fd2_field_magic_present_phase stop_phase;
    int stop_result, failure_at_stop, cleanup_error, bad, captured;
    unsigned prepared, preludes, casts, targets, finished, failed_finishes;
    uint8_t target_order[FD2_FIELD_MAX_UNITS];
} f4_c11_host_observer;

static int f4_c11_host_present(const fd2_field_game *game,
        const fd2_field_magic_present_event *event, void *userdata) {
    f4_c11_host_observer *p=userdata;
    if(game!=p->owner || event->magic_id!=p->id ||
        event->presentation_mode!=FD2_FIELD_MAGIC_MODE_MAP ||
        event->action_kind!=FD2_FIELD_MAGIC_ACTION_MAGIC) {p->bad=1;return -1;}
    switch(event->phase) {
        case FD2_FIELD_MAGIC_PRESENT_PREPARE:p->prepared++;break;
        case FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE:p->preludes++;break;
        case FD2_FIELD_MAGIC_PRESENT_CAST:p->casts++;break;
        case FD2_FIELD_MAGIC_PRESENT_TARGET:
            if(p->targets>=FD2_FIELD_MAX_UNITS){p->bad=1;return -1;}
            p->target_order[p->targets++]=(uint8_t)event->target_index;break;
        case FD2_FIELD_MAGIC_PRESENT_FINISH:
            p->finished++;p->failed_finishes+=event->action_failed!=0;
            return p->cleanup_error?-1:0;
        case FD2_FIELD_MAGIC_PRESENT_MAP_CALLER_PREAMBLE:return 0;
        default:p->bad=1;return -1;
    }
    if(event->phase==p->stop_phase &&
        (event->phase!=FD2_FIELD_MAGIC_PRESENT_TARGET || p->targets==1u) && p->stop_result) {
        if(p->captured){p->bad=1;return -1;}
        memcpy(p->prefix,game,sizeof(*game));p->captured=1;
        if(p->failure_at_stop)p->owner->failure=FD2_FIELD_FAILURE_ACTION;
        return p->stop_result;
    }
    return 0;
}
#endif
