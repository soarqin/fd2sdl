/* 11cac工作页的离屏scope：PREPARE无副作用、真实绘制的借用与资源归还。 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_render_scope.h"

static uint8_t pixel(fd2_field_page *page) {
    uint8_t value=0;
    assert(fd2_field_page_read(page,72,72,1,1,&value,1,1)==0);
    return value;
}
static void put(fd2_field_page *page,uint8_t value) {
    assert(fd2_field_page_write(page,72,72,1,1,&value,1,1)==0);
}

int main(void) {
    fd2_vga *app=calloc(1,sizeof(*app)),*target=calloc(1,sizeof(*target));
    assert(app && target);
    app->field_page=fd2_field_page_create();target->field_page=fd2_field_page_create();
    assert(app->field_page && target->field_page);
    put(app->field_page,190);put(target->field_page,7);
    app->field_visual.cursor_phase=3;app->field_visual.actor_idle_tick=123;
    app->actor_jitter_phase=1;app->info_panel_suppressed=1;
    fd2_vga target_before=*target,app_before=*app;
    fd2_field_render_scope scope;
    assert(fd2_field_render_scope_begin(&scope,target,app,1)==0);
    assert(target->field_page!=app->field_page && pixel(target->field_page)==190);
    assert(!memcmp(&target->field_visual,&app->field_visual,sizeof(app->field_visual)));
    put(target->field_page,55);target->info_panel_right=1;
    fd2_field_render_scope_end(&scope);
    assert(!memcmp(target,&target_before,sizeof(*target)) && !memcmp(app,&app_before,sizeof(*app)));
    assert(pixel(app->field_page)==190 && pixel(target->field_page)==7);
    assert(fd2_field_render_scope_begin(&scope,target,app,0)==0);
    assert(target->field_page==app->field_page);
    put(target->field_page,189);target->info_panel_right=1;
    fd2_field_render_scope_end(&scope);
    assert(!memcmp(target,&target_before,sizeof(*target)) && pixel(app->field_page)==189 && app->info_panel_right==1);
    app_before=*app;
    assert(fd2_field_render_scope_begin(&scope,app,app,1)==0);
    assert(app->field_page!=app_before.field_page);
    put(app->field_page,90);app->field_visual.cursor_phase=0;app->info_panel_right=0;
    fd2_field_render_scope_end(&scope);
    assert(!memcmp(app,&app_before,sizeof(*app)) && pixel(app->field_page)==189);
    assert(fd2_field_render_scope_begin(&scope,app,app,0)==0);
    put(app->field_page,188);app->info_panel_right=0;
    fd2_field_render_scope_end(&scope);
    assert(pixel(app->field_page)==188 && !app->info_panel_right);
    fd2_field_render_scope_end(&scope); /* 清理可重复，不double free。 */
    fd2_field_page_destroy(target->field_page);fd2_field_page_destroy(app->field_page);
    free(target);free(app);puts("field_render_scope: passed");return 0;
}
