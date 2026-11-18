/* 炎龙骑士团 2 SDL3 重写 - 正式战场循环入口
 *
 * 依据field_controller_input @0x117e7、field_player_command_execute @0x18d8c、
 * field_player_magic_command_execute @0x1cff0与
 * field_player_item_command_execute @0x1bbdc（原登记均同名），以及
 * field_unit_inventory_select @0x1b932（原sub_1B932）。
 * B4依据field_unit_experience_reward_execute @0x1e292（原sub_1E292）、
 * field_growth_stat_present_apply @0x1e529（原sub_1E529）及
 * field_phase_status_tick @0x1a866（原登记同名）。
 * B5依据field_loot_descriptors_execute @0x1aa1d（原sub_1AA1D）、
 * field_special_cell_execute @0x190ac（原sub_190AC）、
 * field_stage0_victory_execute @0x22ef6（原sub_22EF6）及
 * persistent_roster_sync_from_battle @0x11506（原sub_11506）。
 * 此处编排输入、呈现和场景退出；玩法记录与提交仍由field_game持有。
 */

#include "field_play.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "field_ai.h"
#include "battle_animation.h"
#include "field_attack.h"
#include "field_game.h"
#include "field_rng.h"
#include "field_round.h"
#include "field_defeat.h"
#include "field_visual.h"
#include "bios_clock.h"
#include "field_render_scope.h"
#include "font.h"
#include "input.h"
#include "portrait.h"
#include "text.h"
#include "scene.h"
#include "static_data.h"

/* 每次调用对应一个原读点；失败不改out，不送错误哨兵给纯tick投影。 */
static int play_clock_ns(fd2_vga *vga,uint64_t *out){return fd2_vga_observe_checked(vga,SDL_GetTicksNS(),out);}
static int play_clock_tick(fd2_vga *vga,int32_t *out){uint64_t actual;if(play_clock_ns(vga,&actual)<0)return -1;*out=fd2_bios_tick_signed_ns(actual);return 0;}
static int play_clock_low(fd2_vga *vga,uint16_t *out){uint64_t actual;if(play_clock_ns(vga,&actual)<0)return -1;*out=fd2_bios_tick_low_ns(actual);return 0;}
static int play_clock_bits(fd2_vga *vga,uint32_t *out){int32_t tick;if(play_clock_tick(vga,&tick)<0)return -1;*out=(uint32_t)tick;return 0;}

/* 仅输入空等的宿主pump节流；有限copy不借此构造原版动画等待。 */
#define FD2_FIELD_COMMAND_FRAME_MS 16

/* 与VGA impl相同的input泵位置；有限copy不履行/取消外来持帧。 */
static int play_present_copy(fd2_vga *vga) {
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    fd2_input_begin_frame(&vga->input);
    fd2_vga_upload_current_frame(vga);
    return fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0?-1:0;
}


static int play_field_progress(fd2_field_game *game, fd2_vga *vga,
                                const fd2_archive *fdother,
                                fd2_field_audio *audio, int fast);
static int loot_fixed_wait(fd2_vga *vga,unsigned milliseconds,int fast);
static int play_command_transition(fd2_field_game *game,fd2_vga *vga,
    fd2_field_audio *audio,int opening);

/* sub_11AA8 @0x11aa8（原登记同名）：先peek，再water，BIOS变化才
 * 11cac(0)，最后重新取clock写App 539F2。不能按每宿主帧调用1297d。 */
static int play_browse_wait_step(fd2_field_game *game,fd2_vga *vga) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if(!game || !vga || game->failure!=FD2_FIELD_FAILURE_NONE)return FD2_SCENE_RESULT_ERROR;
    if(fd2_input_take_quit(&vga->input) || game->runtime_host_quit)return FD2_SCENE_RESULT_HOST_QUIT;
    if(fd2_input_has_any_key(&vga->input))return FD2_SCENE_RESULT_OK;
    if(fd2_field_water_tick_live(vga)<0)return FD2_SCENE_RESULT_ERROR;
    uint16_t tick;if(play_clock_low(vga,&tick)<0)return FD2_SCENE_RESULT_ERROR;
    if(tick!=vga->browse_input_tick) {
        if(fd2_field_game_runtime_refresh(game,vga,0)!=0 || game->failure!=FD2_FIELD_FAILURE_NONE)
            return FD2_SCENE_RESULT_ERROR;
        if(play_clock_low(vga,&vga->browse_input_tick)<0)return FD2_SCENE_RESULT_ERROR;
    }
    return FD2_SCENE_RESULT_OK;
}

/* sub_12DAC @0x12dac（原登记同名）：115b6的ESI候选索引带入局部
 * 等待锚。一次读键后结束该局部生命，不能借539F2跨读键保留。 */
typedef struct { int32_t tick; int active; } field_selector_wait;
static int play_selector_wait_step(fd2_field_game *game,fd2_vga *vga,int32_t *tick) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if(!game || !vga || !tick || game->failure!=FD2_FIELD_FAILURE_NONE)return FD2_SCENE_RESULT_ERROR;
    if(fd2_input_take_quit(&vga->input) || game->runtime_host_quit)return FD2_SCENE_RESULT_HOST_QUIT;
    if(fd2_input_has_any_key(&vga->input))return FD2_SCENE_RESULT_OK;
    if(fd2_field_water_tick_live(vga)<0)return FD2_SCENE_RESULT_ERROR;
    int32_t now;if(play_clock_tick(vga,&now)<0)return FD2_SCENE_RESULT_ERROR;
    if(now!=*tick) {
        if(fd2_field_game_runtime_refresh(game,vga,0)!=0 || game->failure!=FD2_FIELD_FAILURE_NONE)
            return FD2_SCENE_RESULT_ERROR;
        if(play_clock_tick(vga,tick)<0)return FD2_SCENE_RESULT_ERROR;
    }
    return FD2_SCENE_RESULT_OK;
}

static int selector_input_context(fd2_field_interaction interaction) {
    return interaction==FD2_FIELD_INTERACTION_UNIT_SELECTED || interaction==FD2_FIELD_INTERACTION_TARGETING ||
        interaction==FD2_FIELD_INTERACTION_MAGIC_TARGETING || interaction==FD2_FIELD_INTERACTION_MAGIC_TELEPORTING ||
        interaction==FD2_FIELD_INTERACTION_ITEM_TARGETING || interaction==FD2_FIELD_INTERACTION_ITEM_TELEPORTING ||
        interaction==FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING;
}

/* field_unit_selection_summary_wait @0x18b84（原sub_18B84）：18890初选后
 * 非消费式等待下一键，field_unit_summary_draw @0x18c6d（原sub_18C6D）
 * 只在这段等待显示。原caller已消费最初确认；返回时保留新键供mode4
 * 调度，包含无映射普通键，不建立第二份interaction或跨帧输入队列。
 * 完整DWORD首次ESI来源仍未资格；首次暂保候选的先画行为，不猜原初值。
 * 成功body后18c65已证fresh signed tick，随后空轮恢复完整DWORD相等门。
 * have_anchor只标本实现已到18c65，不是原版字段或首次比较资格。 */
static int play_summary_wait_step(fd2_field_game *game,fd2_vga *vga,size_t actor,
        int summary_left,int *have_anchor,int32_t *anchor,int *key_ready) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if(!game || !vga || !have_anchor || !anchor || !key_ready ||
        game->failure!=FD2_FIELD_FAILURE_NONE)return FD2_SCENE_RESULT_ERROR;
    *key_ready=0;
    if(game->runtime_host_quit || fd2_input_take_quit(&vga->input))return FD2_SCENE_RESULT_HOST_QUIT;
    if(fd2_input_has_any_key(&vga->input)){*key_ready=1;return FD2_SCENE_RESULT_OK;}
    if(fd2_field_water_tick_live(vga)<0)return FD2_SCENE_RESULT_ERROR;
    int due=!*have_anchor;
    if(!due){int32_t tick;if(play_clock_tick(vga,&tick)<0)return FD2_SCENE_RESULT_ERROR;due=tick!=*anchor;}
    if(due) {
        if(fd2_field_water_tick_live(vga)<0 ||
            fd2_field_game_runtime_summary_refresh(game,vga,actor,summary_left)!=0)
            return FD2_SCENE_RESULT_ERROR;
        if(play_clock_tick(vga,anchor)<0)return FD2_SCENE_RESULT_ERROR;
        *have_anchor=1;
    }
    return FD2_SCENE_RESULT_OK;
}

static int play_field_move_selection_wait_key(fd2_field_game *game,
                                               fd2_vga *vga) {
    if (!game || !vga || !game->ready ||
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count)
        return FD2_SCENE_RESULT_ERROR;
    size_t actor=(size_t)game->selected_unit;
    int summary_left=(int64_t)game->cursor_cell_x-game->camera_cell_x>=7;
    int have_anchor=0;
    int32_t anchor; /* 仅在have_anchor=1后读取；首次不合成原ESI。 */
    for (;;) {
        int key_ready;
        int result=play_summary_wait_step(game,vga,actor,summary_left,&have_anchor,&anchor,&key_ready);
        if(result!=FD2_SCENE_RESULT_OK || key_ready)return result;
        if(play_present_copy(vga)<0)return FD2_SCENE_RESULT_ERROR;
        if(fd2_vga_delay_ms(vga,16)<0)return FD2_SCENE_RESULT_ERROR;
    }
}

/* 原 main @0x25bf4 的指令0x25d90..0x25db1：只在新 App 启动时初始化并预推进共享流。
 * 显式环境 seed 仅供固定输入夹具；空值／非法值沿正式启动来源。 */
void fd2_field_play_context_init(fd2_field_play_context *context,
                                 fd2_bgm_player *bgm) {
    if (!context) return;
    memset(context, 0, sizeof(*context));
    (void)fd2_field_rng_bootstrap(&context->rng, getenv("FD2SDL_FIELD_RNG_SEED"));
    context->bgm = bgm;
    context->music_enabled = context->sfx_enabled = 1;
    context->camera_option=1;context->effect_option=0;
    context->initialized = 1;
}

int fd2_field_play_request_music(fd2_vga *vga,fd2_field_play_context *context,
    size_t stage, int track, const char *reason) {
    if (!vga || !context || !context->initialized || track < -1) return -1;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    int result = 0;
    if (context->bgm) {
        result = track == -1 ? fd2_bgm_stop(context->bgm) :
            fd2_bgm_play(context->bgm, (size_t)track, 0);
    }
    /* Audio/BGM内部完整source前缀照实保留；只在同步返回边界阻止UI后缀。 */
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    printf("music request: stage=%zu track=%d reason=%s available=%d result=%d current=%d loop=0\n",
        stage, track, reason ? reason : "unspecified", context->bgm != NULL, result,
        context->bgm ? fd2_bgm_current_track(context->bgm) : -1);
    fflush(stdout);
    if (result != 0) {
        context->music_error = 1;
        fprintf(stderr, "音乐请求失败：stage=%zu track=%d。\n", stage, track);
    }
    return result;
}

/* 生命周期仅覆盖当前run_context；不在Game保存第二个VGA/错误owner。 */
typedef struct {fd2_field_play_context *context;fd2_vga *vga;} field_music_scope;
static int field_phase_music(size_t stage,int track,
        fd2_field_music_reason reason,void *userdata) {
    field_music_scope *scope=userdata;
    static const char *const reasons[]={"phase-stop","player","enemy"};
    const char *name=(unsigned)reason<sizeof(reasons)/sizeof(reasons[0])
        ? reasons[(unsigned)reason]:"unknown-phase";
    /* 保旧optional音乐失败政策；只SOURCE/同输出失败阻止业务phase。 */
    (void)fd2_field_play_request_music(scope->vga,scope->context,stage,track,name);
    return fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0 ? -1:0;
}



typedef struct {
    fd2_vga *vga;
    fd2_field_audio *audio;
    int fast;
    int host_quit;
    fd2_field_defeat_visual *defeat; /* 仅本次1db65的图像/临时页，非业务owner。 */
    fd2_field_play_context *context; /* 借原App音乐owner，源剧情同步报错。 */
} field_runtime_presentation;

/* runtime呈现仅回传hostquit时，Game锁存仍是同一退出事实。modal不能把
 * progress_resume的busy当成可继续工作；已锁存业务错误仍先于宿主退出。 */
static int play_field_terminal_result(const fd2_field_game *game,fd2_vga *vga) {
    if (!game || !vga || game->failure!=FD2_FIELD_FAILURE_NONE || (fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))
        return FD2_SCENE_RESULT_ERROR;
    int quit=fd2_input_take_quit(&vga->input);
    return game->runtime_host_quit || quit ? FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_OK;
}

/* H16ms只归局部空等；临时借字段后归还foreign，不以同值认owner。 */
static void modal_host_release(fd2_vga *vga,uint64_t *deadline) {
    (void)vga;*deadline=0;
}
static int modal_host_present(fd2_vga *vga,uint64_t *deadline) {
    uint64_t foreign_deadline=vga->frame_deadline_ns,foreign_interval=vga->frame_interval_ns;
    vga->frame_deadline_ns=*deadline;
    vga->frame_interval_ns=*deadline ? UINT64_C(16000000):0;
    int status=fd2_vga_present_timed(vga,FD2_FIELD_COMMAND_FRAME_MS);
    *deadline=vga->frame_deadline_ns;
    vga->frame_deadline_ns=foreign_deadline;vga->frame_interval_ns=foreign_interval;
    return status;
}

static int runtime_wait(fd2_vga *vga, uint32_t ms, unsigned ticks,
        void *userdata) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return -1;
    field_runtime_presentation *runtime=userdata;
    if (fd2_input_take_quit(&vga->input)) runtime->host_quit=1;
    if (runtime->host_quit) return 1;
    if(play_present_copy(vga)<0)return FD2_SCENE_RESULT_ERROR;
    if (!runtime->fast) {
        if(ticks){if(fd2_vga_wait_ticks(vga,ticks)<0)return -1;}
        else if(ms && fd2_vga_delay_ms(vga,ms)<0)return -1;
    }
    if (fd2_input_take_quit(&vga->input)) runtime->host_quit=1;
    return runtime->host_quit?1:0;
}

static int runtime_project(fd2_vga *vga, void *userdata) {
    (void)userdata;
    return fd2_field_page_project(vga->field_page,72,72,
        vga->framebuffer,sizeof(vga->framebuffer));
}

typedef struct {
    field_runtime_presentation *runtime;
    fd2_field_game *game;
} field_runtime_scope;

static int runtime_scope_wait(fd2_vga *vga,uint32_t ms,unsigned ticks,void *userdata) {
    return runtime_wait(vga,ms,ticks,((field_runtime_scope *)userdata)->runtime);
}
static int runtime_refresh(fd2_vga *vga,void *userdata) {
    field_runtime_scope *scope=userdata;
    return fd2_field_game_runtime_refresh(scope->game,vga,0);
}
static int runtime_phase_redraw(fd2_vga *vga,void *userdata) {
    field_runtime_scope *scope=userdata;
    return fd2_field_game_runtime_refresh(scope->game,vga,1);
}
static int runtime_clear_keys(fd2_vga *vga,void *userdata) {
    (void)userdata;fd2_input_clear_keys(&vga->input);
    return fd2_input_take_quit(&vga->input)?1:0;
}

static int runtime_recovery_eligible(const fd2_field_unit *unit) {
    return unit->side==2u && !(unit->flags&0x81u) &&
        !unit->detail_status[0] && !unit->detail_status[1] && unit->hp!=unit->hp_max;
}

static int runtime_recovery_actor(fd2_field_game *game,fd2_vga *vga,
        size_t raw,unsigned mode) {
    if (vga->field_visual.actor_idle_phase>3u || vga->field_visual.cursor_phase>3u) return -1;
    return fd2_field_map_action_page_actor(vga->field_page,&game->sprites,
        &game->units,raw,game->camera_cell_x,game->camera_cell_y,
        (uint8_t)vga->field_visual.actor_idle_phase,(uint8_t)vga->field_visual.cursor_phase,mode);
}

/* field_turn_cycle_run @0x1a30b（原FUN_00017a53）的原有同步显示服务。
 * 分支/顺序归runtime.turncycle，HP/acted/phase仍只在Game提交。 */
static int play_runtime_step(fd2_field_game *game,
        fd2_field_runtime_present_step step,int value,void *userdata) {
    field_runtime_presentation *runtime=userdata;
    fd2_vga *vga=runtime->vga;
    field_runtime_scope scope={runtime,game};
    const fd2_field_round_hooks hooks={runtime_scope_wait,runtime_project,
        runtime_refresh,runtime_phase_redraw,runtime_clear_keys};
    int result=0;
    /* finally CLOSE必须先于hostquit门，不能让退出锁存泄漏视觉资源。 */
    if(step==FD2_FIELD_RUNTIME_DEFEAT_CLOSE) {
        fd2_field_defeat_close(&runtime->defeat);return value? -1:0;
    }
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    if(step==FD2_FIELD_RUNTIME_FOCUS_BEGIN || step==FD2_FIELD_RUNTIME_FOCUS_STEP ||
            step==FD2_FIELD_RUNTIME_CURSOR_STEP) {
        if(value<0 || game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
        if(fd2_input_take_quit(&vga->input) || runtime->host_quit || game->runtime_host_quit) {
            runtime->host_quit=1;return 1;
        }
        result=runtime_refresh(vga,&scope);
        if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
        if(play_present_copy(vga)<0)return FD2_SCENE_RESULT_ERROR;
        if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
        if(fd2_input_take_quit(&vga->input)){runtime->host_quit=1;return 1;}
        if(step==FD2_FIELD_RUNTIME_FOCUS_STEP && value!=0 && value!=6) {
            /* 12cea本步已由方向helper刷新；runtime_wait还会present，
             * 此处只执行原17aa9(1)，不能重复上传/采输入。 */
            if(!runtime->fast && fd2_vga_wait_ticks(vga,1)<0)return -1;
            if(fd2_input_take_quit(&vga->input)){runtime->host_quit=1;return 1;}
        }
    } else if(step>=FD2_FIELD_RUNTIME_DEFEAT_BEGIN && step<=FD2_FIELD_RUNTIME_DEFEAT_WAIT) {
        if(fd2_input_take_quit(&vga->input) || runtime->host_quit) {
            runtime->host_quit=1;return 1;
        }
        if(step==FD2_FIELD_RUNTIME_DEFEAT_BEGIN) {
            if(value)return -1;
            if(runtime->audio && runtime->audio->player && game->option_values[1] &&
                (!runtime->audio->ui_bank || fd2_pcm_bank_count(runtime->audio->ui_bank)<=3))return -1;
            result=fd2_field_defeat_prepare(&runtime->defeat,game,vga,game->fdother);
        } else if(step==FD2_FIELD_RUNTIME_DEFEAT_ROTATE_PAGE)
            result=value? -1:fd2_field_defeat_rotate(runtime->defeat,game,vga);
        else if(step==FD2_FIELD_RUNTIME_DEFEAT_ROTATE_ACTOR)
            result=value<0? -1:fd2_field_defeat_actor(runtime->defeat,game,vga,(size_t)value);
        else if(step==FD2_FIELD_RUNTIME_DEFEAT_ROTATE_PROJECT)
            result=value? -1:fd2_field_defeat_project(runtime->defeat,game,vga);
        else if(step==FD2_FIELD_RUNTIME_DEFEAT_BASE)
            result=value? -1:fd2_field_defeat_base(runtime->defeat,game,vga);
        else if(step==FD2_FIELD_RUNTIME_DEFEAT_CUE) {
            if(value || !runtime->defeat)return -1;
            if(runtime->audio && runtime->audio->player && game->option_values[1])
                result=fd2_pcm_play_replace(runtime->audio->player,runtime->audio->ui_bank,
                    3,1,FD2_GAME_SFX_DEFAULT_GAIN);
        } else if(step==FD2_FIELD_RUNTIME_DEFEAT_FRAME)
            result=value<0? -1:fd2_field_defeat_frame(runtime->defeat,game,vga,(unsigned)value);
        else if(step==FD2_FIELD_RUNTIME_DEFEAT_WAIT)
            result=value!=1 || !runtime->defeat? -1:runtime_wait(vga,0,1,runtime);
        else result=-1;
    } else if (step==FD2_FIELD_RUNTIME_CELL_OPEN_CUE) {
        /* field_special_cell_execute @0x190ac（原登记同名）：YES后的sfx_play_secondary
         * @0x25b45（原登记同名），UI31/cue12；其后等待归loot冷图。 */
        if(value!=12)return -1;
        if(fd2_input_take_quit(&vga->input) || runtime->host_quit) {
            runtime->host_quit=1;return 1;
        }
        if(runtime->audio && runtime->audio->secondary_player && game->option_values[1]) {
            if(!runtime->audio->ui_bank || fd2_pcm_bank_count(runtime->audio->ui_bank)<=12)return -1;
            result=fd2_pcm_play_replace(runtime->audio->secondary_player,runtime->audio->ui_bank,
                12,1,FD2_GAME_SFX_DEFAULT_GAIN);
        }
    } else if (step==FD2_FIELD_RUNTIME_RECOVERY_WAIT) {
        if (value && (!fd2_field_round_is_ready(&game->info_assets) ||
                !fd2_field_phase_is_ready(&game->info_assets))) return -1;
        /* 13565/16f55的51AAC跨度覆盖整个turncycle；新Game/Load不重置。 */
        vga->info_panel_suppressed=1;
        result=runtime_wait(vga,0,(unsigned)value,runtime);
    } else if (step==FD2_FIELD_RUNTIME_RECOVERY_HIGHLIGHT) {
        for (size_t i=0;i<game->units.count;i++)
            if (runtime_recovery_eligible(&game->units.items[i]) &&
                runtime_recovery_actor(game,vga,i,2)!=0) return -1;
        result=runtime_project(vga,NULL);
    } else if (step==FD2_FIELD_RUNTIME_RECOVERY_CUE) {
        int any=0;
        for (size_t i=0;i<game->units.count;i++) any|=runtime_recovery_eligible(&game->units.items[i]);
        if (any && runtime->audio && runtime->audio->player && game->option_values[1])
            result=fd2_pcm_play_replace(runtime->audio->player,runtime->audio->ui_bank,
                4,1,FD2_GAME_SFX_DEFAULT_GAIN);
    } else if (step==FD2_FIELD_RUNTIME_RECOVERY_NORMAL_ACTOR) {
        if (value<0 || (size_t)value>=game->units.count) return -1;
        result=runtime_recovery_actor(game,vga,(size_t)value,0);
    } else if (step==FD2_FIELD_RUNTIME_RECOVERY_PROJECT) result=runtime_project(vga,NULL);
    else if (step==FD2_FIELD_RUNTIME_REFRESH) {
        if(value!=0 && value!=3)return -1;
        if(value==0)result=runtime_refresh(vga,&scope);
        else {
            if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
            if(fd2_input_take_quit(&vga->input) || runtime->host_quit || game->runtime_host_quit) {
                runtime->host_quit=1;return 1;
            }
            result=fd2_field_game_runtime_refresh(game,vga,3);
            if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
            if(result>0){runtime->host_quit=1;return 1;}
            if(play_present_copy(vga)<0)return FD2_SCENE_RESULT_ERROR; /* 15178的11cac(1)，随后wait1/2不重复提交。 */
            if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
            if(fd2_input_take_quit(&vga->input) || game->runtime_host_quit)result=1;
        }
    }
    else if (step==FD2_FIELD_RUNTIME_ENEMY_TITLE || step==FD2_FIELD_RUNTIME_PLAYER_TITLE)
        result=fd2_field_phase_play(vga,&game->info_assets,vga->field_page,
            step==FD2_FIELD_RUNTIME_ENEMY_TITLE?FD2_FIELD_PHASE_ENEMY:FD2_FIELD_PHASE_PLAYER,
            &hooks,&scope);
    else if (step==FD2_FIELD_RUNTIME_ROUND)
        result=fd2_field_round_play(vga,&game->info_assets,game->turn_number,&hooks,&scope);
    else if (step==FD2_FIELD_RUNTIME_CLEAR_KEYS) {
        result=runtime_clear_keys(vga,NULL);
    } else if(step==FD2_FIELD_RUNTIME_WAIT_TICKS ||
              (step==FD2_FIELD_RUNTIME_DELAY_MS && value==200)) {
        if(step==FD2_FIELD_RUNTIME_WAIT_TICKS && (value<0 || value>65535))return -1;
        if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
        if(fd2_input_take_quit(&vga->input) || runtime->host_quit || game->runtime_host_quit) {
            runtime->host_quit=1;return 1;
        }
        /* 15055的独立17aa9/200ms；不压info、不预检round，也不添present。 */
        if(!runtime->fast) {
            if(step==FD2_FIELD_RUNTIME_WAIT_TICKS){if(fd2_vga_wait_ticks(vga,(uint32_t)value)<0)return -1;}
            else if(fd2_vga_delay_ms(vga,200)<0)return -1;
        }
        if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
        if(fd2_input_take_quit(&vga->input) || game->runtime_host_quit)result=1;
    } else if(step==FD2_FIELD_RUNTIME_DELAY_MS) {
        if(value!=20)return -1;
        int wait=loot_fixed_wait(vga,(unsigned)value,runtime->fast);
        result=wait==FD2_SCENE_RESULT_HOST_QUIT?1:wait;
    } else if(step==FD2_FIELD_RUNTIME_MUSIC_TRACK) {
        if(value < -1 || value > 255)return -1;
        result=runtime->context ? fd2_field_play_request_music(vga,runtime->context,game->stage,value,"source"):0;
    } else if(step==FD2_FIELD_RUNTIME_INFO_VISIBLE) {
        if(value<0 || value>1)return -1;
        if(fd2_input_take_quit(&vga->input) || runtime->host_quit){runtime->host_quit=1;return 1;}
        vga->info_panel_suppressed=(uint8_t)!value;
    } else return -1;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
    if (result>0) runtime->host_quit=1;
    return result;
}

/* 17898/19953共享53c13/53c17；Game仅持当次渲染投影，不另起live锚。
 * 普通菜单phase直接使用，确认框必须除2，不能用phase&1。 */
static int menu_highlight_live(fd2_vga *vga,int confirm) {
    uint32_t comparison_anchor=vga->menu_highlight_tick;
    uint16_t tick;if(play_clock_low(vga,&tick)<0)return -1;
    int changed=fd2_field_system_menu_highlight_tick(&vga->menu_highlight_phase,
        &comparison_anchor,tick,confirm);
    /* 原phase写入后才重读BIOS存53c17；比较值不写回真实owner。 */
    if(changed>0 && play_clock_bits(vga,&vga->menu_highlight_tick)<0)return -1;
    return changed;
}

static int shared_menu_tick(fd2_field_game *game,fd2_vga *vga) {
    int command=game->interaction==FD2_FIELD_INTERACTION_COMMAND ||
        game->interaction==FD2_FIELD_INTERACTION_ITEM_MENU;
    int system=game->interaction==FD2_FIELD_INTERACTION_SYSTEM_MENU;
    if (!command && !system) return -1;
    int confirm=system && game->system_menu.page==FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
    if(menu_highlight_live(vga,confirm)<0)return -1;
    if (command) game->command_highlight_phase=(uint8_t)vga->menu_highlight_phase;
    if (system) game->system_menu.highlight_phase=(uint8_t)(confirm
        ?vga->menu_highlight_phase/2u:vga->menu_highlight_phase);
    return 0;
}

static int command_input_context(const fd2_field_game *game) {
    return game->interaction==FD2_FIELD_INTERACTION_COMMAND ||
        game->interaction==FD2_FIELD_INTERACTION_ITEM_MENU ||
        (game->interaction==FD2_FIELD_INTERACTION_SYSTEM_MENU &&
         game->system_menu.page!=FD2_FIELD_SYSTEM_PAGE_CONFIRMATION);
}

/* 17898空等每轮都合成，不能借11aa8的BIOS-change外门。 */
static int play_command_wait_step(fd2_field_game *game,fd2_vga *vga) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if(!game || !vga || game->failure!=FD2_FIELD_FAILURE_NONE ||
        !command_input_context(game))return FD2_SCENE_RESULT_ERROR;
    if(fd2_input_take_quit(&vga->input) || game->runtime_host_quit)return FD2_SCENE_RESULT_HOST_QUIT;
    if(fd2_input_has_any_key(&vga->input))return FD2_SCENE_RESULT_OK;
    if(fd2_field_water_tick_live(vga)<0 || shared_menu_tick(game,vga)<0 ||
        fd2_field_game_runtime_menu_refresh(game,vga)!=0 || game->failure!=FD2_FIELD_FAILURE_NONE)
        return FD2_SCENE_RESULT_ERROR;
    return FD2_SCENE_RESULT_OK;
}

