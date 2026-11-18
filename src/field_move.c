/* 炎龙骑士团 2 SDL3 重写 - 战场移动查询策略
 *
 * 逆向依据：
 * - field_movement_profile_get @0x4e8a5：record offset 0x20 选择 20 字节
 *   movement profile；
 * - field_reachable_cells_compute @0x4e390：record offset 0x3b 作为预算；
 * - FUN_0004bc06 / FUN_0004bdc8：FDSHAP attr[1] 索引 profile 中的成本；
 * - field_opponent_zoc_build @0x145cd、field_opponent_zoc_mark_unit
 *   @0x14625、field_cell_zoc_mark @0x146a7：敌方格标记 0x40，四邻格
 *   标记 0x80；
 * - field_friendly_destination_block @0x146d1：同阵营其他单位格在范围
 *   传播后设为不可停留。
 */

#include "field_move.h"

#include <stdlib.h>
#include <string.h>

static int same_faction(uint8_t a, uint8_t b) {
    return (a == 0) == (b == 0);
}

static int hostile_occupied(const fd2_field_move_query_context *context,
                            int x, int y) {
    if (!context || !context->units ||
        context->mover_index >= context->units->count)
        return 0;
    const fd2_field_unit *mover =
        &context->units->items[context->mover_index];
    for (size_t i = 0; i < context->units->count; i++) {
        const fd2_field_unit *unit = &context->units->items[i];
        if (i == context->mover_index ||
            (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            unit->x != x || unit->y != y)
            continue;
        if (!same_faction(mover->side, unit->side)) return 1;
    }
    return 0;
}

int fd2_field_move_unit_at(const fd2_field_units *units,
                           int x, int y,
                           int ignore_index,
                           int visible_only) {
    if (!units || x < 0 || x > 0xff || y < 0 || y > 0xff) return -1;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if ((int)i == ignore_index) continue;
        if (visible_only &&
            (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0)
            continue;
        if (unit->x == x && unit->y == y) return (int)i;
    }
    return -1;
}

static int opponent_adjacent(const fd2_field_move_query_context *context,
                             int x, int y) {
    const fd2_field_unit *mover =
        &context->units->items[context->mover_index];
    for (size_t i = 0; i < context->units->count; i++) {
        const fd2_field_unit *unit = &context->units->items[i];
        if (i == context->mover_index ||
            (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            same_faction(mover->side, unit->side))
            continue;
        int dx = abs((int)unit->x - x);
        int dy = abs((int)unit->y - y);
        if (dx + dy == 1) return 1;
    }
    return 0;
}

/* 4e4df..4e4e1和4e6a5..4e6a9均比较signed BYTE；用int域表达，
 * 不依赖超范围uint8_t转int8_t的实现定义行为。 */
static int signed_byte(uint8_t value) {
    return value < 0x80u ? (int)value : (int)value - 0x100;
}

typedef struct {
    size_t index;
    uint8_t remaining;
    uint8_t next_neighbor;
} move_range_frame;

/* field_reachable_cells_compute @0x4e390（原登记同名）及4e42c/4e4be：
 * 在同一raw3上按R/L/D/U深搜，SUB的借位门为unsigned，改善门为signed JLE。
 * 这是有限地图参数的值传播；原共享60079 scratch的跨global深写资格另记，
 * 不以C frame数组大小推导原U域，也不把previous冒作4e4f6的路径。 */
static int range_propagate(fd2_field_map *map,
                            const fd2_terrain_tileset *terrain,
                            const uint8_t *costs, size_t cost_count,
                            size_t start, uint8_t budget) {
    size_t capacity = 64u, depth = 1u;
    move_range_frame *frames = malloc(capacity * sizeof(*frames));
    if (!frames) return -1;
    map->cells[start] = (map->cells[start] & UINT32_C(0x00ffffff)) |
                        ((uint32_t)budget << 24);
    frames[0] = (move_range_frame){start, budget, 0};
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    int result = 0;
    while (depth) {
        move_range_frame *frame = &frames[depth - 1u];
        if (frame->next_neighbor == 4u) { depth--; continue; }
        unsigned neighbor = frame->next_neighbor++;
        int x = (int)(frame->index % (size_t)map->width) + dx[neighbor];
        int y = (int)(frame->index / (size_t)map->width) + dy[neighbor];
        if (x < 0 || y < 0 || x >= map->width || y >= map->height) continue;
        size_t index = (size_t)y * (size_t)map->width + (size_t)x;
        uint32_t cell = map->cells[index];
        uint16_t terrain_id = (uint16_t)(cell & 0x03ffu);
        if (terrain_id >= terrain->attr_count) { result = -1; break; }
        uint8_t kind = terrain->attrs[terrain_id].movement_cost_class;
        if (kind >= cost_count) { result = -1; break; }
        uint8_t cost = costs[kind];
        if (cost > frame->remaining) continue; /* SUB CL,cost / JB */
        uint8_t remaining = (uint8_t)(frame->remaining - cost);
        if (signed_byte(remaining) <= signed_byte((uint8_t)(cell >> 24))) continue;
        if (cell & UINT32_C(0x00400000)) continue;
        if (cell & UINT32_C(0x00800000)) remaining = 0;
        map->cells[index] = (cell & UINT32_C(0x00ffffff)) |
                            ((uint32_t)remaining << 24);
        if (depth == capacity) {
            if (capacity > SIZE_MAX / 2u / sizeof(*frames)) { result = -1; break; }
            size_t next = capacity * 2u;
            move_range_frame *grown = realloc(frames, next * sizeof(*frames));
            if (!grown) { result = -1; break; }
            frames = grown; capacity = next;
        }
        frames[depth++] = (move_range_frame){index, remaining, 0};
    }
    free(frames);
    return result;
}

/* 145cd/14625/146a7：只OR敌方40/邻格80；146d1的友方FF在传播后执行。 */
static int range_mark_units(fd2_field_map *map, const fd2_field_units *units,
                             size_t mover_index) {
    const fd2_field_unit *mover = &units->items[mover_index];
    static const int dx[] = {-1, 0, 1, 0};
    static const int dy[] = {0, -1, 0, 1};
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) continue;
        if (unit->x >= map->width || unit->y >= map->height) return -1;
        if (same_faction(mover->side, unit->side)) continue;
        size_t index = (size_t)unit->y * (size_t)map->width + unit->x;
        map->cells[index] |= UINT32_C(0x00400000);
        for (size_t d = 0; d < 4u; d++) {
            int x = (int)unit->x + dx[d], y = (int)unit->y + dy[d];
            if (x >= 0 && y >= 0 && x < map->width && y < map->height)
                map->cells[(size_t)y * (size_t)map->width + (size_t)x] |=
                    UINT32_C(0x00800000);
        }
    }
    return 0;
}

