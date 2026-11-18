/* v3采集载体：原S16精确F32容器的全值域回译；无设备/GUI/线程运行。
 * 只测试实际worker所用静态数值叶，不复制产品量化式作另一份实现。 */
#include "../audio_pcm_capture.c"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"quantizer:%d: %s\n",__LINE__,#x);return 1; } } while(0)
int main(void) {
    for(int32_t sample=INT16_MIN;sample<=INT16_MAX;sample++) {
        float carrier=(float)sample/32768.0f;
        CHECK(pcm_quantize_s16_unit(carrier)==sample);
    }
    CHECK(pcm_quantize_s16_unit(1.0f)==INT16_MAX);
    CHECK(pcm_quantize_s16_unit(-1.0f)==INT16_MIN);
    CHECK(pcm_quantize_s16_unit(0.5f/32768.0f)==1);
    CHECK(pcm_quantize_s16_unit(-0.5f/32768.0f)==-1);
    CHECK(pcm_quantize_s16_unit(32767.5f/32768.0f)==INT16_MAX);
    puts("PCM v3: all 65536 S16 carriers and explicit half-away/saturation passed; no device");
    return 0;
}
