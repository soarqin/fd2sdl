/* S2非shipping机制别名：原mode2 WORD70→35e5b完整callback。
 * 实际main/Play/Game/Scene与原owner不变；只是测试lookup提供显式源图，
 * 不注册任何新stage绑定，不提升正式支持门。 */
#define fd2_static_flow_lookup fd2_s2_callback_native_lookup
#include "../field_game.c"
#undef fd2_static_flow_lookup
const fd2_static_flow *fd2_static_flow_lookup(const char *domain,const char *id);
const fd2_static_flow *fd2_s2_callback_native_lookup(const char *domain,const char *id) {
    if(domain && id && !strcmp(domain,"common_event") && !strcmp(id,"stage.000.action.070")) {
        static int reported;
        if(!reported) {
            fprintf(stderr,"S2 mechanism alias: stage.000.action.070 -> common.callback.070\n");reported=1;
        }
        return fd2_static_flow_lookup("common_event","common.callback.070");
    }
    return fd2_static_flow_lookup(domain,id);
}
