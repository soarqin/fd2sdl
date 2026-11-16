/* 仅F4私有TU：复用04的解析/握手/seed/emit，无窗口/源文件写入。
 * 探针调用正式text/summary/credits叶，不实现第二formatter/decoder。
 * 双底mask仅表明目标值是否独立于before，不冒充bridge物理store次数。 */
#ifndef FD2_F4_TEXT_PROBE_COMMON_H
#define FD2_F4_TEXT_PROBE_COMMON_H
#include "field_page_private.h"
static int f4_text_run(const char *root);
static int f4_text_cli(const char *root) {
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1 || _setmode(_fileno(stdin),_O_BINARY)==-1)return 1;
#endif
    int result=f4_text_run(root);
    if(result){fputs("{\"schema\":\"fd2-f4-reader-v1\",\"status\":1,\"payload_bytes\":0}\n",stdout);fflush(stdout);}
    return result;
}
#endif