/* field_bottom_notice_open @0x1956b（原sub_1956B）与
 * field_bottom_notice_close @0x196cb（原sub_196CB）共用滑入框，借当前焦点。
 * page保留原始空框供关闭，后续文字只写真实VGA，不能回写成另一份单位真值。 */
typedef struct {
    uint8_t *background;
    fd2_vga *page;
    fd2_image portraits[4];
    int portrait_origin;
    int row;
} field_progress_box;

typedef fd2_text_expanded field_progress_text;
typedef fd2_text_parameters field_text_parameters;
#define FIELD_TEXT_SAFE_MINUS FD2_TEXT_SAFE_MINUS

/* 与准备服务共用纯展开；安全负号资源仍由当前Game呈现owner验证。 */
static int field_text_expand(const fd2_field_game *game,
        const field_text_parameters *parameters, size_t fragment,
        unsigned depth, field_progress_text *text) {
    if (depth || fd2_text_expand(&game->ui_text, game->font.glyph_count,
            parameters, fragment, text) != 0) return -1;
    for (size_t i = 0; i < text->count; i++) if (text->tokens[i] == FIELD_TEXT_SAFE_MINUS) {
        const fd2_image *minus = &game->info_assets.signs[1];
        if (!minus->pixels || minus->width < 1 || minus->width > 16 ||
            minus->height < 1 || minus->height > 16) return -1;
    }
    return 0;
}

/* 预检DATO同一立绘的4帧都能解码为80×80；不保留解码结果。 */
static int portrait_frames_valid(const fd2_field_game *game, size_t portrait) {
    for (unsigned frame = 0; frame < 4; frame++) {
        fd2_image image = {0};
        int valid = fd2_portrait_decode_frame(&image, &game->dato_archive, portrait, frame) == 0 &&
            image.width == 80 && image.height == 80;
        fd2_image_free(&image);
        if (!valid) return 0;
    }
    return 1;
}

static int progress_text_prepare(const fd2_field_game *game,
                                  const fd2_field_progress_notice *notice,
                                  field_progress_text *text) {
    if (!game || !notice || !text || !game->font.glyphs ||
        !game->font.glyph_count || game->font.glyph_count > game->font.size / 32u) return -1;
    memset(text, 0, sizeof(*text));
    switch (notice->kind) {
        case FD2_FIELD_PROGRESS_CLOSE: return notice->fragment == 0 ? 0 : -1;
        case FD2_FIELD_PROGRESS_GAIN: if (notice->fragment != 488) return -1; break;
        case FD2_FIELD_PROGRESS_LEVEL: if (notice->fragment != 489) return -1; break;
        case FD2_FIELD_PROGRESS_GROWTH:
            if (notice->fragment < 490 || notice->fragment > 494) return -1;
            break;
        case FD2_FIELD_PROGRESS_LEARN: if (notice->fragment != 587) return -1; break;
        case FD2_FIELD_PROGRESS_STATUS:
            if (notice->fragment < 481 || notice->fragment > 487) return -1;
            break;
        default: return -1;
    }
    field_text_parameters parameters = {notice->value,game->message_new_fragment,game->message_old_fragment};
    if (notice->kind == FD2_FIELD_PROGRESS_LEARN) {
        if (notice->magic_id >= 36) return -1;
        parameters.new_fragment = 441u + notice->magic_id;
    }
    return field_text_expand(game, &parameters, notice->fragment, 0, text);
}

static int progress_resources_preflight(const fd2_field_game *game,
                                        const fd2_field_progress_notice *notice,
                                        void *userdata) {
    const fd2_archive *fdother = userdata;
    field_progress_text text;
    if (!game || !notice || notice->actor_index >= game->units.count ||
        !game->map.cells || fd2_scene_field_message_validate(fdother) != 0 ||
        progress_text_prepare(game, notice, &text) != 0) return -1;
    return portrait_frames_valid(game, game->units.items[notice->actor_index].unit_id) ? 0 : -1;
}

static void progress_box_free(field_progress_box *box) {
    if (!box) return;
    for (size_t i = 0; i < 4; i++) fd2_image_free(&box->portraits[i]);
    free(box->background);
    free(box->page);
    memset(box, 0, sizeof(*box));
}

static void progress_portrait(field_progress_box *box, fd2_vga *vga,
                               unsigned frame, int opening) {
    const fd2_image *image = &box->portraits[frame];
    int x = box->portrait_origin % VGA_STRIDE;
    int y = box->portrait_origin / VGA_STRIDE;
    int reverse = opening || box->portrait_origin == 36887;
    /* 1956b初始一律reverse；16559仅普通下框reverse，128..132保持forward。 */
    for (int row = 0; row < image->height; row++)
        for (int column = 0; column < image->width; column++) {
            int dx = reverse ? x - column : x + column;
            if (dx >= 0 && dx < VGA_W && y + row >= 0 && y + row < VGA_H)
                vga->framebuffer[(y + row) * VGA_STRIDE + dx] =
                    image->pixels[(size_t)row * (size_t)image->width + (size_t)column];
        }
}

static int progress_box_transition(field_progress_box *box, fd2_vga *vga,
                                     int opening, int fast) {
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    for (int phase = opening ? 5 : 1; opening ? phase >= 0 : phase < 6;
         phase += opening ? -1 : 1) {
        int y = 112 + phase * 13;
        memcpy(vga->framebuffer, box->background, VGA_W * VGA_H);
        for (int row = 0; row < 86 && y + row < VGA_H; row++)
            memcpy(vga->framebuffer + (y + row) * VGA_STRIDE + 5,
                box->page->framebuffer + (112 + row) * VGA_STRIDE + 5, 310);
        /* 1956b/196cb→1974c：开6/关5次memmove，无delay或BIOS门。 */
        if (!fast && play_present_copy(vga)<0)return FD2_SCENE_RESULT_ERROR;
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    }
    return FD2_SCENE_RESULT_OK;
}

/* field_bottom_notice_close @0x196cb（原sub_196CB）尾部先恢复原VGA，
 * 再调用11cac(0)；不能只恢复截图或用pure render代替live刷新。 */
static int progress_box_close(field_progress_box *box,fd2_field_game *game,
        fd2_vga *vga,int fast) {
    int result=play_field_terminal_result(game,vga);
    if(result!=FD2_SCENE_RESULT_OK)return result;
    result=progress_box_transition(box,vga,0,fast);
    if(result!=FD2_SCENE_RESULT_OK)return result;
    memcpy(vga->framebuffer,box->background,VGA_W*VGA_H);
    if(!fast)result=play_present_copy(vga);
    /* 1971c/1972a/19738的buffer释放先于19742的11cac(0)。
     * portraits是此C box借用解码的对应临时资源，也在同一owner收口。 */
    progress_box_free(box);
    if(result<0)return FD2_SCENE_RESULT_ERROR;
    result=play_field_terminal_result(game,vga);
    if(result!=FD2_SCENE_RESULT_OK)return result;
    if(fd2_field_game_runtime_refresh(game,vga,0)!=0)return FD2_SCENE_RESULT_ERROR;
    return play_field_terminal_result(game,vga);
}

static int progress_box_open_portrait(field_progress_box *box, fd2_field_game *game,
                              fd2_vga *vga, const fd2_archive *fdother,
                              unsigned id, int fast) {
    if (!box || box->page) return -1;
    box->portrait_origin = (int)fd2_portrait_notice_origin(id);
    box->row = 2;
    box->background = malloc(VGA_W * VGA_H);
    box->page = calloc(1, sizeof(*box->page));
    if (!box->background || !box->page) goto fail;
    memcpy(box->background, vga->framebuffer, VGA_W * VGA_H);
    memcpy(box->page->framebuffer, box->background, VGA_W * VGA_H);
    for (size_t i = 0; i < 4; i++)
        if (fd2_portrait_decode_frame(&box->portraits[i], &game->dato_archive, id, i) != 0 ||
            box->portraits[i].width != 80 || box->portraits[i].height != 80) goto fail;
    if (fd2_scene_field_message_box(box->page, fdother) != 0) goto fail;
    progress_portrait(box, box->page, 0, 1);
    return progress_box_transition(box, vga, 1, fast);
fail:
    progress_box_free(box);
    return FD2_SCENE_RESULT_ERROR;
}

static int progress_box_open(field_progress_box *box, fd2_field_game *game,
        fd2_vga *vga,const fd2_archive *fdother,size_t actor_index,int fast) {
    if (actor_index>=game->units.count) return -1;
    return progress_box_open_portrait(box,game,vga,fdother,
        game->units.items[actor_index].unit_id,fast);
}

static int progress_wait(field_progress_box *box, fd2_field_game *game,
                          fd2_vga *vga, const fd2_archive *fdother, int fast,
                          int arrow_mode) {
    int terminal=play_field_terminal_result(game,vga);
    if(terminal!=FD2_SCENE_RESULT_OK)return terminal;
    if (!game->attack_rng) return FD2_SCENE_RESULT_ERROR;
    int32_t last;if(play_clock_tick(vga,&last)<0){return FD2_SCENE_RESULT_ERROR;}
    uint32_t counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
    if (fast) return FD2_SCENE_RESULT_OK;
    unsigned arrow = 18, arrow_ticks = 0;
    int blinking = 0;
    uint64_t host_deadline=0;
    int result=FD2_SCENE_RESULT_OK;
    /* dialog_wait_key @0x16c57（原登记同名）：仅实参==1画18/19并在返回时擦13。
     * 190ac/1aa1d的外部等键均传0，仍保留相同blink/RNG与water等待。 */
    if (arrow_mode == 1 && fd2_scene_field_message_arrow(vga, fdother, arrow) != 0) return -1;
    for (;;) {
        if (fd2_input_take_quit(&vga->input)) {result=FD2_SCENE_RESULT_HOST_QUIT;break;}
        if (fd2_input_take_key(&vga->input, NULL)) break;
        if(fd2_field_water_tick_live(vga)<0){result=FD2_SCENE_RESULT_ERROR;break;}
        int32_t tick;if(play_clock_tick(vga,&tick)<0){result=FD2_SCENE_RESULT_ERROR;break;}
        if (tick - last >= 2) {
            if (arrow_mode == 1 && ++arrow_ticks == 3) {
                arrow_ticks = 0;
                arrow = arrow == 18 ? 19 : 18;
                if (fd2_scene_field_message_arrow(vga, fdother, arrow) != 0) {result=-1;break;}
            }
            if (blinking) {
                progress_portrait(box, vga, 0, 0);
                counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
                blinking = 0;
            } else if (counter-- == 0) {
                progress_portrait(box, vga, 3, 0);
                blinking = 1;
            }
            if(play_clock_tick(vga,&last)<0){result=FD2_SCENE_RESULT_ERROR;break;}
        }
        if(modal_host_present(vga,&host_deadline)<0){result=FD2_SCENE_RESULT_ERROR;break;}
    }
    modal_host_release(vga,&host_deadline);
    if(result!=FD2_SCENE_RESULT_OK)return result;
    return arrow_mode == 1 ? fd2_scene_field_message_arrow(vga, fdother, 13) : FD2_SCENE_RESULT_OK;
}

static int progress_scroll(fd2_vga *vga, unsigned origin) {
    return fd2_dialog_scroll_pixels(vga->framebuffer,sizeof(vga->framebuffer),origin);
}

static int field_text_play(field_progress_box *box, fd2_field_game *game,
                               fd2_vga *vga, const fd2_archive *fdother,
                               fd2_field_audio *audio,
                               const field_progress_text *text, int x, int y, int fast) {
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return FD2_SCENE_RESULT_ERROR;
    int start_x = x;
    int glyph_step = !fast;
    for (size_t i = 0; i < text->count; i++) {
        uint16_t token = text->tokens[i];
        if (token == 0xfffeu || token == 0xfffdu) {
            x = start_x;
            y += 19;
            if (token == 0xfffdu) {
                progress_portrait(box, vga, 0, 0);
                int waited = progress_wait(box, game, vga, fdother, fast, 1);
                if (waited != FD2_SCENE_RESULT_OK) return waited;
                glyph_step = !fast;
            }
            continue;
        }
        if (token==FIELD_TEXT_SAFE_MINUS) {
            const fd2_image *minus=&game->info_assets.signs[1];
            /* 负-6原font读已越源owner；保先清74的定义前缀，安全符号仍H。 */
            if(fd2_font_draw_glyph_linear(vga->framebuffer,sizeof(vga->framebuffer),&game->font,
                    10u,(int64_t)y*VGA_STRIDE+x,VGA_STRIDE,0xcd,0x4c,0x4a)!=0)
                return FD2_SCENE_RESULT_ERROR;
            fd2_map_sprite_blit(vga,minus,x+(16-minus->width)/2,y+(16-minus->height)/2,0);
        } else if(fd2_font_draw_glyph_linear(vga->framebuffer,sizeof(vga->framebuffer),&game->font,
                token,(int64_t)y*VGA_STRIDE+x,VGA_STRIDE,0xcd,0x4c,0x4a)!=0)
            return FD2_SCENE_RESULT_ERROR;
        x += 16;
        if (!fast && play_present_copy(vga)<0)return FD2_SCENE_RESULT_ERROR;
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
        if (fd2_input_has_any_key(&vga->input)) glyph_step = 0;
        if (glyph_step) {
            unsigned phase;
            int redraw=fd2_dialog_visual_glyph_step(&vga->dialog_visual,&phase);
            if(redraw<0)return FD2_SCENE_RESULT_ERROR;
            if(redraw) {
                progress_portrait(box,vga,phase,0);
                fd2_vga_upload_current_frame(vga);
                if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return FD2_SCENE_RESULT_ERROR;
            }
            if (audio) {(void)fd2_field_audio_play(audio, FD2_FIELD_SFX_DIALOG_GLYPH);if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)){return FD2_SCENE_RESULT_ERROR;}}
            if(fd2_vga_wait_ticks(vga,1)<0)return FD2_SCENE_RESULT_ERROR;
        }
    }
    return FD2_SCENE_RESULT_OK;
}

static int progress_text_play(field_progress_box *box, fd2_field_game *game,
                               fd2_vga *vga, const fd2_archive *fdother,
                               fd2_field_audio *audio,
                               const fd2_field_progress_notice *notice, int fast) {
    field_progress_text text;
    if (progress_text_prepare(game, notice, &text) != 0) return -1;
    int x = notice->kind == FD2_FIELD_PROGRESS_STATUS ? 99 : 95;
    int y = notice->kind == FD2_FIELD_PROGRESS_STATUS ? 127 :
        notice->kind == FD2_FIELD_PROGRESS_GAIN ? 119 :
        notice->kind == FD2_FIELD_PROGRESS_LEVEL ? 138 : 119 + 19 * box->row;
    return field_text_play(box, game, vga, fdother, audio, &text, x, y, fast);
}

/* field_notice_wait_ticks @0x1e5c0（原sub_1E5C0）：先水色，再查键及两次
 * signed WORD BIOS tick；不推进127e0/raw26，不在入口清键。STATUS的clear
 * 属1a866 caller，CLOSE的1e292保已有键；fast仅诊断。 */
static int progress_timed_wait(fd2_vga *vga, unsigned ticks, int fast) {
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (fast) {
        fd2_input_clear_keys(&vga->input);
        return fd2_input_take_quit(&vga->input)?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_OK;
    }
    int32_t start;if(play_clock_tick(vga,&start)<0){return FD2_SCENE_RESULT_ERROR;}
    uint64_t host_deadline=0;
    int result=FD2_SCENE_RESULT_OK;
    for (;;) {
        if (fd2_input_take_quit(&vga->input)) {result=FD2_SCENE_RESULT_HOST_QUIT;break;}
        if (fd2_field_water_tick_live(vga)<0) {result=FD2_SCENE_RESULT_ERROR;break;}
        int done=fd2_input_has_any_key(&vga->input);
        int32_t now;if(play_clock_tick(vga,&now)<0){result=FD2_SCENE_RESULT_ERROR;break;}
        if ((int64_t)now-start>=(int64_t)ticks) done=1;
        else {int32_t negative_tick;if(play_clock_tick(vga,&negative_tick)<0){result=FD2_SCENE_RESULT_ERROR;break;}if(negative_tick<start)done=1;}
        if (done) break;
        if(modal_host_present(vga,&host_deadline)<0){result=FD2_SCENE_RESULT_ERROR;break;}
    }
    modal_host_release(vga,&host_deadline);
    if(result!=FD2_SCENE_RESULT_OK)return result;
    fd2_input_clear_keys(&vga->input);
    return fd2_input_take_quit(&vga->input)?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_OK;
}

typedef enum {
    FIELD_LIST_ERROR = -1,
    FIELD_LIST_CANCELLED = 0,
    FIELD_LIST_CONFIRMED = 1,
    FIELD_LIST_HOST_QUIT = 2
} field_list_result;

