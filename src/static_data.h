/* 冷流程的唯一已生成实例；不拥有session、存档或任何资源句柄。 */
#ifndef FD2_STATIC_DATA_H
#define FD2_STATIC_DATA_H
#include "static_flow.h"
const fd2_static_flow *fd2_static_flow_lookup(const char *domain, const char *id);
#endif
