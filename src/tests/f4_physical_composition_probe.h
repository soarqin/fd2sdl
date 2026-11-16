/* F4 FULL物理单tick组合，仅Battle测试私有TU；不新增产品API或decoder。
 * 原2ebe1：board→400stride work+16040，按side/layer画attack与idle，
 * 反向震动/常色33后投影320x200。期望来自保留的原4e98d像素+真实mask。
 * 本probe只调用真实full_tick_draw；不运行业务/RNG/clock/present/设备。 */
#ifndef FD2_F4_PHYSICAL_COMPOSITION_PROBE_H
#define FD2_F4_PHYSICAL_COMPOSITION_PROBE_H
enum { F4_PHYS_WORK=128000, F4_PHYS_VGA=64000, F4_PHYS_PAYLOAD=515072 };

static void f4_phys_seed(uint8_t *dst,size_t n,unsigned factor,unsigned stride,unsigned base,int inverse) {
    for(size_t i=0;i<n;i++) {
        uint8_t value=(uint8_t)(i*factor+i/stride+base);
        dst[i]=inverse?(uint8_t)(value^255u):value;
    }
}
static int f4_phys_parse(char *line,long v[7]) {
    char *p=line,*end;
    for(unsigned i=0;i<7;i++) {
        while(isspace((unsigned char)*p))p++;
        errno=0;v[i]=strtol(p,&end,10);
        if(errno || p==end || (i<6 && !isspace((unsigned char)*end)))return -1;
        p=end;
    }
    while(isspace((unsigned char)*p))p++;
    if(*p || v[0]<0 || v[0]>135 || v[1]<0 || v[1]>135 || v[0]==v[1] ||
       v[2]<0 || v[2]>255 || v[3]<0 || v[3]>255 || (v[4]!=0 && v[4]!=2) ||
       v[5]<0 || v[5]>5 || (v[6]!=-1 && v[6]!=33))return -1;
    return 0;
}
static int f4_phys_case(const fd2_archive *archive,const long v[7],unsigned number) {
    fd2_vga *vga=calloc(1,sizeof(*vga)),*before=malloc(sizeof(*before));
    fd2_battle_animation_exchange *e=calloc(1,sizeof(*e)),*snapshot=malloc(sizeof(*snapshot));
    uint8_t *payload=malloc(F4_PHYS_PAYLOAD);int result=-1;
    unsigned old_frames=frames,old_ticks=ticks,old_delays=delays,old_cues=cues;
    unsigned old_pumps=input_pumps,old_rng=business_rng_calls;uint64_t old_ms=fake_ms;
    if(!vga || !before || !e || !snapshot || !payload)goto done;
    if(clip_open_visual(&e->attack[0],archive,3u*(unsigned)v[0]+1u) ||
       clip_open_visual(&e->idle[1],archive,3u*(unsigned)v[1]) ||
       e->attack[0].clip.empty || e->idle[1].clip.empty ||
       (size_t)v[2]>=e->attack[0].clip.part_count || (size_t)v[3]>=e->idle[1].clip.part_count ||
       !e->attack[0].parts[v[2]].part.duration ||
       (e->attack[0].clip.split_mode && (size_t)v[2]<e->attack[0].clip.intro_end))goto done;
    fd2_battle_animation context={0};context.vga=vga;context.exchange=e;
    e->idle_part=(uint8_t)v[3];e->shake=(uint8_t)v[5];e->tint=(int)v[6];
    for(unsigned pass=0;pass<2;pass++) {
        f4_phys_seed(e->board.framebuffer,64000,13,320,31,0);
        f4_phys_seed(e->work,sizeof(e->work),17,400,83,(int)pass);
        f4_phys_seed(vga->framebuffer,64000,19,320,61,0);
        for(unsigned i=0;i<768;i++) {vga->palette[i]=(uint8_t)(i*7u%64u);vga->dac[i]=(uint8_t)(i*11u+29u);}
        *before=*vga;*snapshot=*e;
        full_tick_draw(&context,&e->attack[0],&e->idle[1],(size_t)v[2],v[4]!=0);
        /* 产品work兼容SHORT的尾不属于原400x320 owner，必须完全保留。 */
        if(memcmp(e->work+F4_PHYS_WORK,snapshot->work+F4_PHYS_WORK,sizeof(e->work)-F4_PHYS_WORK))goto done;
        memcpy(snapshot->work,e->work,F4_PHYS_WORK);
        if(memcmp(snapshot,e,sizeof(*e)))goto done;
        memcpy(before->framebuffer,vga->framebuffer,64000);
        if(memcmp(before,vga,sizeof(*vga)))goto done;
        size_t at=(size_t)pass*(F4_PHYS_PAYLOAD/2u);
        memcpy(payload+at,e->work,F4_PHYS_WORK);at+=F4_PHYS_WORK;
        memcpy(payload+at,vga->framebuffer,64000);at+=64000;
        memcpy(payload+at,e->board.framebuffer,64000);at+=64000;
        memcpy(payload+at,vga->palette,768);at+=768;memcpy(payload+at,vga->dac,768);
    }
    if(frames!=old_frames || ticks!=old_ticks || delays!=old_delays || cues!=old_cues ||
       input_pumps!=old_pumps || business_rng_calls!=old_rng || fake_ms!=old_ms)goto done;
    if(printf("{\"schema\":\"fd2-f4-physical-composition-actual-v1\",\"case\":%u,\"request\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld],"
        "\"split\":%u,\"layer\":%u,\"payload_bytes\":%u,\"other_owners_unchanged\":true,\"extra_work_unchanged\":true}\n",
        number,v[0],v[1],v[2],v[3],v[4],v[5],v[6],e->attack[0].clip.split_mode,
        e->attack[0].parts[v[2]].part.layer_flags,F4_PHYS_PAYLOAD)<0 ||
       fwrite(payload,1,F4_PHYS_PAYLOAD,stdout)!=F4_PHYS_PAYLOAD || fflush(stdout))goto done;
    result=0;
done:
    if(e) {clip_close(&e->attack[0]);clip_close(&e->idle[1]);}
    free(payload);free(snapshot);free(e);free(before);free(vga);return result;
}
static int f4_phys_cli(const char *root) {
    fd2_archive archive={0};char path[1024],line[160];uint8_t *frozen=NULL;
    unsigned count=0;int result=1;long request[7];
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)<0)return 1;
#endif
    if(f4_path(path,root,"FIGANI.DAT") || fd2_archive_open(&archive,path) ||
       !(frozen=malloc(archive.size)))goto done;
    memcpy(frozen,archive.data,archive.size);
    if(f4_input_receipt("FIGANI.DAT",archive.data,archive.size))goto done;
    while(fgets(line,sizeof(line),stdin)) {
        if(count>=48 || !strchr(line,'\n') || f4_phys_parse(line,request) ||
           f4_phys_case(&archive,request,count))goto done;
        count++;
    }
    if(ferror(stdin) || count!=48 || memcmp(frozen,archive.data,archive.size))goto done;
    if(printf("{\"end\":%u,\"binding_unchanged\":true}\n",count)<0 || fflush(stdout))goto done;
    result=0;
done:
    free(frozen);fd2_archive_close(&archive);
    if(result)fputs("F4 physical composition rejected request/source/output\n",stderr);
    return result;
}
#endif