static int range_compute(fd2_field_path_result *result,
                           const fd2_field_map *map, fd2_field_map *map_owner,
                           const fd2_terrain_tileset *terrain,
                           const fd2_field_units *units, size_t mover_index,
                           const uint8_t *costs, size_t cost_count,
                           uint32_t movement_points) {
    if (!result || !map || !map->cells || !terrain || !terrain->attrs || !units ||
        units->count > FD2_FIELD_MAX_UNITS || mover_index >= units->count ||
        !costs || !cost_count || movement_points > UINT8_MAX ||
        map->width <= 0 || map->height <= 0 ||
        map->width > UINT8_MAX || map->height > UINT8_MAX)
        return -1;
    const fd2_field_unit *mover = &units->items[mover_index];
    if (mover->x >= map->width || mover->y >= map->height) return -1;
    size_t count = (size_t)map->width * (size_t)map->height;
    uint32_t *cells = malloc(count * sizeof(*cells));
    fd2_field_path_node *nodes = calloc(count, sizeof(*nodes));
    if (!cells || !nodes) { free(cells); free(nodes); return -1; }
    memcpy(cells, map->cells, count * sizeof(*cells));
    /* 纯查询以4df4c后的隔离copy为参数；正式18890入口没有此reset，
     * map_owner分支保留真实owner，不能为任意夹具偷偷清raw3。 */
    if (!map_owner)
        for (size_t i = 0; i < count; i++)
            cells[i] = (cells[i] & UINT32_C(0x001f03ff)) | UINT32_C(0xff000000);
    fd2_field_map work = *map; work.cells = cells;
    size_t start = (size_t)mover->y * (size_t)map->width + mover->x;
    if (range_mark_units(&work, units, mover_index) != 0 ||
        range_propagate(&work, terrain, costs, cost_count, start,
                         (uint8_t)movement_points) != 0) {
        free(cells); free(nodes); return -1;
    }
    for (size_t i = 0; i < count; i++) {
        uint8_t remaining = (uint8_t)(cells[i] >> 24);
        nodes[i].previous = -1; /* 正式路径只归4e4f6，range没有前驱合同。 */
        nodes[i].distance = remaining == UINT8_MAX
            ? FD2_FIELD_PATH_UNREACHABLE : (uint8_t)((uint8_t)movement_points - remaining);
        nodes[i].can_stop = remaining != UINT8_MAX;
        nodes[i].can_expand = remaining != UINT8_MAX &&
            (i == start || !(cells[i] & UINT32_C(0x00800000)));
        nodes[i].visited = remaining != UINT8_MAX;
    }
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if (i == mover_index || (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) ||
            !same_faction(mover->side, unit->side)) continue;
        size_t index = (size_t)unit->y * (size_t)map->width + unit->x;
        cells[index] |= UINT32_C(0xff000000);
        nodes[index].can_stop = 0; /* distance保传播值，供14d17友方屏蔽前查询。 */
    }
    fd2_field_path_close(result);
    *result = (fd2_field_path_result){map->width, map->height,
        mover->x, mover->y, movement_points, nodes};
    if (map_owner) memcpy(map_owner->cells, cells, count * sizeof(*cells));
    free(cells);
    return 0;
}

