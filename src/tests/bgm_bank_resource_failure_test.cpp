/* MIT：普通BGM音色表缺失／resize失败，关闭FILE且不发布播放器。
 * 全局new替身只在本私有process对3242B加载触发；不注入产品状态/时钟。
 * 不是内存耗尽模拟Main/全部BGM功能，正常源数学不因此重跑。 */
#include <cstdlib>
#include <new>
#include <cstdio>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
static bool bank_fail=false;static unsigned bank_faults=0;
/* 保留自定义分配和释放入口，避免 GCC 内联后把配对的 new/delete 当成错配。 */
#if defined(__GNUC__)
[[gnu::noinline]]
#endif
void*operator new(std::size_t n){if(bank_fail&&n==3242){bank_fail=false;++bank_faults;throw std::bad_alloc();}if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
#if defined(__GNUC__)
[[gnu::noinline]]
#endif
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{::operator delete(p);}
#include "../bgm.cpp"
#include "audio_clock_fixture.h"
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line%d %s\n",__LINE__,#x);return 1;}}while(0)
static unsigned handles(){
#if defined(_WIN32)
 DWORD n=0;if(!GetProcessHandleCount(GetCurrentProcess(),&n))std::abort();return unsigned(n);
#else
 unsigned n=0;const long bound=sysconf(_SC_OPEN_MAX);for(int f=0;f<bound;++f)if(fcntl(f,F_GETFD)!=-1)++n;return n;
#endif
}
int main(){
 std::array<AilInstrument,kMaxInstruments>instruments{};
 const unsigned first=handles();CHECK(!load_ail_bank("__fd2_missing_bank_resource__",instruments));CHECK(handles()==first);
 bank_fail=true;CHECK(!load_ail_bank("original_game/SAMPLE.OPL",instruments));CHECK(!bank_fail&&bank_faults==1&&handles()==first);
 fd2_audio_config ac{};ac.sample_rate=48000;ac.allow_null=1;
 fd2_audio*audio=fd2_audio_create(&ac);CHECK(audio&&fd2_test_audio_clock_prepare(audio)==0);
 fd2_bgm_config config{};config.fdmus_path="original_game/FDMUS.DAT";config.ail_bank_path="original_game/SAMPLE.OPL";config.sample_rate=48000;
 const fd2_ail_timer before=*fd2_bios_clock_ail_state();const unsigned live=handles();
 bank_fail=true;CHECK(fd2_bgm_create(audio,&config)==nullptr);CHECK(!bank_fail&&bank_faults==2&&handles()==live);
 CHECK(!std::memcmp(&before,fd2_bios_clock_ail_state(),sizeof(before)));
 CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
 std::puts("{\"passed\":true,\"families\":3,\"resize_failures\":2,\"file_handles_preserved\":true,\"failed_create_preserves_timer\":true}");return 0;
}
