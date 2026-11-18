/* SPDX-License-Identifier: MIT
 * audio.c唯一owner的固定48k私有接线；非第二renderer/clock/FIFO。
 * 固定host Mixer::MixData/FillUp/EndFrame + SB driver401/402/306。
 * 保留共同混音、持续采样相位、短尾和两handle尾音；不模拟CPU/PIC。
 */
#ifndef FD2_AUDIO_ORIGINAL_PRIVATE_H
#define FD2_AUDIO_ORIGINAL_PRIVATE_H

static int original_mix_until(fd2_audio *audio, size_t whole);
static void original_deliver_after_call(fd2_audio *audio) {
    /* 一次Mix/END完成后推进缓冲交接；只服务已生成的音频。
     * provider控制的嵌套FillUp不重入它，pending保留到下一完成边界。 */
    if (audio->original_bound && !audio->original_quiesced &&
        !audio->original_sb.in_control && !audio->original_quiescing && audio->original_sb.dma.irq_pending &&
        fd2_audio_sb_deliver_irq(&audio->original_sb) < 0)
        record_first_error(audio, FD2_AUDIO_ERROR_SOURCE);
}
static int original_global_fill(void *user) {
    fd2_audio *audio = user;
    /* 官方先把PIC_TickIndex窄成float，再乘48000frac/ms并截断。 */
    float index = (float)audio->original_point.tick_index;
    if (index < 0) index = 0;
    if (index > 1) index = 1;
    uint32_t fracs = (uint32_t)((double)index * 48000.0);
    size_t whole = fracs / 1000u;
    int result = original_mix_until(audio, whole);
    original_deliver_after_call(audio);
    return result;
}
static int original_mix_until(fd2_audio *audio, size_t whole) {
    if (whole <= audio->original_rendered) return 0;
    if (whole > 48 || audio->original_busy) {
        record_first_error(audio, FD2_AUDIO_ERROR_SOURCE); return -1;
    }
    audio->original_busy = 1;
    const size_t begin = audio->original_rendered, count = whole - begin;
    int32_t fm[48][2] = {{0}};
    int result = 0;
    if (audio->original_mixer &&
        fd2_audio_sb_mix_until(&audio->original_sb,audio->original_sb_work,48,whole) < 0)
        result = -1;
    for (size_t i=0;i<FD2_AUDIO_MAX_VOICES;i++) {
        fd2_audio_voice *voice=&audio->voices[i];
        if (!voice->active) continue;
        memset(audio->source_buffer,0,count*2u*sizeof(float));
        audio->in_callback++;
        size_t n=voice->source.render(voice->source.userdata,audio->source_buffer,count);
        audio->in_callback--;
        if(n>count)n=count;
        for(size_t sample=0;sample<n;sample++)for(size_t side=0;side<2;side++) {
            /* Nuked S16/32768是精确F32；乘2^28恢复S16*Q13，无EQ或额外gain。 */
            double value=(double)audio->source_buffer[sample*2u+side]*268435456.0;
            if(!isfinite(value)||value<INT32_MIN||value>INT32_MAX) {result=-1;continue;}
            if(value!=(double)(int32_t)value || (int32_t)value%8192) {result=-1;continue;}
            int64_t sum=(int64_t)fm[sample][side]+(int32_t)value;
            if(sum<INT32_MIN||sum>INT32_MAX)result=-1;
            else fm[sample][side]=(int32_t)sum;
        }
        if(n<count)voice_retire(audio,voice,FD2_AUDIO_RETIRE_FINISHED);
    }
    /* 原MixData全channel先EndFrame，再做共同DC/record。whole48一次即end，
     * 后继真正tick的全局rendered48门不会再EndFrame。 */
    if(whole==48 && audio->original_mixer &&
        fd2_audio_sb_end_frame(&audio->original_sb)<0) result=-1;
    size_t valid=0;
    const int32_t record[2]={(int32_t)(audio->master_gain*8192.0f),
                             (int32_t)(audio->master_gain*8192.0f)};
    if(!result)for(size_t sample=0;sample<count;sample++) {
        fd2_mixdown_frame frame;
        if(fd2_mixdown_dc_step(&audio->original_dc,fm[sample],
            audio->original_sb_work[begin+sample],record,1,&frame)<0) {result=-1;break;}
        audio->mix_buffer[sample*2u]=(float)frame.pcm[0]/32768.0f;
        audio->mix_buffer[sample*2u+1u]=(float)frame.pcm[1]/32768.0f;
        valid++;
    }
    audio->original_rendered=whole;
    audio->closed_frames+=count;
    if(valid)submit_chunk(audio,valid);
    if(result)record_first_error(audio,FD2_AUDIO_ERROR_SOURCE);
    audio->original_busy=0;
    return result;
}
static int original_event_time(fd2_audio *audio,uint64_t *tick,double *index) {
    const fd2_sb_end_event *event=&audio->original_sb.dma.end;
    if(!audio->original_bound || !event->pending)return 0;
    double integral;
    if(!isfinite(event->index)||event->index<0)goto invalid;
    *index=modf(event->index,&integral);
    if(integral>=(double)UINT64_MAX || (uint64_t)integral>UINT64_MAX-event->tick)goto invalid;
    *tick=event->tick+(uint64_t)integral;
    if(*tick<audio->original_ticks)goto invalid;
    return 1;
invalid:
    record_first_error(audio,FD2_AUDIO_ERROR_SOURCE);
    audio->original_sb.dma.end.pending=0; /* H异常记录，禁止坏event无界重放。 */
    return 0;
}
static void original_prefix(fd2_audio *audio,uint64_t whole,uint32_t fraction) {
    if(audio->original_quiesced)return;
    uint64_t elapsed=whole-audio->origin_ns;
    uint64_t limit_tick=elapsed/UINT64_C(1000000);
    double limit_index=((double)(elapsed%UINT64_C(1000000))+
        (double)fraction/FD2_BIOS_PIT_RATE)/1000000.0;
    for(;;) {
        uint64_t event_tick=0;double event_index=0;
        int event=original_event_time(audio,&event_tick,&event_index);
        int due=event && (event_tick<limit_tick ||
            (event_tick==limit_tick && event_index<=limit_index));
        /* 同刻先关闭1ms音频窗，再处理短尾；不重放已提交样本。 */
        if(due && event_tick<audio->original_ticks+1u) {
            audio->original_point=(fd2_sb_point){event_tick,event_index,event_index,1};
            if(fd2_audio_sb_dispatch_end(&audio->original_sb,audio->original_sb.dma.end.serial)<0) {
                record_first_error(audio,FD2_AUDIO_ERROR_SOURCE);
                audio->original_sb.dma.end.pending=0;
            }
            original_deliver_after_call(audio);
        } else if(audio->original_ticks<limit_tick) {
            audio->original_point=(fd2_sb_point){audio->original_ticks,1.0,1.0,0};
            (void)original_mix_until(audio,48);
            audio->original_ticks++;
            audio->original_rendered=0;
            memset(audio->original_sb_work,0,sizeof(audio->original_sb_work));
            audio->original_point=(fd2_sb_point){audio->original_ticks,0,0,0};
            original_deliver_after_call(audio);
        } else if(due) {
            audio->original_point=(fd2_sb_point){event_tick,event_index,event_index,1};
            if(fd2_audio_sb_dispatch_end(&audio->original_sb,audio->original_sb.dma.end.serial)<0) {
                record_first_error(audio,FD2_AUDIO_ERROR_SOURCE);
                audio->original_sb.dma.end.pending=0;
            }
            original_deliver_after_call(audio);
        } else break;
    }
    audio->original_point=(fd2_sb_point){audio->original_ticks,limit_index,limit_index,0};
}
int fd2_audio_is_original_mixer(const fd2_audio *audio) {
    return audio && audio->original_mixer;
}
int fd2_audio_original_fill_up(fd2_audio *audio) {
    if(!audio || !audio->original_mixer)return 0;
    if(audio!=active_audio || audio->thread!=SDL_GetCurrentThreadID() || audio->closing)return -1;
    if(audio->original_quiesced)return 0;
    return original_global_fill(audio); /* 不sync、不采wall、不重入BIOS。 */
}
int fd2_audio_bind_dig(fd2_audio *audio,struct fd2_audio_dig *dig,
                       fd2_dig_external_sb16 *external) {
    if(!owner(audio)||!audio->original_mixer||audio->original_quiesced||audio->original_bound||!dig||!external)return -1;
    if(fd2_audio_sb_bind_dig(&audio->original_sb,dig)<0)return -1;
    audio->original_bound=1;
    /* 已在同Audio窗口消费过的SB继续保留；bind不再造新零点/重放FM。 */
    return fd2_audio_sb_external(&audio->original_sb,external);
}
int fd2_audio_quiesce_dig(fd2_audio *audio) {
    if(!audio || !audio->original_mixer)return -1;
    if(audio->original_quiesced)return 0;
    if(!owner(audio))return -1;
    int result=fd2_audio_sync(audio);
    audio->original_quiescing=1;
    if(audio->original_mixer) {
        fd2_sb_dma_provider *sb=&audio->original_sb.dma;
        /* 关闭旧声音前缀后停缓冲和短尾；不回写P位置或取消另一handle。 */
        sb->irq_pending=0;
        if(sb->mode!=FD2_SB_NONE && original_global_fill(audio)<0)result=-1;
        sb->mode=FD2_SB_NONE;sb->left=sb->total=0;sb->end.pending=0;sb->irq_pending=0;
    }
    /* 停止后继混音借用，随后可关闭clock并释放资源。 */
    audio->original_quiesced=1;audio->original_quiescing=0;
    return result;
}

#endif