typedef enum { FIELD_LIST_MAGIC, FIELD_LIST_ITEM, FIELD_LIST_GIVE } field_list_kind;

static fd2_field_magic_list_key magic_list_key(fd2_input_key key) {
    switch (key) {
        case FD2_INPUT_KEY_UP: return FD2_FIELD_MAGIC_LIST_KEY_UP;
        case FD2_INPUT_KEY_DOWN: return FD2_FIELD_MAGIC_LIST_KEY_DOWN;
        case FD2_INPUT_KEY_LEFT: return FD2_FIELD_MAGIC_LIST_KEY_LEFT;
        case FD2_INPUT_KEY_RIGHT: return FD2_FIELD_MAGIC_LIST_KEY_RIGHT;
        case FD2_INPUT_KEY_ENTER:
        case FD2_INPUT_KEY_SPACE:
        case FD2_INPUT_KEY_KEYPAD_CONFIRM:
            return FD2_FIELD_MAGIC_LIST_KEY_CONFIRM;
        case FD2_INPUT_KEY_ESCAPE:
        case FD2_INPUT_KEY_CANCEL: return FD2_FIELD_MAGIC_LIST_KEY_CANCEL;
        default: return FD2_FIELD_MAGIC_LIST_KEY_OTHER;
    }
}

static fd2_field_item_list_key item_list_key(fd2_input_key key) {
    switch (key) {
        case FD2_INPUT_KEY_UP: return FD2_FIELD_ITEM_LIST_KEY_UP;
        case FD2_INPUT_KEY_DOWN: return FD2_FIELD_ITEM_LIST_KEY_DOWN;
        case FD2_INPUT_KEY_LEFT: return FD2_FIELD_ITEM_LIST_KEY_LEFT;
        case FD2_INPUT_KEY_RIGHT: return FD2_FIELD_ITEM_LIST_KEY_RIGHT;
        case FD2_INPUT_KEY_ENTER:
        case FD2_INPUT_KEY_SPACE:
        case FD2_INPUT_KEY_KEYPAD_CONFIRM:
            return FD2_FIELD_ITEM_LIST_KEY_CONFIRM;
        case FD2_INPUT_KEY_ESCAPE:
        case FD2_INPUT_KEY_CANCEL: return FD2_FIELD_ITEM_LIST_KEY_CANCEL;
        default: return FD2_FIELD_ITEM_LIST_KEY_OTHER;
    }
}

static int field_list_selected(const fd2_field_game *game, field_list_kind kind) {
    if (kind == FIELD_LIST_GIVE) return game->item_give_selected;
    return kind == FIELD_LIST_MAGIC ? game->magic_list_selected : game->item_list_selected;
}

static int field_list_step(fd2_field_game *game, field_list_kind kind,
                            fd2_input_key key, uint8_t *confirmed) {
    if (kind == FIELD_LIST_MAGIC)
        return fd2_field_game_magic_list_step(game, magic_list_key(key), confirmed);
    int result = kind == FIELD_LIST_GIVE
        ? fd2_field_game_item_give_list_step(game, item_list_key(key))
        : fd2_field_game_item_list_step(game, item_list_key(key));
    if (result == 1) *confirmed = (uint8_t)field_list_selected(game, kind);
    return result;
}

static int field_list_draw(fd2_vga *vga, const fd2_field_game *game,
                            const fd2_field_unit *unit,
                            field_list_kind kind, int selected) {
    if (kind == FIELD_LIST_MAGIC)
        return fd2_field_magic_list_draw(vga, &game->info_assets, &game->font,
                                         &game->ui_text, unit, selected);
    return fd2_field_item_list_draw(vga, &game->info_assets, &game->font,
                                    &game->ui_text, unit, selected);
}

/* 详情底页与库存列表共用的DATO 80×80立绘：首帧0与眨眼帧3。 */
static int list_portraits_open(const fd2_field_game *game, size_t unit_id,
        fd2_image portraits[2]) {
    for (unsigned i = 0; i < 2; i++)
        if (fd2_portrait_decode_frame(&portraits[i], &game->dato_archive,
                unit_id, i ? 3 : 0) != 0 || !portraits[i].pixels ||
            portraits[i].width != 80 || portraits[i].height != 80) return -1;
    return 0;
}

/* sub_16C57 @0x16c57的列表等键：只推进water/(8,10)立绘眨眼/共享RNG，不画
 * 127e0。入口与每次眨眼复位各取一次RNG；signed WORD差>=2才推进一次。
 * 取到键返回OK（事件写入*event），宿主退出返回HOST_QUIT，其余ERROR。 */
static int list_wait_key(fd2_field_game *game, fd2_vga *vga,
        const fd2_image portraits[2], fd2_input_event *event, uint64_t *host_deadline) {
    int32_t last;
    if (play_clock_tick(vga, &last) < 0) return FD2_SCENE_RESULT_ERROR;
    uint32_t counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
    int blinking = 0;
    for (;;) {
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
        if (fd2_input_take_key(&vga->input, event)) {
            modal_host_release(vga, host_deadline);
            return FD2_SCENE_RESULT_OK;
        }
        if (fd2_field_water_tick_live(vga) < 0) return FD2_SCENE_RESULT_ERROR;
        int32_t tick;
        if (play_clock_tick(vga, &tick) < 0) return FD2_SCENE_RESULT_ERROR;
        if (tick - last >= 2) {
            if (blinking) {
                fd2_map_sprite_blit(vga, &portraits[0], 8, 10, -1);
                counter = game->attack_rng(game->attack_rng_userdata) % 30u + 2u;
                blinking = 0;
            } else if (counter-- == 0u) {
                fd2_map_sprite_blit(vga, &portraits[1], 8, 10, -1);
                blinking = 1;
            }
            if (play_clock_tick(vga, &last) < 0) return FD2_SCENE_RESULT_ERROR;
        }
        if (modal_host_present(vga, host_deadline) < 0) return FD2_SCENE_RESULT_ERROR;
    }
}

/* 17e0b/17eef的12帧详情底页开合；sfx_audio非NULL时按原phase播放开/关音效。 */
static int detail_page_transition(fd2_vga *vga, const fd2_vga *page,
        const uint8_t *background, int opening, fd2_field_audio *sfx_audio) {
    for (int step = 0; step <= 11; step++) {
        int phase = opening ? 11 - step : step;
        fd2_field_detail_transition_frame(vga->framebuffer,
            page->framebuffer, background, phase);
        if (sfx_audio && fd2_field_detail_sfx_for_phase(opening, phase) >= 0) {
            (void)fd2_field_audio_play(sfx_audio, opening
                ? FD2_FIELD_SFX_DETAIL_OPEN : FD2_FIELD_SFX_DETAIL_CLOSE);
            if (fd2_bios_clock_error() < 0 || fd2_vga_output_error(vga) < 0)
                return FD2_SCENE_RESULT_ERROR;
        }
        if (play_present_copy(vga) < 0) return FD2_SCENE_RESULT_ERROR;
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    }
    return FD2_SCENE_RESULT_OK;
}

static field_list_result list_result_from_scene(int scene_result) {
    return scene_result == FD2_SCENE_RESULT_HOST_QUIT ? FIELD_LIST_HOST_QUIT : FIELD_LIST_ERROR;
}

/* field_player_magic_command_execute @0x1cff0（原登记同名）与
 * field_unit_inventory_select @0x1b932（原sub_1B932）的列表呈现／等待；
 * EQT留页依据field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）。
 * 按键分别复用@0x1d51d（原sub_1D51D）与@0x1b9de（原sub_1B9DE）。
 * 不结束LIST、不扣资源、不提交临时移动或pending。
 * sub_16C57 @0x16c57每次等键入口和眨眼复位均消费session共享RNG；
 * 16ms只为SDL呈现；signed WORD差>=2才推进一次，不追赶。 */
static field_list_result play_field_unit_list(
    fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
    field_list_kind kind, uint8_t *confirmed) {
    if (!vga) return FIELD_LIST_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FIELD_LIST_HOST_QUIT;
    if (!game || !game->ready || !confirmed ||
        game->failure != FD2_FIELD_FAILURE_NONE || !game->attack_rng ||
        game->interaction != (kind == FIELD_LIST_MAGIC ? FD2_FIELD_INTERACTION_MAGIC_LIST :
            kind == FIELD_LIST_GIVE ? FD2_FIELD_INTERACTION_ITEM_GIVE_LIST : FD2_FIELD_INTERACTION_ITEM_LIST) ||
        game->detail_visible || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->units.count > FD2_FIELD_MAX_UNITS)
        return FIELD_LIST_ERROR;
    if (kind == FIELD_LIST_ITEM &&
        !(game->command_selected == 0 ? fd2_field_game_item_use_is_available(game) :
          game->command_selected == 1 ? fd2_field_game_item_give_is_available(game) :
          game->command_selected == 2 ? fd2_field_game_item_equip_is_available(game) :
          game->command_selected == 3 && fd2_field_game_item_drop_is_available(game)))
        return FIELD_LIST_ERROR;
    if (kind == FIELD_LIST_GIVE && !fd2_field_game_item_give_is_available(game))
        return FIELD_LIST_ERROR;
    uint8_t candidate_id = 0;
    /* 统一复用core的actor／移动／完整record门，OTHER不改索引或ID。 */
    if (field_list_step(game, kind, FD2_INPUT_KEY_OTHER, &candidate_id) != 0)
        return FIELD_LIST_ERROR;
    int unit_index = kind == FIELD_LIST_GIVE ? game->item_give_recipient : game->selected_unit;
    if (unit_index < 0 || (size_t)unit_index >= game->units.count) return FIELD_LIST_ERROR;
    const fd2_field_unit *unit = &game->units.items[unit_index];
    size_t count = kind == FIELD_LIST_MAGIC
        ? fd2_field_unit_magic_list_build(unit, NULL, 0)
        : fd2_field_unit_inventory_count(unit);
    size_t maximum = kind == FIELD_LIST_MAGIC ? FD2_FIELD_MAGIC_LIST_MAX_COUNT : 8u;
    if (count == 0 || count > maximum || field_list_selected(game, kind) < 0 ||
        (size_t)field_list_selected(game, kind) >= count ||
        !fd2_field_game_detail_upper_is_ready(game, unit))
        return FIELD_LIST_ERROR;

    fd2_image portraits[2] = {{0}, {0}};
    uint8_t *background = NULL;
    fd2_vga *page = NULL;
    field_list_result result = FIELD_LIST_ERROR;
    int initial_info_panel_right = game->info_panel_right;
    int entered = 0;
    uint64_t host_deadline=0;
    if (list_portraits_open(game, unit->unit_id, portraits) != 0) goto done;
    background = malloc(VGA_W * VGA_H);
    if (!background) goto done;
    page = malloc(sizeof(*page));
    if (!page) goto done;
    /* renderer只需一个离屏VGA目标；不复制宿主input／palette的owner。 */
    memset(page, 0, sizeof(*page));
    page->actor_jitter_tick=vga->actor_jitter_tick;
    page->actor_jitter_phase=vga->actor_jitter_phase;
    fd2_field_render_scope visual_scope;
    if (fd2_field_render_scope_begin(&visual_scope, page, vga, 1) != 0) goto done;
    fd2_field_game_render_preview(game, page);
    fd2_field_render_scope_end(&visual_scope);
    memcpy(background, page->framebuffer, VGA_W * VGA_H);
    if (fd2_field_detail_draw(page, &game->info_assets, &game->font,
                          &game->ui_text, unit, &portraits[0]) != 0) goto done;
    if (kind == FIELD_LIST_ITEM && game->command_selected == 2) {
        /* 1c142整flag清0会唤醒hidden equipped；首帧前预检全部潜在名称。
         * 只用80B临时记录，不预演装备或更改真实库存/信息面板owner。 */
        fd2_field_unit visible = *unit;
        uint8_t *raw = (uint8_t *)&visible;
        for (size_t slot = 0; slot < 8u; slot++)
            if ((raw[0x0a + slot * 2u] & 0xc0u) == 0xc0u)
                raw[0x0a + slot * 2u] = 0;
        if (field_list_draw(page, game, &visible, kind, -1) != 0)
            goto done;
    }
    if (field_list_draw(page, game, unit, kind, -1) != 0)
        goto done;
    /* 完整资源预检结束后才写实际VGA、耗键或消费RNG。 */
    if (fd2_input_take_quit(&vga->input)) {
        result = FIELD_LIST_HOST_QUIT;
        goto done;
    }
    entered = 1;
    int open_result = detail_page_transition(vga, page, background, 1, NULL);
    if (open_result != FD2_SCENE_RESULT_OK) {
        result = list_result_from_scene(open_result);
        goto done;
    }
    /* 17e0b打开末尾清队；1cff0 MAGIC手工opening无clear。 */
    if(kind!=FIELD_LIST_MAGIC)fd2_input_clear_keys(&vga->input);
    for (;;) {
        if (fd2_input_take_quit(&vga->input)) {
            result = FIELD_LIST_HOST_QUIT;
            goto done;
        }
        if (field_list_draw(vga, game, unit, kind, field_list_selected(game, kind)) != 0)
            goto done;
        /* 16c57的counter/flag/tick每次调用重置；立绘本身跨键保留。
         * 1d51d/1b9de→16c57：等待仅water/portrait/RNG，不画127e0。 */
        fd2_input_event event;
        int waited = list_wait_key(game, vga, portraits, &event, &host_deadline);
        if (waited != FD2_SCENE_RESULT_OK) {
            result = list_result_from_scene(waited);
            goto done;
        }
        if (fd2_input_take_quit(&vga->input)) {
            result = FIELD_LIST_HOST_QUIT;
            goto done;
        }
        int previous = field_list_selected(game, kind);
        int step = field_list_step(game, kind, event.key, &candidate_id);
        if (step == -2) goto done;
        if (step == 1 && kind == FIELD_LIST_ITEM && game->command_selected == 2) {
            /* field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）确认留同页；保后续独立键，退出先于不可逆装备。 */
            if (fd2_input_take_quit(&vga->input)) {
                result = FIELD_LIST_HOST_QUIT;
                goto done;
            }
            if (!fd2_field_game_item_equip_is_available(game)) goto done;
            int applied = fd2_field_game_equip_item(game);
            if (applied < 0) goto done;
            if (applied) {
                if (fd2_input_take_quit(&vga->input)) {
                    result = FIELD_LIST_HOST_QUIT;
                    goto done;
                }
                if (!fd2_field_game_detail_upper_is_ready(game, unit)) goto done;
                memcpy(page->framebuffer, vga->framebuffer, VGA_W * VGA_H);
                /* 1bffe→17eef重画DATO首帧0及upper，再184c0(-1)重画底页。
                 * 同时更新关闭动画的page，不能闪回换装前的flag/AP。 */
                if (fd2_field_detail_draw(page, &game->info_assets, &game->font,
                                      &game->ui_text, unit, &portraits[0]) != 0) goto done;
                if (field_list_draw(page, game, unit, kind, -1) != 0)
                    goto done;
                memcpy(vga->framebuffer, page->framebuffer, VGA_W * VGA_H);
                if(play_present_copy(vga)<0){result=FIELD_LIST_ERROR;goto done;}
                if (fd2_input_take_quit(&vga->input)) {
                    result = FIELD_LIST_HOST_QUIT;
                    goto done;
                }
            }
            continue;
        }
        if (step != 0) {
            result = step == 1 ? FIELD_LIST_CONFIRMED : FIELD_LIST_CANCELLED;
            break;
        }
        /* native上下即使单项回绕也发声；左右只在实际跨列时发声。 */
        if (audio && game->option_values[1] &&
            (event.key == FD2_INPUT_KEY_UP || event.key == FD2_INPUT_KEY_DOWN ||
             previous != field_list_selected(game, kind)))
            {(void)fd2_field_audio_play(audio, FD2_FIELD_SFX_COMMAND_MENU);if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)){goto done;}}
    }
    int close_result = detail_page_transition(vga, page, background, 0, NULL);
    if (close_result != FD2_SCENE_RESULT_OK) {
        result = list_result_from_scene(close_result);
        goto done;
    }
    memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    if(play_present_copy(vga)<0){result=FIELD_LIST_ERROR;goto done;}
    if (fd2_input_take_quit(&vga->input)) {
        result = FIELD_LIST_HOST_QUIT;
        goto done;
    }
    if (result == FIELD_LIST_CONFIRMED) *confirmed = candidate_id;
done:
    modal_host_release(vga,&host_deadline);
    /* 离屏render会选信息面板侧位；未开始呈现的失败不能提交该UI owner。 */
    if (!entered && (result == FIELD_LIST_ERROR || result == FIELD_LIST_HOST_QUIT))
        game->info_panel_right = initial_info_panel_right;
    free(page);
    free(background);
    fd2_image_free(&portraits[1]);
    fd2_image_free(&portraits[0]);
    return result;
}

static field_list_result play_field_magic_list(
    fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
    uint8_t *confirmed_id) {
    return play_field_unit_list(game, vga, audio, FIELD_LIST_MAGIC, confirmed_id);
}

/* 物品列表仅在完整关页后返回raw槽；四种子操作由command_selected区分。 */
static field_list_result play_field_item_list(
    fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
    uint8_t *confirmed_raw_slot) {
    return play_field_unit_list(game, vga, audio, FIELD_LIST_ITEM, confirmed_raw_slot);
}

static field_list_result play_field_item_give_list(fd2_field_game *game,
        fd2_vga *vga, fd2_field_audio *audio, uint8_t *confirmed_raw_slot) {
    return play_field_unit_list(game, vga, audio, FIELD_LIST_GIVE, confirmed_raw_slot);
}

/* 18d8c的1cff0重入：列表Esc回COMMAND，目标Esc由调用者先复位为LIST。
 * 只编排呈现和目标状态，不在这里提交费用或效果。 */
static int play_field_magic_selection(fd2_field_game *game, fd2_vga *vga,
                                       fd2_field_audio *audio) {
    uint8_t magic_id = 0;
    field_list_result result = play_field_magic_list(game, vga, audio, &magic_id);
    if (result == FIELD_LIST_HOST_QUIT) return FD2_SCENE_RESULT_HOST_QUIT;
    if (result == FIELD_LIST_ERROR) return FD2_SCENE_RESULT_ERROR;
    if (result == FIELD_LIST_CONFIRMED)
        return fd2_field_game_begin_magic_targeting(game, magic_id) == 0
            ? FD2_SCENE_RESULT_OK : FD2_SCENE_RESULT_ERROR;
    if (fd2_field_game_end_magic_list(game) != 0) return FD2_SCENE_RESULT_ERROR;
    return play_command_transition(game,vga,audio,1);
}

typedef int (*menu_animate_fn)(fd2_field_game *game, fd2_vga *vga,
                               int opening, uint32_t frame_delay_ms);

/* 指令菜单与系统菜单共用1741c开／176b4关：音效、动画、失败与退出门相同；
 * 成功关闭后才清键（176b4尾；1741c不清）。 */
static int play_menu_transition(fd2_field_game *game, fd2_vga *vga,
        fd2_field_audio *audio, int opening, menu_animate_fn animate) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if (game->failure != FD2_FIELD_FAILURE_NONE) return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (audio && game->option_values[1])
        (void)fd2_field_audio_play(audio, FD2_FIELD_SFX_COMMAND_MENU);
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    int result = animate(game, vga, opening, FD2_FIELD_COMMAND_FRAME_MS);
    if (result < 0 || game->failure != FD2_FIELD_FAILURE_NONE)
        return FD2_SCENE_RESULT_ERROR;
    if(result>0 || game->runtime_host_quit || fd2_input_take_quit(&vga->input))return FD2_SCENE_RESULT_HOST_QUIT;
    if(!opening)fd2_input_clear_keys(&vga->input);
    return fd2_input_take_quit(&vga->input)?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_OK;
}

static int play_command_transition(fd2_field_game *game, fd2_vga *vga,
                                    fd2_field_audio *audio, int opening) {
    return play_menu_transition(game, vga, audio, opening,
                                fd2_field_game_animate_command);
}

/* 16f55/1728c/19df7复用同一1741c/176b4。 */
static int play_system_menu_transition(fd2_field_game *game,fd2_vga *vga,
        fd2_field_audio *audio,int opening) {
    return play_menu_transition(game, vga, audio, opening,
                                fd2_field_game_animate_system_menu);
}

/* 1bbdc四种子操作共用列表；完整关页才提交raw槽，按当前owner重开菜单。 */
static int play_field_item_selection(fd2_field_game *game, fd2_vga *vga,
                                      fd2_field_audio *audio) {
    uint8_t slot = 0xff;
    field_list_result result = play_field_item_list(game, vga, audio, &slot);
    if (result == FIELD_LIST_HOST_QUIT) return FD2_SCENE_RESULT_HOST_QUIT;
    if (result == FIELD_LIST_ERROR) return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (result == FIELD_LIST_CONFIRMED) {
        if (slot != game->item_list_selected) return FD2_SCENE_RESULT_ERROR;
        if (game->command_selected == 0)
            return fd2_field_game_item_use_is_available(game) &&
                fd2_field_game_begin_item_targeting(game) == 0
                    ? FD2_SCENE_RESULT_OK : FD2_SCENE_RESULT_ERROR;
        if (game->command_selected == 1)
            return fd2_field_game_item_give_is_available(game) &&
                fd2_field_game_begin_item_give(game) == 0 ? FD2_SCENE_RESULT_OK : FD2_SCENE_RESULT_ERROR;
        if (game->command_selected != 3 || !fd2_field_game_item_drop_is_available(game) ||
            fd2_field_game_resolve_item_drop(game) != 0)
            return FD2_SCENE_RESULT_ERROR;
    } else if (fd2_field_game_end_item_list(game) != 0) {
        return FD2_SCENE_RESULT_ERROR;
    }
    return play_command_transition(game, vga, audio, 1);
}