int fd2_field_move_range_compute(fd2_field_path_result *result,
                                 const fd2_field_map *map,
                                 const fd2_terrain_tileset *terrain,
                                 const fd2_field_units *units,
                                 size_t mover_index,
                                 const uint8_t *costs, size_t cost_count,
                                 uint32_t movement_points) {
    return range_compute(result, map, NULL, terrain, units, mover_index,
                           costs, cost_count, movement_points);
}

int fd2_field_move_range_apply(fd2_field_path_result *result,
                               fd2_field_map *map,
                               const fd2_terrain_tileset *terrain,
                               const fd2_field_units *units,
                               size_t mover_index,
                               const uint8_t *costs, size_t cost_count,
                               uint32_t movement_points) {
    return range_compute(result, map, map, terrain, units, mover_index,
                           costs, cost_count, movement_points);
}

typedef struct {
    fd2_field_move_query_context query;
    uint8_t mode;
    int clear_unit_flags;
    int target_x;
    int target_y;
    uint8_t *best_remaining;
    uint8_t *best_turn_score;
    uint8_t *stack;
    size_t stack_capacity;
    fd2_field_move_path_output *output;
    int failed;
} fd2_field_move_path_search;

/* 60073的同一有界输出视图；原每次从offset0写，保留未覆盖尾。
 * 容量不足保合法写前缀；这是C适配H边界，不推原caller的usable extent。 */
static int path_output_write(fd2_field_move_path_search *search,
                              size_t offset,uint8_t value) {
    fd2_field_move_path_output *out=search->output;
    if(!out->bytes || offset>=out->capacity) {search->failed=1;return -1;}
    out->bytes[offset]=value;
    if(out->written<offset+1u)out->written=offset+1u;
    return 0;
}

/* sub_4E751 @0x4e751：先写60078，再逐byte复制当前方向prefix；
 * caller在进入child frame的DX覆mode之前调用，不能挪到child entry之后。 */
static void path_record_target(fd2_field_move_path_search *search,
                                 int x,int y,size_t depth) {
    if(x!=search->target_x || y!=search->target_y)return;
    uint8_t length=(uint8_t)depth;
    if(length>search->output->result)return;
    search->output->result=length;
    for(size_t i=0;i<length;i++)
        if(path_output_write(search,i,search->stack[i])!=0)return;
}

static uint8_t path_turn_score(const uint8_t *directions, size_t length) {
    if (!directions || length == 0) return 0;
    uint8_t runs = 1; /* 4e732 BH=FF，首个direction也计一段。 */
    for (size_t i = 1; i < length; i++)
        if (directions[i] != directions[i - 1u]) runs++;
    /* 4e74b SHL AL,2是BYTE写；不饱和为FC。 */
    return (uint8_t)(runs << 2);
}

