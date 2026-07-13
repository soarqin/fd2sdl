#ifndef FD2_FIELD_PATH_H
#define FD2_FIELD_PATH_H

#include <stddef.h>
#include <stdint.h>

#define FD2_FIELD_PATH_UNREACHABLE UINT32_MAX
#define FD2_FIELD_PATH_RESULT_INITIALIZER {0}

/* 单步查询结果。can_stop 与 can_expand 分离，用于表达原版规则中的
 * 「友军格可穿过但不能停留」和「敌方控制区可停留但不能继续移动」。 */
typedef struct {
    uint32_t cost;
    uint8_t can_stop;
    uint8_t can_expand;
} fd2_field_path_step;

typedef int (*fd2_field_path_query)(void *context,
                                    int from_x, int from_y,
                                    int to_x, int to_y,
                                    fd2_field_path_step *step);

typedef struct {
    uint32_t distance;
    int32_t previous;
    uint8_t direction;
    uint8_t can_stop;
    uint8_t can_expand;
    uint8_t visited;
} fd2_field_path_node;

typedef struct {
    int width;
    int height;
    int start_x;
    int start_y;
    uint32_t budget;
    fd2_field_path_node *nodes;
} fd2_field_path_result;

/* 参数化四邻域 Dijkstra。query 返回 1 表示允许该边，0 表示阻挡，
 * -1 表示数据错误。平价路径保留最先发现者；邻域顺序固定为右、左、下、上，
 * 与原版 field reachable/path helper 的展开顺序一致。
 *
 * result 是拥有堆内存的输出对象；首次使用前必须以
 * FD2_FIELD_PATH_RESULT_INITIALIZER 或 `{0}` 初始化。compute 可复用同一对象，
 * 并会先释放旧结果；最终必须调用 fd2_field_path_close()。 */
int fd2_field_path_compute(fd2_field_path_result *result,
                           int width, int height,
                           int start_x, int start_y,
                           uint32_t budget,
                           fd2_field_path_query query,
                           void *context);
int fd2_field_path_compute_with_start_policy(
    fd2_field_path_result *result,
    int width, int height,
    int start_x, int start_y,
    uint32_t budget,
    int start_can_stop, int start_can_expand,
    fd2_field_path_query query,
    void *context);
void fd2_field_path_close(fd2_field_path_result *result);

int fd2_field_path_is_destination(const fd2_field_path_result *result,
                                  int x, int y);
uint32_t fd2_field_path_distance(const fd2_field_path_result *result,
                                 int x, int y);

/* 重建从起点到目标的方向序列：0 下、1 左、2 上、3 右。
 * 返回步数；不可达、目标不可停留或容量不足时返回 -1。 */
int fd2_field_path_build(const fd2_field_path_result *result,
                         int target_x, int target_y,
                         uint8_t *directions, size_t capacity);

#endif /* FD2_FIELD_PATH_H */