static int play_field_item_give_selection(fd2_field_game *game, fd2_vga *vga,
                                         fd2_field_audio *audio) {
    uint8_t slot = 0xff;
    field_list_result result = play_field_item_give_list(game, vga, audio, &slot);
    if (result == FIELD_LIST_ERROR) return FD2_SCENE_RESULT_ERROR;
    if (result == FIELD_LIST_HOST_QUIT || fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (result == FIELD_LIST_CONFIRMED) {
        if (slot != game->item_give_selected || !fd2_field_game_item_give_is_available(game) ||
            fd2_field_game_resolve_item_give(game) != 0) return FD2_SCENE_RESULT_ERROR;
    } else if (fd2_field_game_cancel_item_give(game) != 0) return FD2_SCENE_RESULT_ERROR;
    return play_command_transition(game, vga, audio, 1);
}

/* 18d8c在1bbdc返回-1后重开COMMAND，仍不回滚临时移动。 */
static int play_field_item_menu_cancel(fd2_field_game *game, fd2_vga *vga,
                                        fd2_field_audio *audio) {
    int result = play_command_transition(game, vga, audio, 0);
    if (result != FD2_SCENE_RESULT_OK) return result;
    if (fd2_field_game_end_item_menu(game) != 0) return FD2_SCENE_RESULT_ERROR;
    return play_command_transition(game, vga, audio, 1);
}

static fd2_input_context field_input_context(const fd2_field_game *game) {
    if (game->detail_visible) return FD2_INPUT_CONTEXT_FIELD_DETAIL;
    switch (game->interaction) {
        case FD2_FIELD_INTERACTION_UNIT_SELECTED:
        case FD2_FIELD_INTERACTION_MOVING: return FD2_INPUT_CONTEXT_FIELD_MOVEMENT;
        case FD2_FIELD_INTERACTION_COMMAND:
        case FD2_FIELD_INTERACTION_ITEM_MENU: return FD2_INPUT_CONTEXT_FIELD_COMMAND;
        case FD2_FIELD_INTERACTION_MAGIC_TARGETING:
        case FD2_FIELD_INTERACTION_MAGIC_TELEPORTING:
        case FD2_FIELD_INTERACTION_ITEM_TARGETING:
        case FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING:
        case FD2_FIELD_INTERACTION_ITEM_TELEPORTING:
            /* 两种raw候选轮询共用既有方向／取消／Z／小键盘5映射。 */
            return FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING;
        case FD2_FIELD_INTERACTION_TARGETING: return FD2_INPUT_CONTEXT_FIELD_TARGETING;
        case FD2_FIELD_INTERACTION_SYSTEM_MENU: return FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU;
        case FD2_FIELD_INTERACTION_AUXILIARY: return FD2_INPUT_CONTEXT_FIELD_AUXILIARY;
        default: return FD2_INPUT_CONTEXT_FIELD;
    }
}

static const char *field_action_command_name(fd2_field_interaction previous, int command) {
    /* ITEM子菜单的Use索引0与顶层ATTACK不同。 */
    if (previous == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
        previous == FD2_FIELD_INTERACTION_ITEM_TELEPORTING) return "item";
    return command == FD2_FIELD_COMMAND_ATTACK ? "attack" :
        command == FD2_FIELD_COMMAND_MAGIC ? "magic" :
        command == FD2_FIELD_COMMAND_ITEM ? "item" : "wait";
}

static void overview_copy_rect(uint8_t *dst, const uint8_t *src,
                                int x, int y, int src_x, int src_y,
                                int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (x < 0) { src_x -= x; width += x; x = 0; }
    if (y < 0) { src_y -= y; height += y; y = 0; }
    if (x + width > VGA_W) width = VGA_W - x;
    if (y + height > VGA_H) height = VGA_H - y;
    if (width <= 0 || height <= 0) return;
    for (int row = 0; row < height; row++)
        memcpy(dst + (size_t)(y + row) * VGA_STRIDE + x,
               src + (size_t)(src_y + row) * VGA_STRIDE + src_x,
               (size_t)width);
}

/* sub_1B1E7 @0x1b1e7（原登记 sub_1B1E7）：四块12相位开合。
 * 主体分别用 sub_1AF99 @0x1af99／sub_1AF1E @0x1af1e 的右入／左出；
 * 其余三块依次复现 sub_1B019 @0x1b019、sub_1B0AD @0x1b0ad、
 * sub_1B14B @0x1b14b，不借用角色详情或关卡转场动画。 */
static void overview_transition_frame(fd2_vga *vga, const uint8_t *overview,
                                        int phase, int opening) {
    int x = 75;
    if (phase < 5)
        x += (opening ? 50 : -50) * (4 - phase);
    overview_copy_rect(vga->framebuffer, overview,
                        x, 37, 75, 37, 170, 117);
    if (phase >= 3) {
        int y = phase <= 7 ? 19 - 6 * (7 - phase) : 19;
        overview_copy_rect(vga->framebuffer, overview,
                            109, y, 109, 19, 102, 17);
    }
    if (phase >= 5) {
        int offset = phase <= 9 ? 9 * (9 - phase) : 0;
        /* 原版 phase5 的 height=36-45=-9，signed 循环复制0行。 */
        int height = offset + 171 > 200 ? offset - 45 : 16;
        overview_copy_rect(vga->framebuffer, overview,
                            75, 155 + offset, 75, 155, 170, height);
    }
    if (phase >= 8) {
        int offset = 4 * (12 - phase);
        /* @0x1b14b：phase8 offset=16；16+187>200，height=16-28=-12，
         * 因此不绘；不能把 y188 通用裁剪为12行而改变原版外观。 */
        int height = offset + 187 > 200 ? offset - 28 : 15;
        overview_copy_rect(vga->framebuffer, overview,
                            129, 172 + offset, 129, 172, 63, height);
    }
}

/* 1b1e7的12帧开(0..11)／关(11..0)：每帧先恢复干净背景再叠四块。 */
static int overview_transition(fd2_vga *vga, const uint8_t *background,
        const uint8_t *overview, int opening) {
    for (int step = 0; step < 12; step++) {
        memcpy(vga->framebuffer, background, VGA_W * VGA_H);
        overview_transition_frame(vga, overview, opening ? step : 11 - step, opening);
        if (play_present_copy(vga) < 0) return FD2_SCENE_RESULT_ERROR;
        if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    }
    return FD2_SCENE_RESULT_OK;
}

/* 同步只读 modal：不调用玩法提交／事件／RNG，两个像素缓冲归本次调用。
 * 原input_check只peek；检测退出和关闭末尾两处4e381清同owner队列，
 * 不重置物理held/repeat。 */
static int play_field_overview(fd2_field_game *game, fd2_vga *vga) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if (!game || !vga || !game->ready || !fd2_field_stage_is_supported(game->stage) ||
        game->failure != FD2_FIELD_FAILURE_NONE || game->active_side != 2 ||
        game->selected_unit != -1 || game->detail_visible ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    uint8_t *background = NULL, *overview = NULL;
    int result = FD2_SCENE_RESULT_ERROR;
    uint64_t host_deadline=0;
    background = malloc(VGA_W * VGA_H);
    if (!background) goto done;
    overview = malloc(VGA_W * VGA_H);
    if (!overview) goto done;

    /* renderer先在未present的原frame上预检／合成；不用复制整个VGA/input
     * 对象借用framebuffer。成功缓存四块后，A才改为最初干净背景。 */
    memcpy(background, vga->framebuffer, VGA_W * VGA_H);
    fd2_field_overview data = {
        game->stage, game->turn_number, game->resource_value, &game->units,
        game->roster, game->roster_count
    };
    if (fd2_field_overview_draw(vga, &game->info_assets, &game->font,
                                &game->ui_text, &data) != 0) {
        memcpy(vga->framebuffer, background, VGA_W * VGA_H);
        goto done;
    }
    memcpy(overview, vga->framebuffer, VGA_W * VGA_H);
    memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    /* sub_1B1E7 @0x1b1e7入口：资源预检成功后11eee→127a9，
     * 不含1297d或water；11eee同时观察terrain/range，127e0推进raw26。 */
    if(fd2_field_game_runtime_overview_refresh(game,vga,0)!=0)goto done;
    memcpy(background, vga->framebuffer, VGA_W * VGA_H);
    int open_result = overview_transition(vga, background, overview, 1);
    if (open_result != FD2_SCENE_RESULT_OK) {
        result = open_result;
        goto done;
    }
    int reported_open = 0;
    for (;;) {
        /* 同一1b1e7稳定页：1297d→4e31c→11eee→127a9；
         * 不借game_tick推进模态以外的deadline／玩法字段。 */
        if(fd2_field_game_runtime_overview_refresh(game,vga,1)!=0)goto done;
        /* phase12为四块稳定位置；静态内容已预检并缓存，不重复格式化。 */
        overview_transition_frame(vga, overview, 12, 1);
        if(modal_host_present(vga,&host_deadline)<0)goto done;
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            goto done;
        }
        if (!reported_open) {
            puts("field overview: opened");
            fflush(stdout);
            reported_open = 1;
        }
        if (fd2_input_has_any_key(&vga->input)) {
            fd2_input_clear_keys(&vga->input); /* 1b1e7检测退出键后。 */
            if(fd2_input_take_quit(&vga->input)){result=FD2_SCENE_RESULT_HOST_QUIT;goto done;}
            break;
        }
    }
    modal_host_release(vga,&host_deadline);
    int close_result = overview_transition(vga, background, overview, 0);
    if (close_result != FD2_SCENE_RESULT_OK) {
        result = close_result;
        goto done;
    }
    memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    if(play_present_copy(vga)<0){result=FD2_SCENE_RESULT_ERROR;goto done;}
    if (fd2_input_take_quit(&vga->input)) {
        result = FD2_SCENE_RESULT_HOST_QUIT;
        goto done;
    }
    fd2_input_clear_keys(&vga->input); /* 1b1e7关闭末尾。 */
    if(fd2_input_take_quit(&vga->input)){result=FD2_SCENE_RESULT_HOST_QUIT;goto done;}
    puts("field overview: closed");
    fflush(stdout);
    result = FD2_SCENE_RESULT_OK;
done:
    modal_host_release(vga,&host_deadline);
    free(overview);
    free(background);
    return result;
}

/* field_auxiliary_map_execute @0x2000a（原登记 sub_2000A）：F1/PageUp独立全场缩放图。
 * 2000a原64×64指针cache域；不按stage/shape编号或首关尺寸设门。
 * 预检先于任何framebuffer写入；复用session的terrain.frames，不替换owner。 */
static int auxiliary_field_ready(const fd2_field_game *game) {
    if (!game || !game->ready ||
        game->failure != FD2_FIELD_FAILURE_NONE || game->active_side != 2 ||
        game->selected_unit != -1 || game->detail_visible ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        game->player_post_waiting_phase ||
        game->map.width<=0 || game->map.height<=0 || game->map.width>64 || game->map.height>64 || !game->map.cells ||
        game->terrain.sheet.width != 24 || game->terrain.sheet.height != 24 ||
        !game->terrain.sheet.frame_count || !game->terrain.frames ||
        !game->terrain.attrs || game->units.count > FD2_FIELD_MAX_UNITS ||
        game->cursor_cell_x < 0 || game->cursor_cell_x >= game->map.width ||
        game->cursor_cell_y < 0 || game->cursor_cell_y >= game->map.height ||
        game->camera_cell_x < 0 ||
        game->camera_cell_x > (game->map.width>FD2_FIELD_VIEW_CELLS_X ? game->map.width-FD2_FIELD_VIEW_CELLS_X:0) ||
        game->camera_cell_y < 0 ||
        game->camera_cell_y > (game->map.height>FD2_FIELD_VIEW_CELLS_Y ? game->map.height-FD2_FIELD_VIEW_CELLS_Y:0))
        return 0;
    for (int y = 0; y < game->map.height; y++) {
        for (int x = 0; x < game->map.width; x++) {
            size_t index = fd2_field_cell_terrain(
                fd2_field_map_cell(&game->map, x, y));
            const fd2_image *frame = fd2_terrain_frame(&game->terrain, index);
            if (index >= game->terrain.attr_count || !frame ||
                !frame->pixels || frame->width != 24 || frame->height != 24)
                return 0;
        }
    }
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if ((unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            unit->unit_id == 121 || unit->race == 10)
            continue;
        if (unit->side > 2 || unit->x >= game->map.width ||
            unit->y >= game->map.height)
            return 0;
    }
    return 1;
}

/* field_view_render_scaled @0x1f558（原登记同名）：tile跨度0xc00，
 * 单像素0x80。既有sampler对负坐标作floor，绘制内区312×192；先清整帧
 * 保留4px黑边。phase=0固定使用原terrain_id，不推进地形动画或遮挡层。 */
static int auxiliary_transition_frame(const fd2_field_game *game,
                                        fd2_vga *vga, int phase) {
    int start_x = game->camera_cell_x * 0xc00 + FD2_FIELD_VIEW_CELLS_X * 0x600;
    int start_y = game->camera_cell_y * 0xc00 + FD2_FIELD_VIEW_CELLS_Y * 0x600;
    int end_x = game->map.width * 0x600;
    int end_y = game->map.height * 0x600;
    /* 开1..7与关6..1共用参数；中心系数为(phase+1)/8，非phase/8。 */
    int center_x = start_x + (end_x - start_x) * (phase + 1) / 8;
    int center_y = start_y + (end_y - start_y) * (phase + 1) / 8;
    int zoom_delta=game->map.height>40 ? 0x380:0x280;
    int step = 0x80 + phase * zoom_delta / 7;
    fd2_vga_clear(vga, 0);
    fd2_terrain_render_field_scaled_animated(vga, &game->terrain, &game->map,
                                             center_x, center_y, step, 0, 0);
    /* 1f558直接写53A49 raw-prefix、2000a再memmove到VGA；不是72/72桥。
     * 稳态marker1f6ef只写VGA，不回灌工作页。 */
    return fd2_field_page_phase_prefix_write(vga->field_page,vga->framebuffer,sizeof(vga->framebuffer));
}

/* field_auxiliary_marker_draw @0x1f6ef（原登记 sub_1F6EF）：scale由map高>40选3，否则4，
 * 固色块边长scale-1。所有参与者已预检，focus最后覆盖同格actor。 */
static void auxiliary_marker(fd2_vga *vga, const fd2_field_map *map,
                              int x, int y, uint8_t color) {
    int scale=map->height>40 ? 3:4;
    int sx=x*scale+160-map->width*scale/2;
    int sy=y*scale+100-map->height*scale/2;
    for (int row=0;row<scale-1;row++)
        memset(vga->framebuffer+(size_t)(sy+row)*VGA_STRIDE+sx,color,(size_t)scale-1u);
}

static void auxiliary_markers(const fd2_field_game *game, fd2_vga *vga,
                               int phase) {
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if ((unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            unit->unit_id == 121 || unit->race == 10)
            continue;
        uint8_t color;
        if(fd2_field_auxiliary_marker_color(unit->side,(unsigned)phase,&color)==0)
            auxiliary_marker(vga,&game->map,unit->x,unit->y,color);
    }
    auxiliary_marker(vga, &game->map, game->cursor_cell_x,
                       game->cursor_cell_y, (uint8_t)(24 + phase));
}

static void auxiliary_next_phase(int *phase, int *direction) {
    *phase += *direction;
    if (*phase < 0 || *phase > 7) {
        *direction = -*direction;
        *phase += *direction; /* 端点重复：7..0,0..7,7.. */
    }
}

/* field_auxiliary_map_execute @0x2000a（原sub_2000A）201cc..202e2：
 * 先peek；已到202db后才有本次函数写出的fresh anchor。首次原DWORD
 * 来自正常caller的FS/GS旧字节，实际值尚未资格；have_anchor=0仅保留
 * 当前候选的首次先画行为，绝不是原版首门或初值的替身证明。 */
static int play_auxiliary_wait_step(fd2_field_game *game,fd2_vga *vga,
        int *phase,int *direction,int *have_anchor,int32_t *anchor,
        int *key_ready,int *updated) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    int terminal=play_field_terminal_result(game,vga);
    if(terminal!=FD2_SCENE_RESULT_OK)return terminal;
    *key_ready=0;*updated=0;
    if(fd2_input_has_any_key(&vga->input)){*key_ready=1;return FD2_SCENE_RESULT_OK;}
    if(*have_anchor) {
        int32_t now;if(play_clock_tick(vga,&now)<0){return FD2_SCENE_RESULT_ERROR;}
        /* 原signed Δ>=2或Δ<0，逐bit等同unsigned Δ>=2；0/1不画不写锚。 */
        if((uint32_t)now-(uint32_t)*anchor<2u)return FD2_SCENE_RESULT_OK;
    }
    auxiliary_markers(game,vga,*phase); /* actor顺序，focus最后覆盖。 */
    auxiliary_next_phase(phase,direction);
    if(play_clock_tick(vga,anchor)<0)return FD2_SCENE_RESULT_ERROR; /* 202db：实际body之后fresh读。 */
    *have_anchor=1;*updated=1;
    return FD2_SCENE_RESULT_OK;
}

static int play_field_auxiliary(fd2_field_game *game, fd2_vga *vga) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_SCENE_RESULT_ERROR;
    if (!vga) return FD2_SCENE_RESULT_ERROR;
    /* 主loop已先处理quit；私有入口也保持宿主关闭先于资源／能力门。 */
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (!vga->field_page || !auxiliary_field_ready(game)) return FD2_SCENE_RESULT_ERROR;
    int result = FD2_SCENE_RESULT_HOST_QUIT;
    /* 2000a开7/关6次1f558→memmove，无额外delay；稳态读点另分账。 */
    for (int phase = 1; phase <= 7; phase++) {
        if(auxiliary_transition_frame(game,vga,phase)!=0){result=FD2_SCENE_RESULT_ERROR;goto done;}
        if(play_present_copy(vga)<0){result=FD2_SCENE_RESULT_ERROR;goto done;}
        if (fd2_input_take_quit(&vga->input)) goto done;
    }
    fd2_input_clear_keys(&vga->input); /* 2000a打开完成后仅一次。 */
    if(fd2_input_take_quit(&vga->input))goto done;
    int phase = 7, direction = -1, have_anchor=0;
    int32_t last_pulse; /* 仅have_anchor=1后读取；不伪造原首次FS/GS值。 */
    int reported_open = 0;
    for (;;) {
        /* 稳态地形保留开phase7的原始cache采样；仅重写同位置方块。
         * 不调用game_tick，不改palette/dac，冻结进入时的当前水色周期。 */
        int key_ready,updated;
        int waited=play_auxiliary_wait_step(game,vga,&phase,&direction,&have_anchor,
            &last_pulse,&key_ready,&updated);
        if(waited!=FD2_SCENE_RESULT_OK){result=waited;goto done;}
        if(key_ready)break;
        if(updated && play_present_copy(vga)<0){result=FD2_SCENE_RESULT_ERROR;goto done;}
        waited=play_field_terminal_result(game,vga);
        if(waited!=FD2_SCENE_RESULT_OK){result=waited;goto done;}
        if (!reported_open) {
            puts("field auxiliary: opened");
            fflush(stdout);
            reported_open = 1;
        }
        /* 原忙轮不重复写VGA；H仅泵本input owner并请求1ms让出，不构造
         * 16ms画面deadline，也不让present或pump推进source phase。 */
        fd2_input_poll_host_events(&vga->input);
        if(!fd2_input_has_any_key(&vga->input) && !fd2_input_take_quit(&vga->input))
            if(fd2_vga_delay_ms(vga,1)<0){result=FD2_SCENE_RESULT_ERROR;goto done;}
    }
    /* 202e7..202fa设置INT16/AH10并读一项；input_check不是消费本身。 */
    if(!fd2_input_take_key(&vga->input,NULL)){result=FD2_SCENE_RESULT_ERROR;goto done;}
    for (int close_phase = 6; close_phase >= 1; close_phase--) {
        if(auxiliary_transition_frame(game,vga,close_phase)!=0){result=FD2_SCENE_RESULT_ERROR;goto done;}
        if(play_present_copy(vga)<0){result=FD2_SCENE_RESULT_ERROR;goto done;}
        if (fd2_input_take_quit(&vga->input)) goto done;
        /* 关闭不再清余队；仅上方AH10已消费的触发键不复用。 */
    }
    /* sub_2000A @0x2000a尾完整调用sub_11CAC(0)，含actor/cursor、
     * water、tiles/range与raw26；开/稳/关期间不推进这些相位。 */
    if(fd2_field_game_runtime_refresh(game,vga,0)!=0) {
        result=FD2_SCENE_RESULT_ERROR;goto done;
    }
    if(play_present_copy(vga)<0){result=FD2_SCENE_RESULT_ERROR;goto done;}
    if (fd2_input_take_quit(&vga->input)) goto done;
    puts("field auxiliary: closed");
    fflush(stdout);
    result = FD2_SCENE_RESULT_OK;
done:
    return result;
}


static void apply_field_options(const fd2_field_game *game,fd2_vga *vga,
        fd2_field_audio *field_audio, fd2_field_play_context *context, int fade_music) {
    if (!game) return;
    /* field_options_menu_execute @0x1728c（原同名登记）：toggle为1秒sequence渐变。
     * sfx_play @0x25a96（原FUN_000231de）/sfx_play_secondary @0x25b45
     * （原FUN_0002328d）：SFX关只gate未来cue；Load仅同步配置。 */
    if (context) {
        context->music_enabled = game->option_values[0];
        context->sfx_enabled = game->option_values[1];
        context->effect_option=game->option_values[2];
        context->camera_option=game->option_values[3];
        if (context->bgm) {(void)fd2_bgm_set_music_enabled(context->bgm, context->music_enabled != 0, fade_music);if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)){return;}}
    }
    if (field_audio && field_audio->player && fd2_pcm_player_available(field_audio->player))
        {(void)fd2_pcm_player_set_enabled(field_audio->player, game->option_values[1] != 0);if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)){return;}}
    if (field_audio && field_audio->secondary_player && fd2_pcm_player_available(field_audio->secondary_player))
        {(void)fd2_pcm_player_set_enabled(field_audio->secondary_player, game->option_values[1] != 0);if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)){return;}}
}

static void detail_phase_sfx(void *userdata, int opening, int phase) {
    fd2_field_audio *audio = userdata;
    if (!audio || fd2_field_detail_sfx_for_phase(opening, phase) < 0) return;
    /* 原版详情路径在上述 phase 调用
     * sfx_play(DAT_00003eec,sample,1)；全局 AIL handle 会重启前一声。 */
    {(void)fd2_field_audio_play(audio, opening
        ? FD2_FIELD_SFX_DETAIL_OPEN : FD2_FIELD_SFX_DETAIL_CLOSE);if(fd2_bios_clock_error()<0){return;}}
}

/* 17aed的单个翻页/关闭叶；完整open/wait/branch/flip/wait/close在冷图。 */
static int play_field_detail_transition(fd2_field_game *game,fd2_vga *vga,
        fd2_field_audio *audio,int flip) {
    if (!vga) return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if (!game || !game->detail_visible || game->detail_unit < 0 ||
        (size_t)game->detail_unit >= game->units.count)
        return FD2_SCENE_RESULT_ERROR;
    int result = flip
        ? fd2_field_game_animate_detail_magic_page(game, vga, detail_phase_sfx, audio)
        : fd2_field_game_animate_detail(game, vga, 0, detail_phase_sfx, audio);
    if (result < 0) return FD2_SCENE_RESULT_ERROR;
    if (result == 1 || fd2_input_take_quit(&vga->input))
        return FD2_SCENE_RESULT_HOST_QUIT;
    if (!flip) fd2_field_game_close_detail(game);
    return FD2_SCENE_RESULT_OK;
}

