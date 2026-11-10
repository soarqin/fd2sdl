/* 11cac/11eee/127a9的窄离屏借用。真实App只持有一份53A49；
 * PREPARE只临时deep-clone该页，UI绘制仍只写target VGA，不另造Game。 */
#ifndef FD2_FIELD_RENDER_SCOPE_H
#define FD2_FIELD_RENDER_SCOPE_H

#include "vga.h"

typedef struct {
    fd2_vga *target, *app;
    fd2_field_page *saved_page, *cloned_page;
    fd2_field_visual_state visual;
    uint16_t jitter_tick;
    uint8_t jitter_phase, info_suppressed, info_right;
    int active, readonly;
} fd2_field_render_scope;

static int fd2_field_render_scope_begin(fd2_field_render_scope *scope,
    fd2_vga *target, fd2_vga *app, int readonly) {
    if (!scope || !target || !app || (readonly != 0 && readonly != 1)) return -1;
    *scope=(fd2_field_render_scope){0};
    /* 无App页的旧纯绘制夹具不冒充正式App验收，也不凭空生成持久页。 */
    if (!app->field_page) return 0;
    fd2_field_page *page=app->field_page;
    if (readonly) {
        scope->cloned_page=fd2_field_page_clone(page);
        if (!scope->cloned_page) return -1;
        page=scope->cloned_page;
    }
    scope->target=target;scope->app=app;scope->readonly=readonly;
    scope->saved_page=target->field_page;
    scope->visual=target->field_visual;
    scope->jitter_tick=target->actor_jitter_tick;
    scope->jitter_phase=target->actor_jitter_phase;
    scope->info_suppressed=target->info_panel_suppressed;
    scope->info_right=target->info_panel_right;
    scope->active=1;
    target->field_page=page;
    target->field_visual=app->field_visual;
    target->actor_jitter_tick=app->actor_jitter_tick;
    target->actor_jitter_phase=app->actor_jitter_phase;
    target->info_panel_suppressed=app->info_panel_suppressed;
    target->info_panel_right=app->info_panel_right;
    return 0;
}

static void fd2_field_render_scope_end(fd2_field_render_scope *scope) {
    if (!scope || !scope->active) return;
    fd2_vga *target=scope->target;
    if (!scope->readonly) scope->app->info_panel_right=target->info_panel_right;
    if (scope->readonly || target!=scope->app) {
        target->field_page=scope->saved_page;
        target->field_visual=scope->visual;
        target->actor_jitter_tick=scope->jitter_tick;
        target->actor_jitter_phase=scope->jitter_phase;
        target->info_panel_suppressed=scope->info_suppressed;
        target->info_panel_right=scope->info_right;
    }
    fd2_field_page_destroy(scope->cloned_page);
    scope->active=0;
}

#endif
