/* 私有测试TU：实际三magic work consumer，stdout only，无Game/设备/新decoder。
 * 所有正常图mask来自原叶；synthetic只独立检RAW/RLE写点的opaque0语义。 */
#ifndef FD2_F4_MAGIC_LINEAR_WORK_PROBE_H
#define FD2_F4_MAGIC_LINEAR_WORK_PROBE_H
enum { F4_MAGIC_WORK_PAYLOAD=604672 };
static int f4_mwork_parse(char *line,long v[9]) {
    char *p=line,*end;
    for(unsigned i=0;i<9;i++) {
        while(isspace((unsigned char)*p))p++;
        errno=0;v[i]=strtol(p,&end,10);
        if(errno || p==end || (i<8 && !isspace((unsigned char)*end)))return -1;
        p=end;
    }
    while(isspace((unsigned char)*p))p++;
    return *p || v[0]<0 || v[0]>3 || v[1]<0 || v[1]>407 || v[2]<0 || v[2]>254 ||
        (v[3]!=0 && v[3]!=2) || v[4]<0 || v[4]>172799 || v[5]<-1024 || v[5]>1024 ||
        v[6]<-1024 || v[6]>1024 || v[7]!=-1 || v[8]<0 || v[8]>19360?-1:0;
}
static int f4_mwork_case(const fd2_archive *other,const fd2_archive *figani,const long v[9],unsigned number) {
    fd2_battle_magic_exchange *e=calloc(1,sizeof(*e)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*vb=malloc(sizeof(*vb));uint8_t *payload=malloc(F4_MAGIC_WORK_PAYLOAD);
    battle_clip *clip=NULL;int result=-1;unsigned previous_frames=frames,previous_pumps=input_pumps;
    if(!e || !before || !vga || !vb || !payload)goto done;
    e->mode=(fd2_field_magic_present_mode)v[3];e->idle_part=(uint8_t)v[2];
    clip=v[0]==1?&e->target_idle[0]:&e->particles;
    if(v[0]==3) {
        if(v[1] || v[2])goto done;
        clip->clip.part_count=1;clip->parts=calloc(1,sizeof(*clip->parts));
        if(!clip->parts)goto done;
        battle_part *part=&clip->parts[0];part->part.width=3;part->part.height=2;part->part.duration=2;
        part->drawing.image.width=3;part->drawing.image.height=2;
        part->drawing.image.pixels=malloc(6);part->drawing.opaque=malloc(6);
        if(!part->drawing.image.pixels || !part->drawing.opaque)goto done;
        static const uint8_t pixels[6]={0,0,5,6,7,0},mask[6]={1,0,1,1,1,1};
        memcpy(part->drawing.image.pixels,pixels,6);memcpy(part->drawing.opaque,mask,6);
    } else if(clip_open_visual(clip,v[0]==1?figani:other,(size_t)v[1]) ||
        (size_t)v[2]>=clip->clip.part_count)goto done;
    size_t owner=e->mode==FD2_FIELD_MAGIC_MODE_FULL?172800u:128000u;
    if((size_t)v[8]+199u*640u+320u>owner)goto done;
    fd2_battle_animation context={0};context.vga=vga;context.magic_exchange=e;
    for(unsigned pass=0;pass<2;pass++) {
        f4_phys_seed(e->work,sizeof(e->work),17,640,83,(int)pass);
        f4_phys_seed(e->board.framebuffer,64000,13,320,31,0);
        f4_phys_seed(vga->framebuffer,64000,19,320,61,0);
        for(unsigned i=0;i<768;i++){vga->palette[i]=(uint8_t)(i*7u%64u);vga->dac[i]=(uint8_t)(i*11u+29u);}
        e->idle_part=(uint8_t)v[2];e->idle_ticks=0;*before=*e;*vb=*vga;
        if(v[0]==1)magic_work_idle(e,(int)v[4],clip,(int)v[5],(int)v[6],(int)v[7]);
        else if(v[0]==2) {
            magic_fx_reader reader={&context,NULL,0,0,(int)v[4]};
            fd2_magic_fx_emission emission={FD2_MAGIC_FX_PART,5,(uint16_t)v[2],(int32_t)v[5],(int32_t)v[6]};
            if(magic_fx_emit(&emission,&reader))goto done;
        } else magic_work_part(e,(int)v[4],clip,(size_t)v[2],(int)v[5],(int)v[6],(int)v[7]);
        magic_work_present(&context,(int)v[8],0);
        memcpy(before->work,e->work,sizeof(e->work));
        if(v[0]==1) {
            before->idle_ticks++;
            if(before->idle_ticks>=clip->parts[before->idle_part].part.duration) {
                before->idle_ticks=0;if(++before->idle_part>=clip->clip.part_count)before->idle_part=0;
            }
        }
        memcpy(vb->framebuffer,vga->framebuffer,64000);
        if(memcmp(e,before,sizeof(*e)) || memcmp(vb,vga,sizeof(*vga)) || context.failed || context.host_quit)goto done;
        size_t at=(size_t)pass*(F4_MAGIC_WORK_PAYLOAD/2u);
        memcpy(payload+at,e->work,172800);at+=172800;
        memcpy(payload+at,vga->framebuffer,64000);at+=64000;
        memcpy(payload+at,e->board.framebuffer,64000);at+=64000;
        memcpy(payload+at,vga->palette,768);at+=768;memcpy(payload+at,vga->dac,768);
    }
    if(frames!=previous_frames+2u || input_pumps!=previous_pumps+2u || ticks || delays || cues || business_rng_calls)goto done;
    if(printf("{\"schema\":\"fd2-f4-magic-work-actual-v1\",\"case\":%u,\"request\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld],"
        "\"owner_bytes\":%zu,\"payload_bytes\":%u,\"idle_part\":%u,\"idle_ticks\":%u,\"other_owners_unchanged\":true}\n",
        number,v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7],v[8],owner,F4_MAGIC_WORK_PAYLOAD,e->idle_part,e->idle_ticks)<0 ||
       fwrite(payload,1,F4_MAGIC_WORK_PAYLOAD,stdout)!=F4_MAGIC_WORK_PAYLOAD || fflush(stdout))goto done;
    result=0;
done:
    if(e){clip_close(&e->particles);clip_close(&e->target_idle[0]);}
    free(e);free(before);free(vga);free(vb);free(payload);return result;
}
static int f4_mwork_cli(const char *root) {
    fd2_archive other={0},figani={0};char path[1024],line[192];long v[9];unsigned count=0;int result=1;
    uint8_t *old_other=NULL,*old_figani=NULL;
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)<0)return 1;
#endif
    if(f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&other,path) ||
       f4_path(path,root,"FIGANI.DAT") || fd2_archive_open(&figani,path) ||
       !(old_other=malloc(other.size)) || !(old_figani=malloc(figani.size)))goto done;
    memcpy(old_other,other.data,other.size);memcpy(old_figani,figani.data,figani.size);
    if(f4_input_receipt("FDOTHER.DAT",other.data,other.size) || f4_input_receipt("FIGANI.DAT",figani.data,figani.size))goto done;
    while(fgets(line,sizeof(line),stdin)) {
        if(count>=8 || !strchr(line,'\n') || f4_mwork_parse(line,v) || f4_mwork_case(&other,&figani,v,count))goto done;
        count++;
    }
    if(count!=8 || ferror(stdin) || memcmp(other.data,old_other,other.size) || memcmp(figani.data,old_figani,figani.size))goto done;
    if(printf("{\"end\":8,\"bindings_unchanged\":true}\n")<0 || fflush(stdout))goto done;
    result=0;
done:
    free(old_other);free(old_figani);fd2_archive_close(&other);fd2_archive_close(&figani);
    if(result)fputs("F4 magic work rejected request/source/output\n",stderr);
    return result;
}
#endif
