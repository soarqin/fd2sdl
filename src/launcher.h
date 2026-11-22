#ifndef FD2_LAUNCHER_H
#define FD2_LAUNCHER_H
#include "settings.h"
bool fd2_setup_requested(const fd2_config_file *config, bool development_mode);
/* 1 启动游戏，0 仅保存或退出，-1 宿主错误。 */
int fd2_launcher_run(const fd2_config_file *config);
#endif
