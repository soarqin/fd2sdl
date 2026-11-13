/* S2非shipping结果机制：只在本TU允许受控stage29/21raw资源与快照导入。
 * stage20的callback47只在本机制夹具允许18..96raw。
 * 复用生产Game/Save实现和原owner；不改正式capability、stage调度或SAV。
 * result source20bf5在另一个真实Play TU显式启动，不作为自然stage29开放。 */
#include "../field_game.c"

static int s2_result_source_capability(size_t stage,fd2_field_stage_capability *out) {
    if(stage==29 && out){*out=(fd2_field_stage_capability){0,21,21,0};return 0;}
    if(stage==20 && out){*out=(fd2_field_stage_capability){0,18,0,0};return 0;}
    return fd2_field_stage_capability_get(stage,out);
}
#define fd2_field_stage_capability_get s2_result_source_capability
#include "../field_save.c"
#undef fd2_field_stage_capability_get

int fd2_s2_result_native_open(fd2_field_game *game,const fd2_archive *other,const char *path) {
    fd2_save_file save={0};fd2_save_battle_snapshot snapshot;
    int result=-1;
    if(!game || !other || !path || fd2_save_file_open(&save,path)!=0)goto done;
    if(fd2_save_file_get_battle_snapshot(&save,&snapshot)!=0)goto done;
    size_t stage=snapshot.meta[FD2_SAVE_BATTLE_META_STAGE],count=snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT];
    if(stage==29) {
        if(count!=21 || snapshot.unit_area[5]&1 || !(snapshot.unit_area[80+5]&1) || snapshot.unit_area[20*80+5]&1)goto done;
    } else if(stage!=20 || count<18 || count>96)goto done;
    /* 仅本夹具绕正式import入口的stage<=2门，资源已同stage准备；
     * 实际恢复仍走生产同stage校验/提交函数，不冒充正式CONTINUE开放。 */
    if(open_resources_for_flow(game,other,stage,1)!=0 || import_current_stage(game,&snapshot)!=0)goto done;
    result=0;
done:
    fd2_save_file_close(&save);
    if(result && game)fd2_field_game_close(game);
    return result;
}