static int play_field_detail_step(fd2_field_game *game,fd2_vga *vga,fd2_field_audio *audio) {
    int flip=game && game->detail_visible && game->detail_unit>=0 &&
        (size_t)game->detail_unit<game->units.count && game->detail_page==FD2_FIELD_DETAIL_EQUIPMENT &&
        fd2_field_unit_magic_list_build(&game->units.items[game->detail_unit],NULL,0)!=0;
    return play_field_detail_transition(game,vga,audio,flip);
}

/* dialog_wait_key @0x16c57（原同名）由17aed两次传0；详情原portrait在
 * (8,10)，不是普通对白origin1832/36887。局部blink不改App mouth DWORD。
 * fd2_dialog_wait_*是既有有限原语，使用同一Game借用RNG及App水色。 */
static int play_field_detail_wait(fd2_field_game *game,fd2_vga *vga,const fd2_image *blink) {
    if(!game || !vga || !blink || !blink->pixels || blink->width!=80 || blink->height!=80 ||
        !game->detail_visible || !game->attack_rng || !game->detail_portrait.pixels ||
        game->detail_portrait.width!=80 || game->detail_portrait.height!=80)return FD2_SCENE_RESULT_ERROR;
    int result=play_field_terminal_result(game,vga);
    if(result!=FD2_SCENE_RESULT_OK)return result;
    uint64_t host_deadline=0;
    fd2_dialog_wait_state wait;uint64_t actual;
    if(play_clock_ns(vga,&actual)<0)return FD2_SCENE_RESULT_ERROR;
    if(fd2_dialog_wait_begin(&wait,actual,0,game->attack_rng,game->attack_rng_userdata)!=0)
        return FD2_SCENE_RESULT_ERROR;
    for(;;) {
        result=play_field_terminal_result(game,vga);
        if(result!=FD2_SCENE_RESULT_OK)break;
        if(fd2_input_take_key(&vga->input,NULL))break;
        if(fd2_field_water_tick_live(vga)<0){result=FD2_SCENE_RESULT_ERROR;break;}
        fd2_dialog_wait_visual visual;
        if(play_clock_ns(vga,&actual)<0){result=FD2_SCENE_RESULT_ERROR;break;}
        int updated=fd2_dialog_wait_step(&wait,actual,&visual);
        if(updated<0){result=FD2_SCENE_RESULT_ERROR;break;}
        if(visual.portrait>=0)
            fd2_map_sprite_blit(vga,visual.portrait==3?blink:&game->detail_portrait,8,10,-1);
        if(updated) {
            if(visual.portrait==0)wait.countdown=game->attack_rng(game->attack_rng_userdata)%30u+2u;
            if(play_clock_tick(vga,&wait.last_tick)<0){result=FD2_SCENE_RESULT_ERROR;break;}
        }
        if(modal_host_present(vga,&host_deadline)<0){result=FD2_SCENE_RESULT_ERROR;break;}
    }
    modal_host_release(vga,&host_deadline);
    return result;
}

/* field_unit_detail_present @0x17aed（原sub_17AED）：正式Enter/F2共用。
 * frame3在open写owner/呈现/RNG前预检，生命周期只在此modal借图，
 * 原Game仍独占detail_portrait0/页/关闭；无新增SAV或业务游标。 */
static int play_field_detail_present(fd2_field_game *game,fd2_vga *vga,
        fd2_field_audio *audio,size_t actor) {
    if(!game || !vga || !game->ready || game->failure!=FD2_FIELD_FAILURE_NONE ||
        !game->attack_rng || game->detail_visible || actor>=game->units.count ||
        game->units.count>FD2_FIELD_MAX_UNITS)return FD2_SCENE_RESULT_ERROR;
    const fd2_static_flow *flow=fd2_static_flow_lookup("runtime","runtime.detail");
    if(!flow)return FD2_SCENE_RESULT_ERROR;
    for(size_t i=0;i<flow->count;i++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,(int)i);
        if(!node || node->views!=FD2_FLOW_PRESENT ||
            (strcmp(node->op,"detail_open") && strcmp(node->op,"detail_wait") &&
             strcmp(node->op,"detail_has_magic") && strcmp(node->op,"detail_flip") &&
             strcmp(node->op,"detail_close") && strcmp(node->op,"detail_done") &&
             strcmp(node->op,"detail_fail")))return FD2_SCENE_RESULT_ERROR;
    }
    fd2_image blink={0};
    if(fd2_portrait_decode_frame(&blink,&game->dato_archive,game->units.items[actor].unit_id,3)!=0 ||
        !blink.pixels || blink.width!=80 || blink.height!=80) {
        fd2_image_free(&blink);return FD2_SCENE_RESULT_ERROR;
    }
    int result=FD2_SCENE_RESULT_ERROR,pc=flow->entry;
    for(unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if(!node)break;
        result=play_field_terminal_result(game,vga);
        if(result!=FD2_SCENE_RESULT_OK)break;
        unsigned port=FD2_FLOW_NEXT;
        if(!strcmp(node->op,"detail_done"))break;
        if(!strcmp(node->op,"detail_fail")){result=FD2_SCENE_RESULT_ERROR;break;}
        if(!strcmp(node->op,"detail_open")) {
            result=fd2_field_game_open_detail(game,actor)!=0?FD2_SCENE_RESULT_ERROR:
                fd2_field_game_animate_detail(game,vga,1,detail_phase_sfx,audio);
            if(!result)fd2_input_clear_keys(&vga->input); /* 17e0b@17ee8。 */
            if(result==1 || (!result && fd2_input_take_quit(&vga->input)))result=FD2_SCENE_RESULT_HOST_QUIT;
        } else if(!strcmp(node->op,"detail_wait"))result=play_field_detail_wait(game,vga,&blink);
        else if(!strcmp(node->op,"detail_has_magic"))
            port=fd2_field_unit_magic_list_build(&game->units.items[actor],NULL,0)?FD2_FLOW_YES:FD2_FLOW_NO;
        else if(!strcmp(node->op,"detail_flip"))result=play_field_detail_transition(game,vga,audio,1);
        else if(!strcmp(node->op,"detail_close"))result=play_field_detail_transition(game,vga,audio,0);
        else {result=FD2_SCENE_RESULT_ERROR;break;}
        if(result==FD2_SCENE_RESULT_HOST_QUIT)break;
        if(result<0)port=FD2_FLOW_ERROR;
        pc=fd2_static_flow_edge(flow,node,port);
        if(pc<0){result=FD2_SCENE_RESULT_ERROR;break;}
        if(budget+1==FD2_FLOW_STEP_BUDGET)result=FD2_SCENE_RESULT_ERROR;
    }
    fd2_image_free(&blink);return result;
}

typedef enum {
    FD2_FIELD_ACTION_NONE = 0,
    FD2_FIELD_ACTION_LEFT,
    FD2_FIELD_ACTION_RIGHT,
    FD2_FIELD_ACTION_UP,
    FD2_FIELD_ACTION_DOWN,
    FD2_FIELD_ACTION_CONFIRM,
    FD2_FIELD_ACTION_CANCEL,
    FD2_FIELD_ACTION_DETAIL,
    FD2_FIELD_ACTION_AUXILIARY,
    FD2_FIELD_ACTION_FOCUS_CYCLE,
    FD2_FIELD_ACTION_EXIT
} fd2_field_action;

static fd2_field_action field_action_from_input(fd2_input_action action) {
    switch (action) {
        case FD2_INPUT_ACTION_LEFT: return FD2_FIELD_ACTION_LEFT;
        case FD2_INPUT_ACTION_RIGHT: return FD2_FIELD_ACTION_RIGHT;
        case FD2_INPUT_ACTION_UP: return FD2_FIELD_ACTION_UP;
        case FD2_INPUT_ACTION_DOWN: return FD2_FIELD_ACTION_DOWN;
        case FD2_INPUT_ACTION_CONFIRM: return FD2_FIELD_ACTION_CONFIRM;
        case FD2_INPUT_ACTION_CANCEL: return FD2_FIELD_ACTION_CANCEL;
        case FD2_INPUT_ACTION_FIELD_DETAIL: return FD2_FIELD_ACTION_DETAIL;
        case FD2_INPUT_ACTION_FIELD_AUXILIARY:
            return FD2_FIELD_ACTION_AUXILIARY;
        case FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE:
            return FD2_FIELD_ACTION_FOCUS_CYCLE;
        case FD2_INPUT_ACTION_EXIT: return FD2_FIELD_ACTION_EXIT;
        default: return FD2_FIELD_ACTION_NONE;
    }
}

/* notice只保来源诊断；正式执行统一使用common父PC。坏外部deferred
 * 状态明确失败，不再创建第二套整事件Scene重放入口。 */
static int field_event_diagnostics_ready(const fd2_field_game *game) {
    if(!game || game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
    for(size_t i=0;i<game->event_log_count;i++)
        if(!game->event_log[i].handled || game->event_log[i].presentation_deferred)return -1;
    return 0;
}

/* 1aa1d/190ac仅借mode0列表，选中项保存在本次modal，不冒充玩家菜单owner。 */
static int loot_inventory_select(fd2_field_game *game, fd2_vga *vga,
    fd2_field_audio *audio, size_t actor, int *raw_slot) {
    if (actor >= game->units.count || !raw_slot || !game->attack_rng)
        return FD2_SCENE_RESULT_ERROR;
    const fd2_field_unit *unit = &game->units.items[actor];
    int selected = 0;
    if (fd2_field_item_list_select_step(unit, &selected,
            FD2_FIELD_ITEM_LIST_KEY_OTHER, 0) != 0 ||
        !fd2_field_game_detail_upper_is_ready(game, unit)) return -1;
    fd2_image portraits[2] = {{0}};
    fd2_vga *page = calloc(1, sizeof(*page));
    uint8_t *background = malloc(VGA_W * VGA_H);
    int result = FD2_SCENE_RESULT_ERROR;
    uint64_t host_deadline=0;
    if (!page || !background || list_portraits_open(game, unit->unit_id, portraits) != 0)
        goto done;
    memcpy(background, vga->framebuffer, VGA_W * VGA_H);
    memcpy(page->framebuffer, background, VGA_W * VGA_H);
    if (fd2_field_detail_draw(page, &game->info_assets, &game->font,
        &game->ui_text, unit, &portraits[0]) != 0) goto done;
    if (fd2_field_item_list_draw(page, &game->info_assets, &game->font,
            &game->ui_text, unit, -1) != 0) goto done;
    fd2_field_audio *sfx_audio = game->option_values[1] ? audio : NULL;
    for (int opening = 1; opening >= 0; opening--) {
        int moved = detail_page_transition(vga, page, background, opening, sfx_audio);
        if (moved != FD2_SCENE_RESULT_OK) {
            result = moved;
            goto done;
        }
        if (!opening) break;
        fd2_input_clear_keys(&vga->input); /* 1b932→17e0b打开末尾。 */
        if(fd2_input_take_quit(&vga->input)){result=FD2_SCENE_RESULT_HOST_QUIT;goto done;}
        for (;;) {
            if (fd2_field_item_list_draw(vga, &game->info_assets, &game->font,
                    &game->ui_text, unit, selected) != 0) goto done;
            /* 190ac/1aa1d→1b932→1b9de→16c57，同普通库存等待。 */
            fd2_input_event event;
            int waited = list_wait_key(game, vga, portraits, &event, &host_deadline);
            if (waited != FD2_SCENE_RESULT_OK) {
                result = waited;
                goto done;
            }
            int previous = selected;
            int step = fd2_field_item_list_select_step(unit, &selected,
                item_list_key(event.key), 0);
            if (step == -2) goto done;
            if (step != 0) {
                *raw_slot = step == 1 ? selected : -1;
                result = FD2_SCENE_RESULT_OK;
                break;
            }
            if (audio && game->option_values[1] &&
                (previous != selected || event.key == FD2_INPUT_KEY_UP || event.key == FD2_INPUT_KEY_DOWN))
                {(void)fd2_field_audio_play(audio, FD2_FIELD_SFX_COMMAND_MENU);if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)){goto done;}}
        }
    }
    memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    if(play_present_copy(vga)<0){result=FD2_SCENE_RESULT_ERROR;goto done;}
done:
    modal_host_release(vga,&host_deadline);
    free(background); free(page);
    for (unsigned i = 0; i < 2; i++) fd2_image_free(&portraits[i]);
    return result;
}

static int loot_text_prepare(const fd2_field_game *game,
    const fd2_field_loot_notice *notice, field_progress_text *text) {
    if (!game || !notice || !text || !game->font.glyphs ||
        !game->font.glyph_count || game->font.glyph_count > game->font.size / 32u)
        return -1;
    memset(text, 0, sizeof(*text));
    field_text_parameters parameters = {notice->value,
        fd2_field_item_record_get(notice->item_id) ? 181u + notice->item_id : SIZE_MAX,
        fd2_field_item_record_get(notice->old_item_id) ? 181u + notice->old_item_id : SIZE_MAX};
    return field_text_expand(game, &parameters, notice->fragment, 0, text);
}

static int loot_resources_preflight(const fd2_field_game *game,
    const fd2_field_loot_notice *notice, void *userdata) {
    const fd2_archive *fdother = userdata;
    if (!game || !notice || notice->actor_index >= game->units.count ||
        !game->map.cells || fd2_scene_field_message_validate(fdother) != 0)
        return -1;
    if (notice->kind == FD2_FIELD_LOOT_STORY)
        return fd2_scene_field_story_validate(game, notice->fragment);
    const fd2_field_unit *unit = &game->units.items[notice->actor_index];
    if (notice->kind == FD2_FIELD_LOOT_INVENTORY) {
        fd2_vga *page = calloc(1, sizeof(*page));
        if (!page) return -1;
        int valid = fd2_field_game_detail_upper_is_ready(game, unit) &&
            fd2_field_item_list_draw(page, &game->info_assets, &game->font,
                &game->ui_text, unit, -1) == 0;
        free(page);
        if (!valid) return -1;
    } else if (notice->kind == FD2_FIELD_LOOT_MESSAGE || notice->kind == FD2_FIELD_LOOT_CHOICE) {
        field_progress_text text;
        if (loot_text_prepare(game, notice, &text) != 0) return -1;
        if (notice->kind == FD2_FIELD_LOOT_CHOICE) {
            static const unsigned ids[] = {48,49,51,52};
            if (!game->command_assets.ready) return -1;
            for (size_t i = 0; i < sizeof(ids)/sizeof(ids[0]); i++) {
                const fd2_image *image = &game->command_assets.frames[ids[i]];
                if (!image->pixels || image->width != 24 || image->height != 16) return -1;
            }
        }
    } else return notice->kind == FD2_FIELD_LOOT_DELAY ? 0 : -1;
    return portrait_frames_valid(game, unit->unit_id) ? 0 : -1;
}

/* sub_19953 @0x19953：Yes/No固定24×16，phase/2高亮；返回前phase0。 */
static void loot_choice_draw(fd2_field_game *game, fd2_vga *vga,
    const uint8_t *background, int displacement, int selected, unsigned phase) {
    for (int row = 0; row < 16; row++)
        memcpy(vga->framebuffer + (168 + row) * VGA_STRIDE + 232,
            background + row * 56, 56);
    fd2_map_sprite_blit(vga, &game->command_assets.frames[48u + (selected == 0 && phase/2u)], 248-displacement, 168, 0);
    fd2_map_sprite_blit(vga, &game->command_assets.frames[51u + (selected == 1 && phase/2u)], 248+displacement, 168, 0);
}

typedef struct {
    field_progress_box *box;
    fd2_field_game *game;
    fd2_vga *vga;
    fd2_vga *snapshot;
    fd2_field_loot_reply *reply;
    uint8_t background[56*16];
    uint32_t counter;
    int selected,blinking;
    uint64_t host_deadline;
} field_choice_scope;

static int play_choice_step(fd2_field_choice_step step,int value,void *userdata) {
    field_choice_scope *scope=userdata;
    fd2_vga *vga=scope->vga;
    fd2_field_game *game=scope->game;
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return -1;
    if (fd2_input_take_quit(&vga->input)) return 3;
    if (step==FD2_FIELD_CHOICE_BEGIN) {
        if (!game->attack_rng || !scope->reply) return -1;
        scope->selected=0;scope->blinking=0;
        scope->counter=game->attack_rng(game->attack_rng_userdata)%30u+2u;
        memcpy(scope->snapshot->framebuffer,vga->framebuffer,sizeof(vga->framebuffer));
        for (int row=0;row<16;row++) memcpy(scope->background+row*56,
            vga->framebuffer+(168+row)*VGA_STRIDE+232,56);
    } else if (step==FD2_FIELD_CHOICE_DRAW || step==FD2_FIELD_CHOICE_STEADY) {
        /* 19953 opening及197e5 closing用normal帧，不借当前phase。 */
        memcpy(vga->framebuffer,scope->snapshot->framebuffer,sizeof(vga->framebuffer));
        loot_choice_draw(game,vga,scope->background,value,scope->selected,
            step==FD2_FIELD_CHOICE_STEADY?vga->menu_highlight_phase:0);
        if (fd2_field_page_choice_bottom_write(vga->field_page,vga->framebuffer,
                sizeof(vga->framebuffer))!=0 || runtime_project(vga,NULL)!=0) return -1;
        if(step==FD2_FIELD_CHOICE_DRAW) {
            modal_host_release(vga,&scope->host_deadline);
            if(play_present_copy(vga)<0)return FD2_SCENE_RESULT_ERROR; /* 19953/197e5各4次，不等16ms。 */
        } else if(modal_host_present(vga,&scope->host_deadline)<0)return -1;
    } else if (step==FD2_FIELD_CHOICE_WAIT_KEY) {
        return fd2_input_has_any_key(&vga->input)?1:0;
    } else if (step==FD2_FIELD_CHOICE_READ_KEY) {
        fd2_input_event event;
        if (!fd2_input_take_key(&vga->input,&event)) return -1;
        modal_host_release(vga,&scope->host_deadline);
        if (event.key==FD2_INPUT_KEY_LEFT) scope->selected=0;
        else if (event.key==FD2_INPUT_KEY_RIGHT) scope->selected=1;
        else if (event.key==FD2_INPUT_KEY_ENTER || event.key==FD2_INPUT_KEY_SPACE ||
                event.key==FD2_INPUT_KEY_KEYPAD_CONFIRM) {
            *scope->reply=scope->selected?FD2_FIELD_LOOT_NO:FD2_FIELD_LOOT_YES;
            return 1;
        } else if (event.key==FD2_INPUT_KEY_ESCAPE || event.key==FD2_INPUT_KEY_CANCEL) {
            *scope->reply=FD2_FIELD_LOOT_CANCEL;return 2;
        }
    } else if (step==FD2_FIELD_CHOICE_TICK) {
        if(fd2_field_water_tick_live(vga)<0)return -1;
        int due=menu_highlight_live(vga,1);
        if (due<0) return -1;
        if (due) return 1;
        if(modal_host_present(vga,&scope->host_deadline)<0)return -1;
    } else if (step==FD2_FIELD_CHOICE_BLINK) {
            if (scope->blinking) {
                progress_portrait(scope->box,scope->snapshot,0,0);
                scope->counter=game->attack_rng(game->attack_rng_userdata)%30u+10u;
                scope->blinking=0;
            } else if (scope->counter--==0u) {
                progress_portrait(scope->box,scope->snapshot,3,0);scope->blinking=1;
            }
    } else if (step==FD2_FIELD_CHOICE_HAS_FIELD) return 1;
    else if (step==FD2_FIELD_CHOICE_FIELD_REDRAW)
        return fd2_field_game_runtime_refresh(game,vga,2);
    else if (step==FD2_FIELD_CHOICE_SNAPSHOT)
        return fd2_field_page_choice_snapshot_write(vga->field_page,
            scope->snapshot->framebuffer,sizeof(scope->snapshot->framebuffer));
    else if (step==FD2_FIELD_CHOICE_FINISH) {
        modal_host_release(vga,&scope->host_deadline);
        fd2_field_system_menu_highlight_finish(&vga->menu_highlight_phase);
    }
    else return -1;
    return fd2_input_take_quit(&vga->input)?3:0;
}

static int loot_choice(field_progress_box *box, fd2_field_game *game,
    fd2_vga *vga, fd2_field_loot_reply *reply) {
    field_choice_scope scope={0};scope.box=box;scope.game=game;scope.vga=vga;scope.reply=reply;
    scope.snapshot=calloc(1,sizeof(*scope.snapshot));
    if (!scope.snapshot) return FD2_SCENE_RESULT_ERROR;
    int result=fd2_field_choice_run(play_choice_step,&scope);
    modal_host_release(vga,&scope.host_deadline); /* 错误/HOST同样结束本次pump。 */
    if (!result) {
        for (int row=0;row<16;row++) memcpy(vga->framebuffer+(168+row)*VGA_STRIDE+232,
            scope.background+row*56,56);
        fd2_vga_upload_current_frame(vga);
        if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)result=-1;
    }
    free(scope.snapshot);
    return result>0?FD2_SCENE_RESULT_HOST_QUIT:result<0?FD2_SCENE_RESULT_ERROR:FD2_SCENE_RESULT_OK;
}

/* 16f55/19df7不借跨帧system page作第二输入owner；普通prompt完成后
 * 同步执行同一19953，默认YES与blink RNG在每次BEGIN按源恢复。 */
static int play_system_choice(fd2_field_game *game,fd2_vga *vga,
        field_progress_box *box,fd2_field_loot_reply *reply) {
    if (!box->page) return FD2_SCENE_RESULT_ERROR;
    return loot_choice(box,game,vga,reply);
}

/* 16f55/19df7共有1956b→15f84→19953→197e5→反馈→200ms→196cb。
 * 冷choice拥有等待/确认/取消；此口只做已选片段的窄文本/底框呈现。 */