static int path_cell_cost(const fd2_field_move_path_search *search,
                          int x, int y, uint8_t *cost) {
    if (!search || !cost || x < 0 || y < 0 ||
        x >= search->query.map->width || y >= search->query.map->height)
        return -1;
    uint32_t cell = search->query.map->cells[
        (size_t)y * (size_t)search->query.map->width + (size_t)x];
    uint16_t terrain_id = (uint16_t)(cell & 0x03ffu);
    if (!search->query.terrain->attrs ||
        terrain_id >= search->query.terrain->attr_count)
        return -1;
    uint8_t cost_class =
        search->query.terrain->attrs[terrain_id].movement_cost_class;
    if (cost_class >= search->query.movement_profile_cost_count) return -1;
    *cost = search->query.movement_profile_costs[cost_class];
    return 0;
}

static void move_path_search_visit(fd2_field_move_path_search *search,
                                   int x, int y,
                                   uint8_t remaining,
                                   size_t depth) {
    if (!search || search->failed || depth > search->stack_capacity) return;
    /* 4e5ce按8B frame写DX：60079+32*8的高byte即6017a。
     * index31的EBX止于60178，与计数60077/结果60078无关；pop不恢复mode。
     * 这里只表达当前搜索的已证alias，60181+外部表collateral另属F6。 */
    if(depth==32u)search->mode=(uint8_t)y;
    search->output->mode=search->mode;

    /* field_path_find @0x4e4f6 / 正确 dual 0x4e4f6 的递归顺序。 */
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    static const uint8_t direction[] = {3, 1, 0, 2};
    for (size_t d = 0; d < 4u && !search->failed; d++) {
        int nx = x + dx[d];
        int ny = y + dy[d];
        if (nx < 0 || ny < 0 || nx >= search->query.map->width ||
            ny >= search->query.map->height)
            continue;
        uint8_t cost = 0;
        if (path_cell_cost(search, nx, ny, &cost) != 0) {
            search->failed = 1;
            return;
        }
        if (cost > remaining) continue;
        uint8_t next_remaining = (uint8_t)(remaining - cost);
        int hostile = !search->clear_unit_flags &&
            hostile_occupied(&search->query, nx, ny);
        size_t index = (size_t)ny * (size_t)search->query.map->width +
                       (size_t)nx;
        if (depth >= search->stack_capacity) continue;
        search->stack[depth] = direction[d];
        uint8_t turn_score = path_turn_score(search->stack, depth + 1u);
        /* 4e6a5 signed JL/JG；初始raw3=FF、raw1高六bit=0。
         * FF不是另一个visited owner，0剩余也必须参与改善/平价门。 */
        int previous = signed_byte(search->best_remaining[index]);
        int candidate = signed_byte(next_remaining);
        if (candidate < previous ||
            (candidate == previous &&
             (search->mode != FD2_FIELD_MOVE_PATH_EQUAL_ROUTE ||
              turn_score <= search->best_turn_score[index])))
            continue;

        search->best_turn_score[index] = turn_score;
        if(search->mode==FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS) {
            search->best_remaining[index]=next_remaining;
            if(hostile) {
                /* 4e703按X、Y、result1顺序，沿同一个60073输出写；
                 * requested_mode不决定该次实际输出类型。 */
                if(path_output_write(search,0,(uint8_t)nx)!=0 ||
                   path_output_write(search,1,(uint8_t)ny)!=0)return;
                search->output->result=1;
            }
        } else {
            /* 4e6d2的raw1高位写在40拒绝之前；不能提前跳过敌占格。 */
            if(hostile)continue;
            if(!search->clear_unit_flags && opponent_adjacent(&search->query,nx,ny))
                next_remaining=0;
            search->best_remaining[index]=next_remaining;
            path_record_target(search,nx,ny,depth+1u);
            if(search->failed)return;
        }
        move_path_search_visit(
            search, nx, ny, next_remaining, depth + 1u);
    }
}

