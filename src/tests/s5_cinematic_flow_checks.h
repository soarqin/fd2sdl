/* include于S5真实Scene私有TU：31bdf→1956b/1974c/26996/16c57合同。
 * 源geometry(310x86,5,112;phase5..0/1..5,13px)独立于生成JSON。
 * 只替身宿主present/等待；字模/头像/静态页和唯一walker均真实执行。 */
#ifndef FD2_S5_CINEMATIC_FLOW_CHECKS_H
#define FD2_S5_CINEMATIC_FLOW_CHECKS_H

static uint32_t s5_cin_rng(void *userdata) {(*(unsigned *)userdata)++;return 7;}
typedef struct {unsigned count;uint32_t value;fd2_vga *quit;} s5_montage_rng_state;
static uint32_t s5_montage_rng(void *userdata) {
    s5_montage_rng_state *state=userdata;state->count++;
    if(state->quit)state->quit->input.quit_requested=1;
    return state->value;
}
static fd2_static_flow_node s5_cin_node(const char *id,const char *op) {
    fd2_static_flow_node node={0};node.id=id;node.op=op;
    node.views=FD2_FLOW_LOGIC|FD2_FLOW_PRESENT;
    for(size_t i=0;i<FD2_FLOW_PORT_COUNT;i++)node.edges[i]=-1;
    return node;
}