static int play_system_confirmation_text(fd2_field_game *game,fd2_vga *vga,
        field_progress_box *box,fd2_field_audio *audio,size_t fragment,int fast,
        fd2_field_play_context *context) {
    int prompt=fragment==410u || fragment==413u || fragment==415u || fragment==417u || fragment==419u;
    if (!game->font.glyphs || !game->font.glyph_count ||
        game->font.glyph_count>game->font.size/32u) return -1;
    field_progress_text text;
    field_text_parameters parameters={game->message_number,game->message_new_fragment,game->message_old_fragment};
    if (field_text_expand(game,&parameters,fragment,0,&text)!=0) return -1;
    if (prompt) {
        if (box->page) return -1;
        unsigned portrait=fragment==417u
            ?(game->units.count?game->units.items[0].unit_id:UINT16_MAX):75u;
        if (fd2_scene_field_message_validate(game->fdother)!=0 ||
            !portrait_frames_valid(game,portrait)) return -1;
        if (fd2_field_game_runtime_refresh(game,vga,0)!=0) return -1;
        int opened=progress_box_open_portrait(box,game,vga,game->fdother,portrait,fast);
        if (opened) return opened;
    } else if (!box->page) return -1;
    int played=field_text_play(box,game,vga,game->fdother,audio,&text,99,prompt?127:146,fast);
    if (played) return played;
    if (prompt) {
        progress_portrait(box,vga,0,0); /* caller16559(0)，之后19953才取blink RNG。 */
        fd2_vga_upload_current_frame(vga);
        return fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0?-1:0;
    }
    if (fragment==FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED) {
        /* field_secondary_menu_execute @0x19df7（原同名）：1a2dc的416
         * 完整返回后，1a2e7请求-1/4000ms，才到1a2f4的200ms与196cb。
         * 只借App现有音乐owner；早期错误/HOST不新增请求，晚失败不回滚。 */
        int terminal=play_field_terminal_result(game,vga);
        if (terminal!=FD2_SCENE_RESULT_OK) return terminal;
        if (context && fd2_field_play_request_music(vga,context,game->stage,-1,"leave")!=0)
            return FD2_SCENE_RESULT_ERROR;
        terminal=play_field_terminal_result(game,vga);
        if (terminal!=FD2_SCENE_RESULT_OK) return terminal;
    }
    if(!fast && fd2_vga_delay_ms(vga,200)<0)return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    int box_closed=progress_box_close(box,game,vga,fast);
    return box_closed;
}

static int play_system_message(field_runtime_presentation *runtime,
        fd2_field_game *game,fd2_vga *vga,field_progress_box *box,
        fd2_field_audio *audio,size_t fragment,int fast) {
    int result=play_system_confirmation_text(game,vga,box,audio,fragment,fast,runtime->context);
    if (result==FD2_SCENE_RESULT_HOST_QUIT) runtime->host_quit=1;
    return result;
}

/* field_special_cell_execute @0x190ac（原登记同名）的j___delay @0x3790a：
 * 仅执行本次源ms等待，不重画、上传或借用既存frame_deadline_ns。
 * fd2_delay_ms负责宿主pump；普通FIFO排队，不推进水色/actor/RNG。 */
static int loot_fixed_wait(fd2_vga *vga, unsigned milliseconds, int fast) {
    if (!vga) return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    if(!fast && fd2_vga_delay_ms(vga,milliseconds)<0)return FD2_SCENE_RESULT_ERROR;
    return fd2_input_take_quit(&vga->input) ? FD2_SCENE_RESULT_HOST_QUIT : FD2_SCENE_RESULT_OK;
}

static int play_field_loot(fd2_field_game *game, fd2_vga *vga,
    const fd2_archive *fdother, fd2_field_audio *audio, int fast) {
    field_progress_box box = {0};
    int result = 0;
    for (;;) {
        result=play_field_terminal_result(game,vga);
        if(result!=FD2_SCENE_RESULT_OK)break;
        fd2_field_loot_notice notice;
        int pending = fd2_field_game_loot_peek(game, &notice);
        if (pending < 0) { result = -1; break; }
        if (!pending) { if (box.page) result = -1; break; }
        printf("field loot: kind=%d actor=%zu fragment=%u value=%u item=%u old=%u open=%u append=%u close=%u key=%u delay=%u\n",
            (int)notice.kind, notice.actor_index, notice.fragment, notice.value,
            notice.item_id, notice.old_item_id, notice.open_box, notice.append_line,
            notice.close_box, notice.wait_key, notice.wait_ms);
        fflush(stdout);
        fd2_field_loot_reply reply = FD2_FIELD_LOOT_ACK;
        int slot = -1;
        if (notice.kind == FD2_FIELD_LOOT_MESSAGE || notice.kind == FD2_FIELD_LOOT_CHOICE) {
            field_progress_text text;
            if (loot_text_prepare(game, &notice, &text) != 0) { result = -1; break; }
            if (notice.open_box) {
                if (box.page) { result = -1; break; }
                fd2_field_game_render(game, vga);
                result = progress_box_open(&box, game, vga, fdother, notice.actor_index, fast);
                if (result != 0) break;
            }
            if (!box.page) { result = -1; break; }
            result = field_text_play(&box, game, vga, fdother, audio, &text,
                99, notice.append_line ? 146 : 127, fast);
            if (result != 0) break;
            if (fd2_input_take_quit(&vga->input)) { result = FD2_SCENE_RESULT_HOST_QUIT; break; }
            if (notice.kind == FD2_FIELD_LOOT_MESSAGE &&
                fd2_field_game_loot_text_ready(game) != 0) { result = -1; break; }
            progress_portrait(&box, vga, 0, 0);
            if (notice.kind == FD2_FIELD_LOOT_CHOICE) result = loot_choice(&box, game, vga, &reply);
            else if (notice.wait_key) {
                /* 190ac/1aa1d在此直接16c57；已排帧键仍可完成wait0。 */
                result = progress_wait(&box, game, vga, fdother, fast, 0);
            }
            if (result == 0 && notice.wait_ms) result = loot_fixed_wait(vga, notice.wait_ms, fast);
        } else if (notice.kind == FD2_FIELD_LOOT_INVENTORY) {
            /* 原选择页独立开合；先完成仍留着的底框再进入mode0。 */
            if (box.page) {
                result = progress_box_close(&box,game,vga,fast);
                if (result != 0) break;
            }
            result = loot_inventory_select(game, vga, audio, notice.actor_index, &slot);
            reply = slot < 0 ? FD2_FIELD_LOOT_CANCEL : FD2_FIELD_LOOT_SLOT;
        } else if (notice.kind == FD2_FIELD_LOOT_DELAY) {
            result = loot_fixed_wait(vga, notice.wait_ms, fast);
        } else if (notice.kind == FD2_FIELD_LOOT_STORY) {
            result = fd2_scene_play_field_story(vga, fdother, game, notice.fragment, audio, fast);
        } else result = -1;
        if (result != 0) break;
        if (notice.close_box && box.page) {
            result = progress_box_close(&box,game,vga,fast);
            if (result != 0) break;
        }
        if (fd2_input_take_quit(&vga->input)) { result = FD2_SCENE_RESULT_HOST_QUIT; break; }
        if (fd2_field_game_loot_reply(game, reply, slot) != 0) { result = -1; break; }
        result=play_field_terminal_result(game,vga);
        if(result!=FD2_SCENE_RESULT_OK)break;
    }
    progress_box_free(&box);
    return result;
}

#include "field_common_event_present.h"

int fd2_field_play_bind_event_preflight(fd2_field_game *game,const fd2_archive *fdother) {
    if(!game || !fdother)return -1;
    game->event_preflight=common_event_resources_preflight;
    game->event_preflight_userdata=(void *)fdother;
    return 0;
}

static int play_field_progress(fd2_field_game *game, fd2_vga *vga,
                                const fd2_archive *fdother,
                                fd2_field_audio *audio, int fast) {
    if (!game || !vga || !fdother || game->failure != FD2_FIELD_FAILURE_NONE)
        return FD2_SCENE_RESULT_ERROR;
    field_progress_box box = {0};
    int result = FD2_SCENE_RESULT_OK;
    for (;;) {
        result=play_field_terminal_result(game,vga);
        if(result!=FD2_SCENE_RESULT_OK)break;
        if (game->event_resume.active) {
            if (box.page) {result=FD2_SCENE_RESULT_ERROR;break;}
            result=play_common_events(game,vga,fdother,audio,fast);
            if (result!=FD2_SCENE_RESULT_OK) break;
        }
        result = play_field_loot(game, vga, fdother, audio, fast);
        if (result != FD2_SCENE_RESULT_OK) break;
        if (game->event_resume.active) continue;
        result = field_event_diagnostics_ready(game);
        if (result != FD2_SCENE_RESULT_OK) break;
        result=play_field_terminal_result(game,vga);
        if(result!=FD2_SCENE_RESULT_OK)break;
        int resumed = fd2_field_game_progress_resume(game);
        if (resumed < 0) { result = FD2_SCENE_RESULT_ERROR; break; }
        result=play_field_terminal_result(game,vga);
        if(result!=FD2_SCENE_RESULT_OK)break;
        fd2_field_progress_notice notice;
        int pending = fd2_field_game_progress_peek(game, &notice);
        if (pending < 0) { result = FD2_SCENE_RESULT_ERROR; break; }
        if (!pending) {
            if (resumed > 0) continue;
            if (box.page) result = FD2_SCENE_RESULT_ERROR;
            break;
        }
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            break;
        }
        if (!box.page) {
            if (notice.kind != FD2_FIELD_PROGRESS_GAIN && notice.kind != FD2_FIELD_PROGRESS_STATUS) {
                result = FD2_SCENE_RESULT_ERROR;
                break;
            }
            if (progress_resources_preflight(game, &notice, (void *)fdother) != 0) {
                result = FD2_SCENE_RESULT_ERROR;
                break;
            }
            if (notice.kind == FD2_FIELD_PROGRESS_GAIN) fd2_input_clear_keys(&vga->input);
            if(fd2_input_take_quit(&vga->input)){result=FD2_SCENE_RESULT_HOST_QUIT;break;}
            fd2_field_game_render(game, vga);
            result = progress_box_open(&box, game, vga, fdother, notice.actor_index, fast);
            if (result != FD2_SCENE_RESULT_OK) break;
        }
        printf("field progress: kind=%d actor=%zu fragment=%u value=%u magic=%u\n",
            (int)notice.kind, notice.actor_index, (unsigned)notice.fragment,
            (unsigned)notice.value, (unsigned)notice.magic_id);
        fflush(stdout);
        if (notice.kind == FD2_FIELD_PROGRESS_CLOSE) {
            result = progress_timed_wait(vga, notice.wait_ticks, fast);
        } else {
            if (notice.kind == FD2_FIELD_PROGRESS_GROWTH && box.row == 3) {
                result = progress_scroll(vga, (unsigned)box.portrait_origin);
                if (result != FD2_SCENE_RESULT_OK) break;
                box.row = 2;
            }
            /* 1e529先scroll后1e56d clear；LEVEL为1e41f。 */
            if(notice.kind==FD2_FIELD_PROGRESS_LEVEL || notice.kind==FD2_FIELD_PROGRESS_GROWTH)
                fd2_input_clear_keys(&vga->input);
            if(fd2_input_take_quit(&vga->input)){result=FD2_SCENE_RESULT_HOST_QUIT;break;}
            result = progress_text_play(&box, game, vga, fdother, audio, &notice, fast);
            if (result != FD2_SCENE_RESULT_OK) break;
            if (notice.kind == FD2_FIELD_PROGRESS_GAIN) progress_portrait(&box, vga, 0, 0);
            if (notice.kind == FD2_FIELD_PROGRESS_STATUS) {
                /* 1a928/1a9fb：文本完成后caller清键，再调用1e5c0。 */
                fd2_input_clear_keys(&vga->input);
                result = progress_timed_wait(vga, notice.wait_ticks, fast);
            }
        }
        if (result != FD2_SCENE_RESULT_OK) break;
        if (notice.kind == FD2_FIELD_PROGRESS_STATUS || notice.kind == FD2_FIELD_PROGRESS_CLOSE) {
            result = progress_box_close(&box,game,vga,fast);
            if (result != FD2_SCENE_RESULT_OK) break;
        }
        if (fd2_input_take_quit(&vga->input)) {
            result = FD2_SCENE_RESULT_HOST_QUIT;
            break;
        }
        if (fd2_field_game_progress_ack(game) != 0) {
            result = FD2_SCENE_RESULT_ERROR;
            break;
        }
        result=play_field_terminal_result(game,vga);
        if(result!=FD2_SCENE_RESULT_OK)break;
        if (notice.kind == FD2_FIELD_PROGRESS_GROWTH) box.row++;
    }
    progress_box_free(&box);
    return result;
}

static void report_field_events(const fd2_field_game *game,
                                size_t *reported_count,
                                uint32_t *reported_dropped) {
    if (!game || !reported_count || !reported_dropped) return;
    while (*reported_count < game->event_log_count) {
        const fd2_field_event_notice *notice =
            &game->event_log[*reported_count];
        const char *status = notice->handled
            ? (notice->presentation_deferred
                   ? "handled, presentation deferred"
                   : "handled")
            : "unhandled, stopped";
        if (notice->kind == FD2_FIELD_EVENT_TURN) {
            printf("field event: turn=%u phase=%u slot=%zu action=%u %s\n",
                   notice->turn_number, notice->phase, notice->slot,
                   notice->action, status);
        } else {
            printf("field event: cell=(%u,%u) unit=%u slot=%zu code=%u %s\n",
                   notice->x, notice->y, notice->unit_index,
                   notice->slot, notice->action, status);
        }
        (*reported_count)++;
    }
    if (*reported_dropped != game->dropped_event_count) {
        uint32_t delta = game->dropped_event_count - *reported_dropped;
        printf("field event: %u diagnostic record(s) dropped from bounded log\n", delta);
        *reported_dropped = game->dropped_event_count;
    }
    if (game->event_log_count != 0 || game->dropped_event_count != 0)
        fflush(stdout);
}

static void report_session_failure(const fd2_field_game *game) {
    if (game->failure == FD2_FIELD_FAILURE_NONE) return;
    const fd2_field_event_notice *notice = &game->failure_notice;
    fprintf(stderr,
            "field session stopped: reason=%d stage=%zu turn=%u phase=%u "
            "actor=%u slot=%zu action=%u cell=(%u,%u); committed state retained\n",
            (int)game->failure, game->stage, notice->turn_number,
            notice->phase, notice->unit_index, notice->slot,
            notice->action, notice->x, notice->y);
}

fd2_field_play_result fd2_field_play_run(fd2_vga *vga,
                       const fd2_archive *fdother,
                       size_t stage,
                       int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio,
                       const char *save_path,
                       int load_active) {
    return fd2_field_play_run_context(vga, fdother, stage, once, handoff,
        field_audio, save_path, load_active, NULL);
}

/* App仅解释胜利流程的线性owner前缀；资源预检/切曲的顺序和曲目均是冷数据。 */
static int begin_victory_flow(fd2_field_game *game,fd2_vga *vga,fd2_field_play_context *context) {
    char id[64];
    int written = snprintf(id, sizeof(id), "stage.victory.%03zu", game->stage);
    if (written < 0 || (size_t)written >= sizeof(id)) return -1;
    const fd2_static_flow *flow = fd2_field_stage_flow_find(id);
    /* main @0x25bf4 的25e0b片段：先stop再进胜利caller。来源图只消费一次，
     * 不能用旧settlement投影提前提交对白后缀或再同步一次持久队伍。 */
    if (fd2_field_stage_source_is_bound(game->stage, "victory"))
        flow = fd2_field_stage_flow_find("stage.victory.source.prefix");
    int pc = flow ? flow->entry : -1;
    int music_requested = 0;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, pc);
        if (!node) return -1;
        if (!strcmp(node->op, "preflight_before_music")) {
            if (fd2_scene_stage_victory_validate(game) != 0) return -1;
        } else if (!strcmp(node->op, "victory_music")) {
            if (fd2_field_play_request_music(vga,context, game->stage, node->args[0], "victory") != 0)
                return -1;
            music_requested = 1;
        } else return music_requested ? 0 : -1;
        pc = fd2_static_flow_edge(flow, node, FD2_FLOW_NEXT);
    }
    return -1;
}

/* main@25bf4的53BF7跨Game/标题仍活着；只交还roster/count，不激活准备
 * 或用旧stage/gold/options覆盖已完成的结算。真实未建立的Game不交接。 */
static void play_roster_return(const fd2_field_game *game,fd2_field_play_context *context) {
    if(context && game->roster_count<=FD2_SAVE_BATTLE_MAX_ROSTER) {
        memcpy(context->preparation.unit_area,game->roster,sizeof(game->roster));
        context->preparation.unit_count=(uint8_t)game->roster_count;
        context->music_enabled=game->option_values[0];
        context->sfx_enabled=game->option_values[1];
        context->effect_option=game->option_values[2];
        context->camera_option=game->option_values[3];
    }
}
static void play_game_close(fd2_field_game *game,fd2_field_play_context *context,int roster_owned) {
    if(roster_owned)play_roster_return(game,context);
    fd2_field_game_close(game);
}

/* Scene／modal 结果映射为 Play 返回；调用者已排除 OK。 */
static fd2_field_play_result play_result_from_scene(int scene_result) {
    return scene_result == FD2_SCENE_RESULT_HOST_QUIT
        ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR;
}

/* 本关 primary 曲请求；stop_label 非 NULL 时先请求停止（CONTINUE）。
 * 曲目表缺项即失败，且不发出任何请求。 */
static int play_request_stage_music(fd2_vga *vga, fd2_field_play_context *context,
        size_t stage, const char *stop_label, const char *label) {
    const fd2_field_stage_music *music = fd2_field_stage_music_get(stage);
    if (!music) return -1;
    if (stop_label && fd2_field_play_request_music(vga, context, stage, -1, stop_label) != 0)
        return -1;
    return fd2_field_play_request_music(vga, context, stage, music->primary, label);
}

/* sub_10010 @0x10010：51A83=0→11cac(1)→共同Round及其11cac(0)/200ms尾
 * →index0→51A83=1；不能提前露cursor。CONTINUE入口与活动Load共用。
 * 返回0成功，>0宿主退出，<0错误。 */
static int play_active_load_round(fd2_field_game *game, fd2_vga *vga,
        field_runtime_presentation *runtime) {
    game->ordinary_display_mode = 0;
    int round_result = fd2_field_game_runtime_refresh(game, vga, 3);
    if (!round_result)
        round_result = play_runtime_step(game, FD2_FIELD_RUNTIME_ROUND, 0, runtime);
    if (round_result) return round_result;
    game->focus_cycle_unit = 0;
    game->ordinary_display_mode = 1;
    return 0;
}

/* 22ef6/22f37：本关整段对白/脚本成功后才交接下一准备owner。 */
static void play_store_preparation(fd2_field_play_context *context,
        const fd2_save_manual_state *prepared) {
    context->preparation = *prepared;
    context->has_preparation = 1;
    printf("field settlement: stage=%u persistent=%u resource=%u\n",
        prepared->stage_id, prepared->unit_count, prepared->gold_or_flags);
    fflush(stdout);
}

/* 正式战场主循环的一次运行状态；各输入分派借同一 Game／App owner。 */
typedef struct {
    fd2_field_game *game;
    fd2_vga *vga;
    fd2_field_audio *audio;
    fd2_field_play_context *context;
    field_runtime_presentation *runtime;
    field_progress_box *system_box;
    const fd2_battle_animation *battle;
    const char *save_path;
    int once;
    size_t reported_event_count;
    uint32_t reported_dropped_events;
    fd2_field_play_result result;
    int running;
    int end_input_frame;
} field_play_loop;

static void loop_stop(field_play_loop *loop, fd2_field_play_result result) {
    loop->result = result;
    loop->running = 0;
}

/* 非OK的Scene结果结束主循环；ERROR时输出诊断。返回1表示已停止。 */
static int loop_scene_stopped(field_play_loop *loop, int scene_result,
        const char *error_message) {
    if (scene_result == FD2_SCENE_RESULT_OK) return 0;
    loop_stop(loop, play_result_from_scene(scene_result));
    if (scene_result == FD2_SCENE_RESULT_ERROR && error_message)
        fprintf(stderr, "%s\n", error_message);
    return 1;
}

/* 系统提示失败时由runtime锁存区分HOST与错误；非HOST才输出诊断。 */
static int loop_message_stopped(field_play_loop *loop, size_t fragment,
        const char *error_message) {
    if (play_system_message(loop->runtime, loop->game, loop->vga, loop->system_box,
            loop->audio, fragment, loop->once) == 0) return 0;
    if (!loop->runtime->host_quit && error_message)
        fprintf(stderr, "%s\n", error_message);
    loop_stop(loop, loop->runtime->host_quit
        ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR);
    return 1;
}

/* 终局／宿主退出门：错误优先，其余为HOST。返回1时已结束循环。 */
static int loop_terminal_stopped(field_play_loop *loop, int include_runtime_quit) {
    int terminal = play_field_terminal_result(loop->game, loop->vga);
    if (terminal == FD2_SCENE_RESULT_OK &&
        !(include_runtime_quit && loop->runtime->host_quit)) return 0;
    loop_stop(loop, terminal == FD2_SCENE_RESULT_ERROR
        ? FD2_FIELD_PLAY_RETURN_ERROR : FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    return 1;
}

static int loop_session_failed(const field_play_loop *loop, int include_music) {
    return loop->battle->failed || loop->game->failure != FD2_FIELD_FAILURE_NONE ||
        (include_music && loop->context && loop->context->music_error);
}

static void loop_session_stop(field_play_loop *loop) {
    report_session_failure(loop->game);
    loop_stop(loop, FD2_FIELD_PLAY_RETURN_ERROR);
}

static fd2_field_command_direction field_action_direction(fd2_field_action action) {
    return action == FD2_FIELD_ACTION_UP ? FD2_FIELD_COMMAND_DIRECTION_UP
        : action == FD2_FIELD_ACTION_DOWN ? FD2_FIELD_COMMAND_DIRECTION_DOWN
        : action == FD2_FIELD_ACTION_LEFT ? FD2_FIELD_COMMAND_DIRECTION_LEFT
        : FD2_FIELD_COMMAND_DIRECTION_RIGHT;
}

/* 方向键：系统菜单／指令菜单选向，其余移动光标。 */
static void play_direction(fd2_field_game *game, fd2_field_action action) {
    fd2_field_command_direction direction = field_action_direction(action);
    if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU)
        (void)fd2_field_game_select_system_menu_direction(game, direction);
    else if (game->interaction == FD2_FIELD_INTERACTION_COMMAND)
        (void)fd2_field_game_select_command_direction(game, direction);
    else
        fd2_field_game_move_cursor(game,
            action == FD2_FIELD_ACTION_LEFT ? -1 : action == FD2_FIELD_ACTION_RIGHT ? 1 : 0,
            action == FD2_FIELD_ACTION_UP ? -1 : action == FD2_FIELD_ACTION_DOWN ? 1 : 0);
}