int fd2_field_move_path_find_raw(const fd2_field_map *map,
                             const fd2_terrain_tileset *terrain,
                             const fd2_field_units *units,
                             size_t mover_index,
                             const uint8_t *movement_profile_costs,
                             size_t movement_profile_cost_count,
                             uint32_t movement_points,
                             fd2_field_move_path_mode mode,
                             int target_x, int target_y,
                             fd2_field_move_path_output *output) {
    if (!map || !map->cells || map->width <= 0 || map->height <= 0 ||
        !terrain || !units || mover_index >= units->count ||
        !movement_profile_costs || movement_profile_cost_count == 0 ||
        movement_points > UINT8_MAX ||
        ((unsigned)mode & ~7u) != 0u ||
        ((unsigned)mode & 3u) > FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS ||
        (((unsigned)mode & FD2_FIELD_MOVE_PATH_CLEAR_UNIT_FLAGS) != 0u &&
         ((unsigned)mode & 3u) != FD2_FIELD_MOVE_PATH_EQUAL_ROUTE) ||
        !output ||
        (mode != FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS &&
         (target_x < 0 || target_y < 0 || target_x >= map->width ||
          target_y >= map->height)))
        return -1;
    if ((size_t)map->width > SIZE_MAX / (size_t)map->height) return -1;
    size_t count = (size_t)map->width * (size_t)map->height;
    const fd2_field_unit *mover = &units->items[mover_index];
    if (count > SIZE_MAX / 3u || mover->x >= map->width || mover->y >= map->height)
        return -1;
    uint8_t *work = calloc(count * 3u, sizeof(uint8_t));
    if (!work) return -1;

    output->written=0;
    output->result=UINT8_MAX;
    output->mode=(uint8_t)((unsigned)mode&3u);
    fd2_field_move_path_search search = {
        .query = {
            .map = map,
            .terrain = terrain,
            .units = units,
            .mover_index = mover_index,
            .movement_profile_costs = movement_profile_costs,
            .movement_profile_cost_count = movement_profile_cost_count,
        },
        .mode = output->mode,
        .clear_unit_flags = ((unsigned)mode & FD2_FIELD_MOVE_PATH_CLEAR_UNIT_FLAGS) != 0u,
        .target_x = target_x,
        .target_y = target_y,
        .best_remaining = work,
        .best_turn_score = work + count,
        .stack = work + count * 2u,
        .stack_capacity = count,
        .output=output,
    };
    memset(search.best_remaining, UINT8_MAX, count);
    search.best_turn_score[
        (size_t)mover->y * (size_t)map->width + (size_t)mover->x] = 0;
    search.best_remaining[
        (size_t)mover->y * (size_t)map->width + (size_t)mover->x] =
        (uint8_t)movement_points;
    path_record_target(&search,mover->x,mover->y,0); /* 4e5b8不按mode跳过root。 */
    if(!search.failed)move_path_search_visit(
        &search, mover->x, mover->y, (uint8_t)movement_points, 0);
    free(work);
    return search.failed ? -2 : 0;
}

int fd2_field_move_path_find(const fd2_field_map *map,
                             const fd2_terrain_tileset *terrain,
                             const fd2_field_units *units,size_t mover_index,
                             const uint8_t *costs,size_t cost_count,
                             uint32_t movement_points,fd2_field_move_path_mode mode,
                             int target_x,int target_y,uint8_t *directions,size_t capacity,
                             size_t *path_length,int *witness_x,int *witness_y) {
    if(!path_length)return -1;
    /* typed witness只投影实际已写的两byte。可显式借directions给足完整
     * raw输出域；旧NULL调用仅有2B局部视图，转方向后超域明确报H错误，
     * 不推原14121真实frame/output extent为2B或首U。 */
    uint8_t xy[2]={0,0};
    int witness=mode==FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS;
    fd2_field_move_path_output out={.bytes=witness && !directions?xy:directions,
        .capacity=witness && !directions?sizeof(xy):capacity};
    int status=fd2_field_move_path_find_raw(map,terrain,units,mover_index,costs,cost_count,
        movement_points,mode,target_x,target_y,&out);
    if(status==-1)return -1; /* 校验／分配失败尚未开始，保旧typed输出。 */
    *path_length=0;
    if(witness_x)*witness_x=-1;
    if(witness_y)*witness_y=-1;
    if(status<0)return -1;
    if(out.result==UINT8_MAX)return 0;
    if(witness) {
        if(out.written<2u)return 0; /* root匹配result0未写XY，不读局部初值。 */
        if(witness_x)*witness_x=out.bytes[0];
        if(witness_y)*witness_y=out.bytes[1];
    } else *path_length=out.result;
    return 1;
}