static int s5_cinematic_checks(void) {
    fd2_archive other={0},dato={0};fd2_font font={0};fd2_ui_sheet ui={0};
    fd2_text_entry text={0};
    uint8_t one_glyph[]={2,0,5,0,255,255};
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga));
    fd2_scene_field_state *state=calloc(1,sizeof(*state));
    scene_flow_context *context=calloc(1,sizeof(*context));
    uint8_t *initial=malloc(64000),*wanted=malloc(64000),*page=malloc(64000);
    CHECK(game && before && vga && state && context && initial && wanted && page);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_archive_open(&dato,"original_game/DATO.DAT")==0);
    CHECK(fd2_font_open_fdother(&font,&other,4)==0);
    CHECK(ui_sheet_open_fdother(&ui,&other,5)==0);
    CHECK(fd2_text_entry_open_mem(&text,one_glyph,sizeof(one_glyph))==0);
    game->ready=1;game->stage=26;game->units.count=game->roster_count=1;
    game->units.items[0].hp=17;game->resource_value=0x12345678;
    context->vga=vga;context->game=context->mutable_game=game;context->state=state;
    context->fdother=&other;context->dato=&dato;context->font=&font;context->ui=&ui;
    context->text=&text;context->source_flow=1;context->fast=1;
    scene_source_load(context);*before=*game;
    for(size_t i=0;i<64000;i++)vga->framebuffer[i]=(uint8_t)(i*17u+9u);
    memcpy(initial,vga->framebuffer,64000);s5_reset_io(vga);

    /* 全资源宽度先验/坏owner不改前缀，不能把特殊128..132当普通页开放。 */
    CHECK(cinematic_notice_open(context,128)==-1);
    CHECK(!memcmp(initial,vga->framebuffer,64000));
    CHECK(cinematic_notice_open(context,4)==0);
    CHECK(cinematic_notice_open(context,4)==-1);
    CHECK(!memcmp(initial,vga->framebuffer,64000));
    memcpy(page,cinematic_notice_background(&context->cinematic_notice)+64000,64000);
    unsigned slides=0;
    for(int phase=5;phase>=0;phase--) {
        memcpy(wanted,initial,64000);
        int target_y=112+13*phase;
        for(int row=0;row<86 && target_y+row<200;row++)
            memcpy(wanted+(size_t)(target_y+row)*320+5,page+(size_t)(112+row)*320+5,310);
        CHECK(cinematic_notice_slide(context,phase)==0);
        CHECK(!memcmp(wanted,vga->framebuffer,64000));slides++;
    }
    CHECK(slides==6 && s5_presents==0 && s5_uploads==0 && s5_delays==0);
    memcpy(wanted,vga->framebuffer,64000);
    CHECK(cinematic_notice_slide(context,6)==-1);
    CHECK(!memcmp(wanted,vga->framebuffer,64000));

    /* 单字形明确从84,119绘制，不套普通story的95px origin；fast不额外RNG。 */
    fd2_vga *glyph=calloc(1,sizeof(*glyph));CHECK(glyph);
    memcpy(glyph->framebuffer,wanted,64000);
    fd2_font_draw_glyph(glyph,&font,5,84,119,205,76,74);
    CHECK(cinematic_notice_text(context,0)==0);
    CHECK(!memcmp(glyph->framebuffer,vga->framebuffer,64000));free(glyph);
    CHECK(context->cinematic_notice.x==100 && context->cinematic_notice.y==119);

    /* 原16c57即使入口已有键，也先一次RNG后消费唯一键；mode0无箭头写。 */
    unsigned rng_count=0;state->runtime.rng=s5_cin_rng;state->runtime.rng_userdata=&rng_count;
    context->fast=0;vga->input.queued_count=1;vga->input.queued_head=0;
    vga->input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
    memcpy(wanted,vga->framebuffer,64000);
    CHECK(cinematic_notice_wait(context,0)==0 && rng_count==1);
    CHECK(vga->input.queued_count==0 && !memcmp(wanted,vga->framebuffer,64000));
    CHECK(cinematic_notice_wait(context,2)==-1 && rng_count==1);
    for(int phase=1;phase<=5;phase++) {
        CHECK(cinematic_notice_slide(context,phase)==0);slides++;
    }
    CHECK(slides==11);
    fd2_static_flow_node close=s5_cin_node("close","cin_notice_close");unsigned port=99;
    CHECK(scene_cinematic_node(context,&close,&port)==1 && port==FD2_FLOW_NEXT);
    CHECK(!context->cinematic_notice.is_open && !memcmp(initial,vga->framebuffer,64000));
    CHECK(scene_cinematic_node(context,&close,&port)==-1);
    CHECK(!memcmp(before,game,sizeof(*game)));

    /* stage20的68个cin_water节点共享一个op；只核路由/owner/返回门，
     * 不把本TU的固定tick适配器当原clock oracle。真实live叶的比较读→
     * DAC写→fresh锚另由field_visual_live:test_water_reads核，不能在op预取ms。 */
    {
        fd2_static_flow_node water=s5_cin_node("water","cin_water");
        const int statuses[]={0,1,-1};unsigned clocks=s5_clock_reads;
        unsigned presents=s5_presents,uploads=s5_uploads,delays=s5_delays,ticks=s5_ticks;
        s5_water_probe=1;s5_water_owner=vga;s5_water_calls=s5_water_bad_owner=0;
        for(size_t i=0;i<sizeof(statuses)/sizeof(statuses[0]);i++) {
            s5_water_result=statuses[i];port=99;
            CHECK(scene_cinematic_node(context,&water,&port)==(statuses[i]<0?-1:1));
            CHECK(port==FD2_FLOW_NEXT && s5_water_calls==i+1 && !s5_water_bad_owner);
            CHECK(s5_clock_reads==clocks && rng_count==1);
            CHECK(s5_presents==presents && s5_uploads==uploads && s5_delays==delays && s5_ticks==ticks);
            CHECK(!memcmp(before,game,sizeof(*game)));
        }
        s5_water_probe=0;s5_water_owner=NULL;
    }

    /* 六个固定call的独立局部预算仍由唯一walker使用；父计数/深度还原。 */
    context->fast=1;context->flow_depth=1;context->flow_steps=FD2_FLOW_STEP_BUDGET-1;
    fd2_static_flow_node nodes[3];
    nodes[0]=s5_cin_node("paint","palette_add");nodes[0].args[1]=255;nodes[0].args[2]=1;
    nodes[0].edges[FD2_FLOW_NEXT]=1;nodes[1]=s5_cin_node("end","end");
    fd2_static_flow child={"stage.cinematic.card_first",nodes,2,0};s5_flow_override=&child;
    memset(vga->palette,7,768);memset(vga->dac,0,768);
    fd2_static_flow_node call=s5_cin_node("child","call_cinematic_card_first");
    static const char *const ids[]={"stage.cinematic.ending","stage.cinematic.credits",
        "stage.cinematic.card_normal","stage.cinematic.card_last","stage.cinematic.card_first",
        "stage.cinematic.montage_round"};
    static const char *const operations[]={"call_cinematic_ending","call_cinematic_credits",
        "call_cinematic_card_normal","call_cinematic_card_last","call_cinematic_card_first",
        "call_cinematic_montage_round"};
    for(size_t i=0;i<6;i++) {
        child.id=ids[i];call.op=operations[i];
        CHECK(scene_cinematic_node(context,&call,&port)==1);
        CHECK(context->flow_steps==FD2_FLOW_STEP_BUDGET-1 && context->flow_depth==1);
        for(size_t channel=0;channel<768;channel++)CHECK(vga->dac[channel]==32);
    }
    /* 已到DAC前缀不回滚，坏分配不增加Game/RNG写点，不执行后面的add63。 */
    nodes[1]=s5_cin_node("bad","cin_page_alloc");nodes[1].args[0]=1;nodes[1].args[1]=1;
    nodes[1].edges[FD2_FLOW_NEXT]=2;
    nodes[2]=s5_cin_node("forbidden","palette_add");nodes[2].args[1]=255;nodes[2].args[2]=63;
    child.count=3;memset(vga->dac,0,768);
    CHECK(scene_cinematic_node(context,&call,&port)==-1);
    for(size_t i=0;i<768;i++)CHECK(vga->dac[i]==32);
    CHECK(context->flow_steps==FD2_FLOW_STEP_BUDGET-1 && context->flow_depth==1 && rng_count==1);
    CHECK(!memcmp(before,game,sizeof(*game)));
    /* 同一个固定ID递归也必须在原深度8硬拒，不扩成任意VM。 */
    nodes[0]=call;nodes[0].edges[FD2_FLOW_NEXT]=1;nodes[1]=s5_cin_node("end","end");
    child.count=2;
    CHECK(scene_cinematic_node(context,&call,&port)==-1 && context->flow_depth==1);
    CHECK(context->flow_steps==FD2_FLOW_STEP_BUDGET-1);
    s5_flow_override=NULL;

    /* 20421的临时52766不属于53A65/DAC；真实ANI decoder的像素/键/时间
     * 委托不变，此I/O spy故意污染两色区并在每帧hook之后核还原。 */
    context->flow_depth=0;context->flow_steps=0;context->fast=0;
    fd2_static_flow_node ani=s5_cin_node("ani","cin_animation");
    ani.args[0]=0;ani.args[1]=15;ani.args[2]=0;
    uint8_t original_palette[768],original_dac[768];
    memcpy(original_palette,vga->palette,768);memcpy(original_dac,vga->dac,768);
    static const int statuses[]={0,-1,-2};
    for(size_t k=0;k<3;k++) {
        s5_reset_io(vga);s5_ani_status=statuses[k];s5_ani_calls=0;s5_ani_palette_ok=1;
        int status=scene_cinematic_node(context,&ani,&port);
        CHECK(status==(k? -1:1) && s5_ani_calls==1 && s5_ani_palette_ok);
        CHECK(s5_ani_index==0 && s5_ani_delay==15 && s5_ani_check==0);
        CHECK(vga->framebuffer[0]==43);
        CHECK(!memcmp(original_palette,vga->palette,768) && !memcmp(original_dac,vga->dac,768));
        CHECK(!memcmp(before,game,sizeof(*game)) && rng_count==1);
        CHECK(scene_host_quit_requested==(statuses[k]==-2));
    }
    s5_reset_io(vga);s5_ani_status=0;context->fast=1;ani.args[2]=1;
    CHECK(scene_cinematic_node(context,&ani,&port)==1 && s5_ani_delay==0 && s5_ani_check==0);
    context->fast=0;ani.args[0]=-1;unsigned calls_before=(unsigned)s5_ani_calls;
    CHECK(scene_cinematic_node(context,&ani,&port)==-1 && (unsigned)s5_ani_calls==calls_before);
    ani.args[0]=1; /* ANI1带原SFX78特殊caller，不借此新source叶提权。 */
    CHECK(scene_cinematic_node(context,&ani,&port)==-1 && (unsigned)s5_ani_calls==calls_before);

    /* HOST在wait之前到达：不消费新业务键/RNG，不走后续palette；保开框前缀。 */
    context->flow_depth=0;context->flow_steps=0;context->fast=0;s5_reset_io(vga);
    CHECK(cinematic_notice_open(context,4)==0);
    nodes[0]=s5_cin_node("wait","cin_notice_wait");nodes[0].edges[FD2_FLOW_NEXT]=1;
    nodes[1]=s5_cin_node("after","palette_add");nodes[1].args[1]=255;nodes[1].args[2]=63;
    nodes[1].edges[FD2_FLOW_NEXT]=2;nodes[2]=s5_cin_node("end","end");
    fd2_static_flow host={"stage.source.test.notice-host",nodes,3,0};
    uint8_t old_dac[768];memcpy(old_dac,vga->dac,768);vga->input.quit_requested=1;
    CHECK(scene_flow_run(context,&host)==-1 && scene_host_quit_requested);
    CHECK(rng_count==1 && !memcmp(old_dac,vga->dac,768));
    CHECK(context->cinematic_notice.is_open && !memcmp(before,game,sizeof(*game)));
    vga->input.quit_requested=0;s5_reset_io(vga);
    CHECK(scene_cinematic_node(context,&close,&port)==1);
    fd2_scene_cinematic_close(&context->cinematic);

    /* 31529明确冷写真实Game raw0/1，绘制仅借其值；不触cache/text/roster。
     * 真montage_round走唯一walker/实际DAT，marker HP不藏在renderer内。 */
    CHECK(fd2_archive_open(&game->figani_archive,"original_game/FIGANI.DAT")==0);
    game->units.count=2;game->units.items[0].unit_id=51;game->units.items[0].side=2;
    game->units.items[1].unit_id=103;game->units.items[1].side=0;
    game->units.items[0].sprite_cache_class=7;game->units.items[1].sprite_cache_class=9;
    game->units.items[0].text_id=12;game->units.items[1].text_id=13;
    game->units.items[0].hp=17;game->units.items[1].hp=23;
    s5_montage_rng_state random={0,0,NULL};game->attack_rng=s5_montage_rng;game->attack_rng_userdata=&random;
    scene_source_load(context);*before=*game;context->fast=1;s5_reset_io(vga);
    fd2_static_flow_node montage=s5_cin_node("open","cin_montage_open");
    montage.args[0]=51;montage.args[1]=103;montage.args[2]=4;
    CHECK(scene_cinematic_node(context,&montage,&port)==1 && !memcmp(game,before,sizeof(*game)));
    montage.op="cin_montage_intro";
    for(int phase=8;phase>=0;phase--){montage.args[0]=phase;CHECK(scene_cinematic_node(context,&montage,&port)==1);}
    montage.op="cin_montage_intro_finish";CHECK(scene_cinematic_node(context,&montage,&port)==1);
    montage.op="cin_montage_half";montage.args[0]=0;CHECK(scene_cinematic_node(context,&montage,&port)==1);
    montage.op="branch_cin_montage_rng";montage.args[0]=101;
    CHECK(scene_cinematic_node(context,&montage,&port)==-1 && random.count==0);
    montage.args[0]=3;CHECK(scene_cinematic_node(context,&montage,&port)==1 && port==FD2_FLOW_YES && random.count==1);
    random.value=50;CHECK(scene_cinematic_node(context,&montage,&port)==1 && port==FD2_FLOW_NO && random.count==2);
    const fd2_static_flow *round=fd2_static_flow_lookup("stage","stage.cinematic.montage_round");CHECK(round);
    CHECK(scene_flow_run(context,round)==0);
    before->units.items[1].hp=63680;
    CHECK(!memcmp(game,before,sizeof(*game)) && random.count==2);
    montage.op="cin_montage_half";montage.args[0]=1;CHECK(scene_cinematic_node(context,&montage,&port)==1);
    CHECK(scene_flow_run(context,round)==0);before->units.items[0].hp=63680;
    CHECK(!memcmp(game,before,sizeof(*game)) && random.count==2);

    /* 已提交HP/frame后遇坏part，保前缀且不执行后置HP0/RNG；HOST也不洗错误。 */
    fd2_static_flow_node prefix[6];
    prefix[0]=s5_cin_node("hp","unit_field_write");prefix[0].args[0]=prefix[0].args[1]=0;
    prefix[0].args[2]=18;prefix[0].args[4]=63680;prefix[0].edges[FD2_FLOW_NEXT]=1;
    prefix[1]=s5_cin_node("frame","cin_montage_frame");prefix[1].args[0]=0;prefix[1].edges[FD2_FLOW_NEXT]=2;
    prefix[2]=s5_cin_node("bad","cin_montage_frame");prefix[2].args[0]=22;prefix[2].edges[FD2_FLOW_NEXT]=3;
    prefix[3]=s5_cin_node("forbidden","unit_field_write");prefix[3].args[0]=prefix[3].args[1]=0;
    prefix[3].args[2]=18;prefix[3].args[4]=0;prefix[3].edges[FD2_FLOW_NEXT]=4;
    prefix[4]=s5_cin_node("end","end");
    fd2_static_flow broken={"stage.source.test.montage-prefix",prefix,5,0};
    CHECK(scene_flow_run(context,&broken)==-1 && !scene_host_quit_requested);
    CHECK(!memcmp(game,before,sizeof(*game)) && random.count==2);
    prefix[0]=s5_cin_node("rng","branch_cin_montage_rng");prefix[0].args[0]=3;
    prefix[0].edges[FD2_FLOW_YES]=prefix[0].edges[FD2_FLOW_NO]=3;
    random.quit=vga;context->fast=0;s5_reset_io(vga);
    CHECK(scene_flow_run(context,&broken)==-1 && scene_host_quit_requested && random.count==3);
    CHECK(!memcmp(game,before,sizeof(*game)));
    random.quit=NULL;vga->input.quit_requested=0;s5_reset_io(vga);
    montage.op="cin_montage_close";CHECK(scene_cinematic_node(context,&montage,&port)==1);
    CHECK(scene_cinematic_node(context,&montage,&port)==-1 && random.count==3);
    fd2_scene_cinematic_close(&context->cinematic);fd2_archive_close(&game->figani_archive);
    fd2_text_entry_close(&text);fd2_archive_close(&dato);fd2_archive_close(&other);
    free(initial);free(wanted);free(page);free(context);free(state);free(vga);free(before);free(game);
    return 0;
}
#endif