static void play_cancel(field_play_loop *loop) {
    fd2_field_game *game = loop->game;
    fd2_vga *vga = loop->vga;
    fd2_field_audio *audio = loop->audio;
    if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU) {
        fd2_field_system_page page = game->system_menu.page;
        fd2_field_system_action cancelled_origin = game->system_menu.confirmation_action;
        /* Secondary/options cancel first closes the current radial page,
         * then parent page opens again. Parent cancel closes the menu
         * before returning to browse. */
        if (loop_scene_stopped(loop, play_system_menu_transition(game, vga, audio, 0), NULL))
            return;
        (void)fd2_field_game_cancel_system_menu(game);
        /* 只结束回到浏览的帧；新opening中采到的键仍归新菜单。 */
        loop->end_input_frame = game->interaction != FD2_FIELD_INTERACTION_SYSTEM_MENU;
        if (page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION &&
            loop_message_stopped(loop, FD2_FIELD_SYSTEM_TEXT_CANCELLED, "取消提示资源无效。"))
            return;
        if (page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION &&
            (cancelled_origin == FD2_FIELD_SYSTEM_ACTION_SAVE ||
             cancelled_origin == FD2_FIELD_SYSTEM_ACTION_LOAD))
            fd2_input_clear_keys(&vga->input); /* 19df7反馈412→LABEL24。 */
        if (page != FD2_FIELD_SYSTEM_PAGE_PARENT &&
            game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU)
            (void)loop_scene_stopped(loop, play_system_menu_transition(game, vga, audio, 1), NULL);
    } else if (game->interaction == FD2_FIELD_INTERACTION_AUXILIARY) {
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    } else if (game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED ||
               game->interaction == FD2_FIELD_INTERACTION_MOVING) {
        (void)fd2_field_game_cancel_selection(game);
        loop->end_input_frame = 1;
    } else if (game->interaction == FD2_FIELD_INTERACTION_COMMAND) {
        int result = play_command_transition(game, vga, audio, 0);
        if (result == FD2_SCENE_RESULT_OK && !fd2_field_game_cancel_selection(game))
            result = FD2_SCENE_RESULT_ERROR;
        loop->end_input_frame = 1;
        (void)loop_scene_stopped(loop, result, NULL);
    } else if (game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU ||
               game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
               game->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING ||
               game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING) {
        int result = game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU
            ? play_field_item_menu_cancel(game, vga, audio)
            : game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING
                ? fd2_field_game_cancel_item_give(game) == 0
                    ? play_command_transition(game, vga, audio, 1)
                    : FD2_SCENE_RESULT_ERROR
            : fd2_field_game_cancel_item_targeting(game) == 0
                ? play_field_item_selection(game, vga, audio)
                : FD2_SCENE_RESULT_ERROR;
        loop->end_input_frame = 1;
        (void)loop_scene_stopped(loop, result, "物品取消或列表重开失败。");
    } else if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
               game->interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING) {
        int result = fd2_field_game_cancel_magic_targeting(game) == 0
            ? play_field_magic_selection(game, vga, audio)
            : FD2_SCENE_RESULT_ERROR;
        loop->end_input_frame = 1;
        (void)loop_scene_stopped(loop, result, "法术目标取消或列表重开失败。");
    } else if (game->interaction == FD2_FIELD_INTERACTION_TARGETING &&
               fd2_field_game_cancel_attack(game)) {
        /* field_player_command_execute_core @0x18d8c：
         * target selector 返回 -1 后重新进入菜单循环。 */
        (void)loop_scene_stopped(loop, play_command_transition(game, vga, audio, 1), NULL);
    }
}

static void play_detail_key(field_play_loop *loop) {
    fd2_field_game *game = loop->game;
    int unit_at = fd2_field_game_unit_at(game, game->cursor_cell_x, game->cursor_cell_y);
    if (game->interaction != FD2_FIELD_INTERACTION_BROWSE || unit_at < 0 ||
        game->units.items[unit_at].unit_id == 0x79 ||
        game->units.items[unit_at].race == 0x0a) return;
    int detail_result = play_field_detail_present(game, loop->vga, loop->audio, (size_t)unit_at);
    loop->end_input_frame = 1;
    (void)loop_scene_stopped(loop, detail_result, "单位详情页资源无效。");
}

/* field_controller_input @0x117e7（原登记同名）的F1/PageUp直接调用
 * sub_2000A，不走玩家动作后置。 */
static void play_auxiliary_key(field_play_loop *loop) {
    if (loop->game->interaction != FD2_FIELD_INTERACTION_BROWSE) return;
    int auxiliary_result = play_field_auxiliary(loop->game, loop->vga);
    loop->end_input_frame = 1;
    (void)loop_scene_stopped(loop, auxiliary_result, NULL);
}

static void play_focus_cycle(field_play_loop *loop) {
    fd2_field_game *game = loop->game;
    if (game->interaction == FD2_FIELD_INTERACTION_TARGETING ||
        game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
        game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
        game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING) {
        int result = game->interaction == FD2_FIELD_INTERACTION_TARGETING
            ? fd2_field_game_cycle_attack_target(game)
            : game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING
            ? fd2_field_game_cycle_item_give_target(game)
            : game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING
            ? fd2_field_game_cycle_item_target(game)
            : fd2_field_game_cycle_magic_target(game);
        if (result < 0 || game->failure != FD2_FIELD_FAILURE_NONE) {
            fprintf(stderr, "目标轮换状态无效。\n");
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_ERROR);
        } else if (game->runtime_host_quit) {
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_HOST_QUIT);
        }
    } else if (game->interaction == FD2_FIELD_INTERACTION_BROWSE) {
        int unit = fd2_field_game_cycle_focus(game);
        if (game->failure != FD2_FIELD_FAILURE_NONE) {
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_ERROR);
            return;
        }
        if (game->runtime_host_quit) {
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_HOST_QUIT);
            return;
        }
        fd2_input_clear_keys(&loop->vga->input); /* 1187a；G保余队。 */
        printf("field focus: unit=%d, cell=(%d,%d), interaction=browse\n",
               unit, game->cursor_cell_x, game->cursor_cell_y);
        fflush(stdout);
    }
}

/* code0 0x1884：只有可行动的 side 2 普通单位进入移动／操作路径；
 * 敌方、已行动单位和特殊记录先走原版的单位详情 helper。
 * 返回1表示本次确认已由详情路径处理。 */
static int play_confirm_detail_gate(field_play_loop *loop, int unit_at) {
    fd2_field_game *game = loop->game;
    if (game->interaction != FD2_FIELD_INTERACTION_BROWSE || unit_at < 0) return 0;
    const fd2_field_unit *focused = &game->units.items[unit_at];
    const uint8_t *record = (const uint8_t *)focused;
    if (focused->unit_id == 0x79 || focused->race == 0x0a ||
        (focused->side == 2u && !fd2_field_unit_has_acted(focused) && record[0x26] == 0))
        return 0;
    if (focused->side == 2u && fd2_field_unit_has_acted(focused)) {
        printf("field acted gate: unit=%d, interaction=browse\n", unit_at);
        fflush(stdout);
    }
    int detail_result = fd2_field_game_begin_player_controller(game, (size_t)unit_at) != 0 ? -1 :
        play_field_detail_present(game, loop->vga, loop->audio, (size_t)unit_at);
    loop->end_input_frame = 1;
    if (loop_scene_stopped(loop, detail_result, "单位详情页资源无效。")) return 1;
    /* 17aed整段返回后才执行118ee的controller尾；F2直接caller不经过此写点。 */
    if (fd2_field_game_complete_player_controller(game, (size_t)unit_at) != 0)
        loop_session_stop(loop);
    return 1;
}

/* secondary dispatcher @code0 0x9df7 操作活动 snapshot；四槽 hand_save/
 * hand_load 属于独立 dispatcher @code0 0x19300，不能在此替换。
 * 返回1表示原分派在此中止（不执行确认尾的清键／结束帧）。 */
static int play_storage_action(field_play_loop *loop, fd2_field_system_action system_action) {
    fd2_field_game *game = loop->game;
    fd2_vga *vga = loop->vga;
    size_t fragment = 0;
    if (system_action == FD2_FIELD_SYSTEM_ACTION_LOAD) {
        /* 19df7原414/200ms/196cb先于stop及10010；
         * Load不得先换session，再用新scene伪造旧提示。 */
        int feedback = play_system_message(loop->runtime, game, vga, loop->system_box,
            loop->audio, FD2_FIELD_SYSTEM_TEXT_LOAD_ACCEPTED, loop->once);
        if (feedback || (loop->context && fd2_field_play_request_music(vga,
                loop->context, game->stage, -1, "active-load-stop") != 0)) {
            loop_stop(loop, feedback == FD2_SCENE_RESULT_HOST_QUIT
                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR);
            return 1;
        }
    }
    int success = fd2_field_game_execute_storage_action(
        game, system_action, loop->save_path, &fragment) == 1;
    if (success && system_action == FD2_FIELD_SYSTEM_ACTION_LOAD) {
        if (fd2_field_game_bind_visual(game, vga) != 0) {
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_ERROR);
            return 1;
        }
        game->runtime_present = play_runtime_step;
        game->runtime_present_userdata = loop->runtime;
        loop->reported_event_count = 0;
        loop->reported_dropped_events = 0;
        apply_field_options(game, vga, loop->audio, loop->context, 0);
        if (fd2_bios_clock_error() < 0 || fd2_vga_output_error(vga) < 0 ||
            (loop->context && play_request_stage_music(vga, loop->context,
                game->stage, NULL, "active-load") != 0)) {
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_ERROR);
            return 1;
        }
        /* 10010的活动dispatcher caller同源mode0/1跨度。 */
        int round_result = play_active_load_round(game, vga, loop->runtime);
        if (round_result) {
            loop_stop(loop, round_result > 0
                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR);
            return 1;
        }
    }
    if (system_action == FD2_FIELD_SYSTEM_ACTION_LOAD) fragment = 0;
    if (fragment != 0) {
        /* 存储动作已返回；呈现失败不能回滚或把已写的Save误报成未写。
         * Load成功也已替换session。 */
        char message[96];
        snprintf(message, sizeof(message), "%s，但结果提示资源无效。",
            system_action == FD2_FIELD_SYSTEM_ACTION_SAVE
                ? (success ? "活动存档已写入" : "活动存档返回失败")
                : (success ? "活动读档已完成" : "活动读档返回失败"));
        if (loop_message_stopped(loop, fragment, message)) return 1;
    }
    if (!success)
        fprintf(stderr, "field %s failed: %s\n",
                system_action == FD2_FIELD_SYSTEM_ACTION_SAVE ? "save" : "load",
                loop->save_path ? loop->save_path : "(null)");
    if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU &&
        loop_scene_stopped(loop, play_system_menu_transition(game, vga, loop->audio, 1), NULL))
        return 1;
    return 0;
}

/* LEAVE/MARCH/END_TURN的受理提示：提示完成后再核宿主退出。
 * 返回1表示已结束循环。 */
static int loop_accept_message_stopped(field_play_loop *loop, size_t fragment) {
    if (loop_message_stopped(loop, fragment, NULL)) return 1;
    if (!fd2_input_take_quit(&loop->vga->input)) return 0;
    loop_stop(loop, FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    return 1;
}

/* open/input/close primitive 的原版边界：确认后先按当前 command IDs
 * 收起，再进入下一 dispatcher。confirmation page 独立绘制 FDTXT 与 YES/NO。 */
static void play_system_menu_confirm(field_play_loop *loop) {
    fd2_field_game *game = loop->game;
    fd2_vga *vga = loop->vga;
    fd2_field_system_action confirmation_origin =
        game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION
            ? game->system_menu.confirmation_action : FD2_FIELD_SYSTEM_ACTION_NONE;
    int result = play_system_menu_transition(game, vga, loop->audio, 0);
    if (result != FD2_SCENE_RESULT_OK) {
        loop_stop(loop, play_result_from_scene(result));
        if (game->failure != FD2_FIELD_FAILURE_NONE) report_session_failure(game);
        return;
    }
    fd2_field_system_action system_action = fd2_field_game_confirm_system_menu(game);
    if (system_action == FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING) {
        apply_field_options(game, vga, loop->audio, loop->context,
                            game->system_menu.selected == 0);
        if (fd2_bios_clock_error() < 0 || fd2_vga_output_error(vga) < 0) {
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_ERROR);
            return;
        }
        unsigned index = game->system_menu.selected;
        printf("field option: index=%u value=%u\n", index, game->option_values[index]);
        fflush(stdout);
    }
    if (system_action == FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION) {
        confirmation_origin = game->system_menu.confirmation_action;
        size_t fragment = 0;
        if (fd2_field_system_menu_get_confirmation_fragment(&game->system_menu,
                FD2_FIELD_SYSTEM_CONFIRM_PROMPT, &fragment) != 0) {
            if (!loop->runtime->host_quit) fprintf(stderr, "系统确认提示资源无效。\n");
            loop_stop(loop, loop->runtime->host_quit
                ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR);
            return;
        }
        if (loop_message_stopped(loop, fragment, "系统确认提示资源无效。")) return;
        fd2_field_loot_reply reply = FD2_FIELD_LOOT_CANCEL;
        if (loop_scene_stopped(loop, play_system_choice(game, vga, loop->system_box, &reply), NULL))
            return;
        game->system_menu.confirmation_yes = (uint8_t)(reply == FD2_FIELD_LOOT_YES);
        system_action = reply == FD2_FIELD_LOOT_CANCEL
            ? fd2_field_game_cancel_system_menu(game)
            : fd2_field_game_confirm_system_menu(game);
    }
    if (system_action == FD2_FIELD_SYSTEM_ACTION_SCREEN_TRANSITION) {
        int overview_result = play_field_overview(game, vga);
        loop->end_input_frame = 1;
        (void)loop_scene_stopped(loop, overview_result, NULL);
    } else if (system_action == FD2_FIELD_SYSTEM_ACTION_SAVE ||
               system_action == FD2_FIELD_SYSTEM_ACTION_LOAD) {
        if (play_storage_action(loop, system_action)) return;
    } else if (system_action == FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE) {
        /* field_secondary_menu_execute @0x19df7：
         * FDTXT[416] 后返回 -1，main 最终退出游戏。 */
        if (loop_accept_message_stopped(loop, FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED)) return;
        puts("field leave: exiting game");
        fflush(stdout);
        loop_stop(loop, FD2_FIELD_PLAY_RETURN_LEAVE_GAME);
    } else if (system_action == FD2_FIELD_SYSTEM_ACTION_MARCH_CONFIRM) {
        /* @0x16f55：418提示在固定anchor和逐actor提交之前。 */
        if (loop_accept_message_stopped(loop, FD2_FIELD_SYSTEM_TEXT_MARCH_ACCEPTED)) return;
        printf("field march: anchor=(%d,%d) turn=%u phase=%u\n",
               game->cursor_cell_x, game->cursor_cell_y,
               game->turn_number, (unsigned)game->active_side);
        fflush(stdout);
        vga->info_panel_suppressed = 1; /* 170ba..17163，含整批移动。 */
        int march_result = fd2_field_game_march(game, vga, 55);
        loop->end_input_frame = 1;
        if (march_result == 1)
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_HOST_QUIT);
        else if (march_result != 0)
            loop_session_stop(loop);
    } else if (system_action == FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM) {
        /* field_empty_focus_menu_execute @0x16f55：
         * FDTXT[420] 提示先于 field_turn_cycle_run。 */
        if (loop_accept_message_stopped(loop, FD2_FIELD_SYSTEM_TEXT_END_TURN_ACCEPTED)) return;
        printf("field end turn: turn=%u phase=%u remaining=%zu\n",
               game->turn_number, game->active_side,
               fd2_field_game_remaining_units(game, 2));
        fflush(stdout);
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
        vga->info_panel_suppressed = 1; /* 1726b..17277。 */
        if (fd2_field_game_end_player_turn(game) != 0) loop_session_stop(loop);
    } else if (system_action == FD2_FIELD_SYSTEM_ACTION_BACK &&
               game->interaction == FD2_FIELD_INTERACTION_BROWSE) {
        (void)loop_message_stopped(loop, FD2_FIELD_SYSTEM_TEXT_CANCELLED, "取消提示资源无效。");
    } else if (loop_scene_stopped(loop, play_system_menu_transition(game, vga, loop->audio, 1), NULL)) {
        return;
    }
    /* 结束本轮已消费键的分派；只有19df7 Save/Load有源清队，
     * 其它结果保后续独立缓存，不把宿主屏障当4e381。 */
    if (game->interaction == FD2_FIELD_INTERACTION_BROWSE) {
        if (confirmation_origin == FD2_FIELD_SYSTEM_ACTION_SAVE ||
            confirmation_origin == FD2_FIELD_SYSTEM_ACTION_LOAD)
            fd2_input_clear_keys(&vga->input); /* 19df7 LABEL24，不是任意BACK。 */
        loop->end_input_frame = 1;
    }
}

/* 普通确认：菜单收起、光标确认、后续移动／法术／物品编排及诊断输出。 */
static void play_confirm_cursor(field_play_loop *loop, int unit_at,
        fd2_field_interaction previous_interaction) {
    fd2_field_game *game = loop->game;
    fd2_vga *vga = loop->vga;
    fd2_field_audio *audio = loop->audio;
    if (previous_interaction == FD2_FIELD_INTERACTION_ITEM_MENU ||
        (previous_interaction == FD2_FIELD_INTERACTION_COMMAND &&
         game->command_selected == FD2_FIELD_COMMAND_ITEM)) {
        int result = play_command_transition(game, vga, audio, 0);
        if (loop_scene_stopped(loop, result, "物品菜单收起失败。")) {
            loop->end_input_frame = 1;
            return;
        }
    } else if (previous_interaction == FD2_FIELD_INTERACTION_COMMAND) {
        int result = play_command_transition(game, vga, audio, 0);
        loop->end_input_frame = 1;
        if (loop_scene_stopped(loop, result, NULL)) return;
    }
    int action_unit = game->selected_unit;
    int action_command = game->command_selected;
    uint16_t attack_hp_before =
        previous_interaction == FD2_FIELD_INTERACTION_TARGETING &&
            unit_at >= 0 && (size_t)unit_at < game->units.count
        ? game->units.items[unit_at].hp : 0;
    int unit_index = previous_interaction == FD2_FIELD_INTERACTION_BROWSE && unit_at < 0
        ? fd2_field_game_open_system_menu(game)
        : fd2_field_game_confirm_cursor(game);
    if (loop_session_failed(loop, 0)) {
        loop_session_stop(loop);
        return;
    }
    if (previous_interaction == FD2_FIELD_INTERACTION_BROWSE &&
        game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED) {
        int result = play_field_move_selection_wait_key(game, vga);
        if (result == FD2_SCENE_RESULT_ERROR) report_session_failure(game);
        if (loop_scene_stopped(loop, result, "单位初选状态板或等待失败。")) return;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU &&
        previous_interaction == FD2_FIELD_INTERACTION_BROWSE &&
        loop_scene_stopped(loop, play_system_menu_transition(game, vga, audio, 1), NULL))
        return;
    if (game->interaction == FD2_FIELD_INTERACTION_MOVING) {
        int moved = fd2_field_game_execute_move(game, vga, 55);
        if (moved < 0 || game->failure != FD2_FIELD_FAILURE_NONE) {
            loop_session_stop(loop);
            fprintf(stderr, "field move execution failed\n");
            return;
        }
        if (moved > 0 || game->runtime_host_quit) {
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_HOST_QUIT);
            return;
        }
        if (loop_scene_stopped(loop, play_command_transition(game, vga, audio, 1), NULL)) return;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_LIST) {
        int result = play_field_magic_selection(game, vga, audio);
        loop->end_input_frame = 1;
        if (loop_scene_stopped(loop, result, "法术列表或目标选择资源无效。")) return;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU ||
        game->interaction == FD2_FIELD_INTERACTION_ITEM_LIST ||
        game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_LIST ||
        (game->interaction == FD2_FIELD_INTERACTION_COMMAND && unit_index >= 0 &&
         previous_interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING)) {
        int result = (game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU ||
                      game->interaction == FD2_FIELD_INTERACTION_COMMAND)
            ? play_command_transition(game, vga, audio, 1)
            : game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_LIST
                ? play_field_item_give_selection(game, vga, audio)
            : play_field_item_selection(game, vga, audio);
        loop->end_input_frame = 1;
        if (loop_scene_stopped(loop, result, "物品菜单、列表或目标选择资源无效。")) return;
    }
    if (action_unit >= 0 && (size_t)action_unit < game->units.count &&
        (previous_interaction == FD2_FIELD_INTERACTION_COMMAND ||
         previous_interaction == FD2_FIELD_INTERACTION_TARGETING ||
         previous_interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
         previous_interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING ||
         previous_interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
         previous_interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING) &&
        fd2_field_unit_has_acted(&game->units.items[action_unit]) &&
        game->selected_unit == -1) {
        loop->end_input_frame = 1;
        printf("field action: unit=%d command=%s acted=1 remaining=%zu phase=%u\n",
               action_unit, field_action_command_name(previous_interaction, action_command),
               fd2_field_game_remaining_units(game, 2u), (unsigned)game->active_side);
    }
    /* AI结果不属于本次玩家确认，不能用本帧HP计算其累计伤害。 */
    if (game->last_attack_valid && previous_interaction == FD2_FIELD_INTERACTION_TARGETING) {
        printf("field attack: target=%d strikes=%u hit=%u critical=%u base=%u rolled=%u damage=%u total=%u hp=%u defeated=%u hidden=%u\n",
               game->attack_target,
               (unsigned)game->last_attack_strikes,
               game->last_attack.hit,
               game->last_attack.critical,
               game->last_attack.base_damage,
               game->last_attack.rolled_damage,
               game->last_attack.damage,
               (unsigned)(attack_hp_before - game->last_attack.hp_after),
               game->last_attack.hp_after,
               game->last_attack.defeated,
               game->attack_target >= 0 && (size_t)game->attack_target < game->units.count
                   ? (unsigned)fd2_field_unit_is_hidden(&game->units.items[game->attack_target])
                   : 0u);
    }
    game->last_attack_valid = 0;
    uint16_t terrain_id = fd2_field_cell_terrain(
        fd2_field_map_cell(&game->map, game->cursor_cell_x, game->cursor_cell_y));
    printf("field cursor: cell=(%d,%d), terrain=%u, "
           "unit=%d, selected=%d, interaction=%d, "
           "camera=(%d,%d), camera-offset=(%d,%d)\n",
           game->cursor_cell_x, game->cursor_cell_y,
           (unsigned)terrain_id, unit_at, unit_index,
           (int)game->interaction,
           game->camera_cell_x, game->camera_cell_y,
           game->camera_pixel_offset_x, game->camera_pixel_offset_y);
    fflush(stdout);
}

