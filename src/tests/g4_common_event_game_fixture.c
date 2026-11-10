/* G4非shipping窗口机制夹具：1aa1d mode2 / callback39@352ca。
 * 只给原Game lookup增加本测试的显式alias，其余main/Play/热owner/资源/
 * RNG/提交代码全部沿用生产。生产冷表和关卡支持门不变，不作S已支持声明。
 * 原callback39从52742取得child(0,211,0)，返回后用当前stage的story11；
 * stage0 FDTXT1[11]为10token/text_id1；runner显式将active raw0.text_id设1，
 * 使12c60取得合法unit0及focus，不沿原START无匹配时的低址读取取portrait。 */
#define fd2_static_flow_lookup fd2_g4_common_fixture_lookup
#include "../field_game.c"
#undef fd2_static_flow_lookup

const fd2_static_flow *fd2_static_flow_lookup(const char *domain,const char *id);

const fd2_static_flow *fd2_g4_common_fixture_lookup(const char *domain,const char *id) {
    if(domain && id && !strcmp(domain,"common_event") && !strcmp(id,"stage.000.action.039")) {
        static int reported;
        if(!reported) {
            fprintf(stderr,"G4 common-event mechanism alias: stage.000.action.039 -> common.loot.child.0\n");
            reported=1;
        }
        return fd2_static_flow_lookup("common_event","common.loot.child.0");
    }
    return fd2_static_flow_lookup(domain,id);
}