static void play_confirm(field_play_loop *loop) {
    fd2_field_game *game = loop->game;
    int unit_at = fd2_field_game_unit_at(game, game->cursor_cell_x, game->cursor_cell_y);
    if (play_confirm_detail_gate(loop, unit_at)) return;
    game->detail_acknowledged_unit = -1;
    fd2_field_interaction previous_interaction = game->interaction;
    if (previous_interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU) {
        play_system_menu_confirm(loop);
        return;
    }
    if (previous_interaction == FD2_FIELD_INTERACTION_AUXILIARY) {
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
        return;
    }
    play_confirm_cursor(loop, unit_at, previous_interaction);
}

/* 一次已读输入的分派；详情页与非玩家阶段的门在调用者处理。 */
static void play_dispatch_action(field_play_loop *loop, fd2_field_action action) {
    switch (action) {
        case FD2_FIELD_ACTION_EXIT:
            loop_stop(loop, FD2_FIELD_PLAY_RETURN_HOST_QUIT);
            break;
        case FD2_FIELD_ACTION_CANCEL: play_cancel(loop); break;
        case FD2_FIELD_ACTION_DETAIL: play_detail_key(loop); break;
        case FD2_FIELD_ACTION_AUXILIARY: play_auxiliary_key(loop); break;
        case FD2_FIELD_ACTION_FOCUS_CYCLE: play_focus_cycle(loop); break;
        case FD2_FIELD_ACTION_CONFIRM: play_confirm(loop); break;
        case FD2_FIELD_ACTION_LEFT:
        case FD2_FIELD_ACTION_RIGHT:
        case FD2_FIELD_ACTION_UP:
        case FD2_FIELD_ACTION_DOWN:
            play_direction(loop->game, action);
            break;
        case FD2_FIELD_ACTION_NONE:
            break;
    }
}

/* 战斗胜利／败北后的跨关交接；只在 App context 下执行。 */
static fd2_field_play_result play_finish_battle(fd2_field_game *game, fd2_vga *vga,
        const fd2_archive *fdother, fd2_field_audio *field_audio, int once,
        fd2_field_play_context *context, int *roster_owned,
        fd2_field_play_result play_result) {
    if (play_result == FD2_FIELD_PLAY_RETURN_VICTORY) {
        vga->info_panel_suppressed = 1; /* main25e0b..25e5e跨result/prepare/entry。 */
        fd2_save_manual_state prepared;
        int scene_result = FD2_SCENE_RESULT_ERROR;
        if (!fd2_field_stage_battle_is_supported(game->stage)) {
            fprintf(stderr, "当前关卡结算尚未开放，已停止跨关（stage=%zu）。\n", game->stage);
        } else if (fd2_field_stage_source_is_bound(game->stage, "victory")) {
            if (begin_victory_flow(game, vga, context) == 0) {
                scene_result = fd2_scene_play_stage_victory(vga, fdother, game, field_audio, once);
                if (scene_result == FD2_SCENE_RESULT_OK) {
                    if (fd2_field_game_export_preparation(game, &prepared) != 0)
                        scene_result = FD2_SCENE_RESULT_ERROR;
                    else
                        play_store_preparation(context, &prepared);
                }
            }
        } else if (fd2_field_game_settlement_prepare(game, &prepared) == 0 &&
            begin_victory_flow(game, vga, context) == 0) {
            scene_result = fd2_scene_play_stage_victory(vga, fdother, game, field_audio, once);
            if (scene_result == FD2_SCENE_RESULT_OK) {
                if (fd2_field_game_settlement_commit(game, &prepared) != 0)
                    scene_result = FD2_SCENE_RESULT_ERROR;
                else
                    play_store_preparation(context, &prepared);
            }
        }
        if (scene_result != FD2_SCENE_RESULT_OK) play_result = play_result_from_scene(scene_result);
    } else if (play_result == FD2_FIELD_PLAY_RETURN_DEFEAT) {
        /* controller已返回；22e5c不再写persistent，先交还真实live表，
         * HOST/呈现失败也不能丢掉败北前已到达的Load/业务前缀。 */
        if (*roster_owned) {
            play_roster_return(game, context);
            *roster_owned = 0;
        }
        vga->info_panel_suppressed = 1; /* main25dde..25dea。 */
        int scene_result = fd2_scene_play_game_over(vga, fdother, context->bgm, once);
        if (scene_result == FD2_SCENE_RESULT_OK) vga->info_panel_suppressed = 0;
        else play_result = play_result_from_scene(scene_result);
    }
    return play_result;
}

fd2_field_play_result fd2_field_play_run_context(fd2_vga *vga,
                       const fd2_archive *fdother, size_t stage, int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio, const char *save_path,
                       int load_active, fd2_field_play_context *context) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0))return FD2_FIELD_PLAY_RETURN_ERROR;
    field_music_scope music_scope={context,vga};
    fd2_field_game game;
    if (!vga || !fdother || (context && !context->initialized))
        return FD2_FIELD_PLAY_RETURN_ERROR;
    int prepared_entry = context && context->has_preparation && !load_active;
    int opening_roster=context && handoff && handoff->valid && !prepared_entry && !load_active && stage==0u;
    int roster_owned=context && (prepared_entry || opening_roster);
    int opened = prepared_entry
        ? (context->preparation.stage_id != stage ? -1 :
            fd2_field_game_prepare_stage(&game, fdother, &context->preparation))
        : load_active ? fd2_field_game_open_resources(&game, fdother, stage)
        : opening_roster ? fd2_field_game_open_from_roster(&game,vga,fdother,&context->preparation)
        : fd2_field_game_open(&game, vga, fdother, stage);
    if (opened != 0) {
        fprintf(stderr, "cannot load stage %zu field game\n", stage);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    fd2_field_play_result setup_result = FD2_FIELD_PLAY_RETURN_ERROR;
    /* main @0x25bf4（原同名登记）→title_action_dispatch @0x25ebb
     * （原同名登记）的START不写四个原option byte；不从准备meta继承。 */
    if (context && !load_active && !prepared_entry) {
        game.option_values[0] = context->music_enabled;
        game.option_values[1] = context->sfx_enabled;
        game.option_values[2] = context->effect_option;
        game.option_values[3] = context->camera_option;
        fd2_field_system_menu_set_options(&game.system_menu, game.option_values);
    }
    /* 正式 App 的标题／Scene／准备／Game／Load 始终借同一 context 流。
     * 无 context 的独立战场入口才建立本地启动流；SAV 不保存 RNG。 */
    fd2_field_rng m6_rng;
    if (!context)
        (void)fd2_field_rng_bootstrap(&m6_rng, getenv("FD2SDL_FIELD_RNG_SEED"));
    fd2_field_rng *runtime_rng = context ? &context->rng : &m6_rng;
    if (fd2_field_game_set_attack_hooks(
            &game, NULL, NULL, NULL, NULL,
            fd2_field_rng_next, runtime_rng) != 0)
        goto setup_failed;
    if (context) {
        game.experience_value = context->experience_value;
        game.message_number = context->message_number;
        game.message_new_fragment = context->message_new_fragment;
        game.message_old_fragment = context->message_old_fragment;
        if (fd2_field_game_set_music_request(&game, field_phase_music, &music_scope) != 0)
            goto setup_failed;
    }
    if (handoff && fd2_field_game_apply_handoff(&game, handoff) != 0) {
        fprintf(stderr, "cannot apply stage %zu opening handoff\n", stage);
        goto setup_failed;
    }
    if (load_active && fd2_field_game_load_active(&game, save_path) != 0) {
        fprintf(stderr, "cannot import active field save: %s\n",
                save_path ? save_path : "(null)");
        goto setup_failed;
    }
    if(load_active && context)roster_owned=1;
    if (fd2_field_game_bind_visual(&game,vga)!=0) goto setup_failed;
    field_runtime_presentation runtime={vga,field_audio,once,0,NULL,context};
    game.runtime_present=play_runtime_step;
    game.runtime_present_userdata=&runtime;
    if(fd2_field_play_bind_event_preflight(&game,fdother)!=0) goto setup_failed;
    if (load_active) {
        const uint8_t *palette;
        size_t size;
        if (fd2_archive_get(fdother, 0, &palette, &size) != 0 || size < 768)
            goto setup_failed;
        /* 标题CONTINUE先恢复共用调色板，活动快照不重播205da开场。 */
        fd2_vga_set_palette(vga, palette);
        fd2_vga_set_brightness(vga, 0);
    }
    if (prepared_entry) {
        int entry_result = fd2_scene_play_stage_entry(vga, fdother, &game, field_audio, once);
        if (entry_result != FD2_SCENE_RESULT_OK) {
            setup_result = play_result_from_scene(entry_result);
            goto setup_failed;
        }
        context->has_preparation = 0;
    }
    fd2_field_game_set_save_resource_available(
        &game, fd2_field_game_save_resource_available(&game, save_path));
    apply_field_options(&game,vga, field_audio, context, 0);
    if (fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0 ||
        (context && play_request_stage_music(vga, context, game.stage,
            load_active ? "continue-stop" : NULL, load_active ? "continue" : "entry") != 0))
        goto setup_failed;
    /* 10010活动Load复用Round但不递增回合、不触发phase2/status，不强开
     * 51AAC。新entry的main跨度在primary请求之后才恢复信息板。 */
    if (load_active) {
        int round_result = play_active_load_round(&game, vga, &runtime);
        if (round_result) {
            if (round_result > 0) setup_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
            goto setup_failed;
        }
    } else vga->info_panel_suppressed=0;
    if (!once && (fd2_field_game_set_progress_preflight(
            &game, progress_resources_preflight, (void *)fdother) != 0 ||
        fd2_field_game_set_loot_preflight(
            &game, loot_resources_preflight, (void *)fdother) != 0))
        goto setup_failed;

    /* B6同步服务属于本轮Play，借用App的两个PCM handle与FDOTHER。
     * present只读业务，原11cac点经窄接口推进同一Game视觉锚；退出先解除借用再释放。 */
    fd2_battle_animation battle = {0};
    if (fd2_battle_animation_open(&battle, vga,
            field_audio ? field_audio->player : NULL,
            field_audio ? field_audio->secondary_player : NULL,
            fdother, &game.figani_archive, &game) != 0)
        goto setup_failed;
    fd2_battle_animation_set_skip(&battle, once);
    game.physical_present = fd2_battle_animation_physical_present;
    game.physical_present_userdata = &battle;
    game.magic_present = fd2_battle_animation_magic_present;
    game.magic_present_userdata = &battle;

    printf("field play: stage=%zu, map=%dx%d, units=%zu, camera=(%d,%d), "
           "cursor=(%d,%d), turn=%u, phase=%u, handoff=%s%s\n",
           game.stage, game.map.width, game.map.height, game.units.count,
           game.camera_cell_x, game.camera_cell_y,
           game.cursor_cell_x, game.cursor_cell_y,
           game.turn_number, game.active_side,
           handoff ? "yes" : "no",
           once ? ", once" : "");
    fflush(stdout);

    /* 普通Enter详情返回controller后置；F2是原版独立直接返回分支。 */
    field_progress_box system_box={0};
    field_play_loop loop = {&game, vga, field_audio, context, &runtime, &system_box,
        &battle, save_path, once, 0, 0, FD2_FIELD_PLAY_RETURN_COMPLETE, 1, 0};
    uint32_t reported_turn = game.turn_number;
    uint8_t reported_side = game.active_side;
    int input_ready_reported = 0;
    field_selector_wait selector_wait={0};
    while (loop.running) {
        if (loop_session_failed(&loop, 1)) {
            loop_session_stop(&loop);
            break;
        }
        if (loop_terminal_stopped(&loop, 1)) break;
        if (!once && game.event_resume.active) {
            int event_result=play_common_events(&game,vga,fdother,field_audio,0);
            if (event_result!=FD2_SCENE_RESULT_OK) {
                loop.result=play_result_from_scene(event_result);
                break;
            }
            input_ready_reported=0;
        }
        if (!once) {
            int had_progress = game.loot.source || game.progress.notice.kind || game.progress.resume;
            int progress_result = play_field_progress(&game, vga, fdother, field_audio, 0);
            if (progress_result != FD2_SCENE_RESULT_OK) {
                loop.result = play_result_from_scene(progress_result);
                if (progress_result == FD2_SCENE_RESULT_ERROR)
                    fprintf(stderr, "战场奖励、状态提示或必需演出失败。\n");
                break;
            }
            if (had_progress) input_ready_reported = 0; /* 仍在实际present/quit检查后重报ready。 */
        }
        if (fd2_field_game_resume_player_controller(&game) < 0) {
            loop_session_stop(&loop);
            break;
        }
        if (loop_terminal_stopped(&loop, 0)) break;
        /* main @0x25bf4：25dce的controller返回后，25dd5/25e02先读53ECC，
         * 再决定是否进入下一次controller。这里只读已锁存结果；上方已排完
         * 当前caller的剧情/奖励并恢复后置，不能再先执行一次11aa8或live页。
         * 中间latch仍须由原loot/event owner完成，错误/HOST保持先行。 */
        int continuation_pending = game.loot.source != 0u || game.event_resume.active;
        int automatic_continuation = game.loot.source != 0u &&
            game.loot.notice.kind == FD2_FIELD_LOOT_NONE;
        if (!once && !continuation_pending && game.battle_result != FD2_FIELD_BATTLE_ONGOING) {
            loop.result = game.battle_result == FD2_FIELD_BATTLE_VICTORY
                ? FD2_FIELD_PLAY_RETURN_VICTORY : FD2_FIELD_PLAY_RETURN_DEFEAT;
            printf("field battle: %s, stage=%zu\n",
                   game.battle_result == FD2_FIELD_BATTLE_VICTORY
                       ? "victory" : "defeat", game.stage);
            report_field_events(&game, &loop.reported_event_count, &loop.reported_dropped_events);
            fflush(stdout);
            break;
        }
        int idle_player_turn = !once && game.active_side==2 &&
            !game.loot.source && !game.event_resume.active && !game.progress.resume && !game.progress.notice.kind;
        int no_turn_post = game.turn_post_caller==FD2_FIELD_TURN_POST_NONE && !game.player_post_waiting_phase;
        int browse_wait=idle_player_turn && no_turn_post && game.interaction==FD2_FIELD_INTERACTION_BROWSE;
        int target_wait=idle_player_turn && no_turn_post && selector_input_context(game.interaction);
        int command_wait=idle_player_turn && command_input_context(&game);
        if(!target_wait)selector_wait.active=0;
        if(browse_wait || target_wait || command_wait) {
            if(target_wait && !selector_wait.active) {
                /* 115b6每次入口ESI=0；Z后才为当前槽。mode4及目的地模式
                 * 不轮询候选，不得继承其它picker的magic_target_cycle。 */
                size_t cycle=game.interaction==FD2_FIELD_INTERACTION_UNIT_SELECTED ||
                    game.interaction==FD2_FIELD_INTERACTION_MAGIC_TELEPORTING ||
                    game.interaction==FD2_FIELD_INTERACTION_ITEM_TELEPORTING ? 0:game.magic_target_cycle;
                if(cycle>INT32_MAX){loop.result=FD2_FIELD_PLAY_RETURN_ERROR;break;}
                selector_wait.tick=(int32_t)cycle;selector_wait.active=1;
            }
            int waited=browse_wait?play_browse_wait_step(&game,vga):command_wait?
                play_command_wait_step(&game,vga):play_selector_wait_step(&game,vga,&selector_wait.tick);
            if(waited!=FD2_SCENE_RESULT_OK) {
                loop.result=play_result_from_scene(waited);
                break;
            }
            fd2_field_game_tick_deadlines(&game,SDL_GetTicks());
        } else if(!once && game.active_side!=2 && game.interaction==FD2_FIELD_INTERACTION_BROWSE) {
            /* 1d80b/1d8ba逐raw直接调用13a9f；不插入宿主150ms空等、
             * 1297d或water。动作内有源等待仍由实际runtime消费。 */
        } else {
            (void)fd2_field_actor_visual_tick(vga, SDL_GetTicksNS());
            (void)fd2_field_water_tick_live(vga);
            fd2_field_game_tick(&game, SDL_GetTicksNS());
        }
        if (game.turn_number != reported_turn ||
            game.active_side != reported_side) {
            printf("field phase: turn=%u phase=%u remaining=%zu\n",
                   game.turn_number, game.active_side,
                   fd2_field_game_remaining_units(&game, game.active_side));
            fflush(stdout);
            reported_turn = game.turn_number;
            reported_side = game.active_side;
        }
        report_field_events(&game, &loop.reported_event_count, &loop.reported_dropped_events);
        if(browse_wait || target_wait || command_wait) {
            /* helper已投影完整原页；无tick/有早键则保上次画面，只泵宿主。 */
        } else if (game.turn_post_caller==FD2_FIELD_TURN_POST_MARCH)
            fd2_field_game_render_background(&game, vga);
        else
            fd2_field_game_render(&game, vga);
        if(fd2_vga_present(vga)<0){loop.result=FD2_FIELD_PLAY_RETURN_ERROR;break;}
        /* present 可泵入退出或暴露已有业务/声音错误；先核错误，不能
         * 让同点的 HOST 覆盖它，也不能由 once 绕过这一返回边界。 */
        if (loop.result==FD2_FIELD_PLAY_RETURN_ERROR || loop_session_failed(&loop, 1)) {
            loop_session_stop(&loop);
            break;
        }
        if (loop_terminal_stopped(&loop, 1)) break;
        if (once || !loop.running) break;
        if (automatic_continuation || (game.active_side != 2 &&
            game.interaction == FD2_FIELD_INTERACTION_BROWSE)) {
            int ai_result = fd2_field_game_process_automatic_action_visual(
                &game, vga, 55, game.turn_post_caller==FD2_FIELD_TURN_POST_MARCH);
            int terminal=play_field_terminal_result(&game,vga);
            if(ai_result>=0 && terminal!=FD2_SCENE_RESULT_OK) {
                loop.result=play_result_from_scene(terminal);
                break;
            }
            if (ai_result < 0) {
                report_session_failure(&game);
                fprintf(stderr, "field automatic action unsupported or failed\n");
                loop_stop(&loop, FD2_FIELD_PLAY_RETURN_ERROR);
            }
            continue;
        }
        if (!input_ready_reported) {
            printf("field input ready: stage=%zu turn=%u phase=%u\n",
                   game.stage, game.turn_number, game.active_side);
            fflush(stdout);
            input_ready_reported = 1;
        }
        fd2_input_action input_action;
        loop.end_input_frame = 0;
        while (loop.running && !loop.end_input_frame &&
               fd2_input_take_action(
                   &vga->input, field_input_context(&game),
                   &input_action)) {
            selector_wait.active=0; /* 原12dac已读一并返回；下一次等待重新带入ESI槽。 */
            fd2_field_action action = field_action_from_input(input_action);
            if (game.detail_visible) {
                int result = play_field_detail_step(&game, vga, field_audio);
                loop.end_input_frame = 1;
                (void)loop_scene_stopped(&loop, result, "单位详情翻页或关闭失败。");
                continue;
            }
            if (game.active_side != 2 && action != FD2_FIELD_ACTION_EXIT)
                continue;
            if (game.interaction == FD2_FIELD_INTERACTION_ITEM_MENU &&
                (action == FD2_FIELD_ACTION_UP || action == FD2_FIELD_ACTION_DOWN ||
                 action == FD2_FIELD_ACTION_LEFT || action == FD2_FIELD_ACTION_RIGHT)) {
                if (fd2_field_game_select_item_menu_direction(
                        &game, field_action_direction(action)) < 0) {
                    fprintf(stderr, "物品子菜单方向状态无效。\n");
                    loop_stop(&loop, FD2_FIELD_PLAY_RETURN_ERROR);
                }
                continue;
            }
            play_dispatch_action(&loop, action);
            if (loop_session_failed(&loop, 0)) loop_session_stop(&loop);
            if (loop.running) (void)loop_terminal_stopped(&loop, 1);
            if (game.player_post_waiting_phase) loop.end_input_frame = 1;
        }
        if(fd2_vga_delay_ms(vga,16)<0){loop.result=FD2_FIELD_PLAY_RETURN_ERROR;break;}
    }

    fd2_field_play_result play_result = loop.result;
    if (context) {
        play_result = play_finish_battle(&game, vga, fdother, field_audio, once,
            context, &roster_owned, play_result);
        context->experience_value = game.experience_value;
        context->message_number = game.message_number;
        context->message_new_fragment = game.message_new_fragment;
        context->message_old_fragment = game.message_old_fragment;
    }
    progress_box_free(&system_box);
    fd2_field_defeat_close(&runtime.defeat);
    game.physical_present = NULL;
    game.physical_present_userdata = NULL;
    game.magic_present = NULL;
    game.magic_present_userdata = NULL;
    game.runtime_present=NULL;
    game.runtime_present_userdata=NULL;
    if ((runtime.host_quit || game.runtime_host_quit) &&
        play_result!=FD2_FIELD_PLAY_RETURN_ERROR && game.failure==FD2_FIELD_FAILURE_NONE &&
        !battle.failed && !(context && context->music_error)) play_result=FD2_FIELD_PLAY_RETURN_HOST_QUIT;
    /* App门沿实际caller已到写点继承；错误/host cleanup只释放借用资源，
     * 不凭离场将51AAC恢复为尚未发生的成功尾。 */
    fd2_battle_animation_close(&battle);
    play_game_close(&game,context,roster_owned);
    return play_result;

setup_failed:
    play_game_close(&game,context,roster_owned);
    return setup_result;
}

int fd2_field_effect_play_run(fd2_vga *vga,
                              const fd2_archive *fdother,
                              size_t stage,
                              fd2_field_audio *field_audio) {
    if (!vga || !fdother) return -1;
    fd2_field_game game;
    if (fd2_field_game_open(&game, vga, fdother, stage) != 0)
        return -1;
    int result = -1;
    uint8_t unit_indices[] = {0};
    int center_screen_x =
        (game.cursor_cell_x - game.camera_cell_x) * 24 + 12;
    int center_screen_y =
        (game.cursor_cell_y - game.camera_cell_y) * 24 + 16;

    fd2_field_game_render(&game, vga);
    if(fd2_vga_present(vga)<0)goto done;
    if (fd2_field_game_play_actor_group_flash(
            &game, vga, field_audio, unit_indices, 1, 0xc0, 1) != 0)
        goto done;
    if (fd2_field_game_play_earthquake(
            &game, vga, field_audio, 1) != 0)
        goto done;
    if (fd2_field_game_play_stage_transition(
            &game, vga, field_audio, center_screen_x, center_screen_y,
            10, 8, 1) != 0)
        goto done;
    printf("field effects: stage=%zu, actor-flash=ok, earthquake=ok, "
           "stage-transition=ok, timed\n", stage);
    fflush(stdout);
    result = 0;

done:
    fd2_field_game_close(&game);
    return result;
}
