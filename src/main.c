#include "raylib.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef uint32_t u32;
typedef int32_t  i32;
typedef float    f32;
typedef double   f64;

typedef struct {
    i32 x, y;
} Vector2i;

typedef struct {
    u32      cap;
    u32      len;
    Vector2* buffer;
} Rays;

typedef struct {
    i32 x[2];
    i32 y;
} Horizontal;

typedef struct {
    i32 x;
    i32 y[2];
} Vertical;

typedef struct {
    Vector2i points[2];
} Line;

#define FPS_X 10
#define FPS_Y FPS_X

#define SCREEN_X 1536
#define SCREEN_Y 768

#define PLAYER_X 20
#define PLAYER_Y 30

#define RUN      4.0f
#define FRICTION 0.75f

#define FOV 0.5f

#define EPSILON 0.00001f

#define WALL_DISTANCE 500
#define WALL_WIDTH    10

#define DOOR_GAP 150

#define NODE_RADIUS 10
#define DOOR_RADIUS NODE_RADIUS

#define CAP_RECTS (1 << 8)
static Rectangle RECTS[CAP_RECTS];
static u32       LEN_RECTS = 0;

#define CAP_SUBSET (1 << 7)
static Rectangle SUBSET[CAP_SUBSET];
static u32       LEN_SUBSET = 0;

#define CAP_HORIZONTALS (1 << 6)
static Horizontal HORIZONTALS[CAP_HORIZONTALS];
static u32        LEN_HORIZONTALS = 0;

#define CAP_VERTICALS (1 << 6)
static Vertical VERTICALS[CAP_VERTICALS];
static u32      LEN_VERTICALS = 0;

#define CAP_SPLITS (1 << 6)
static i32 SPLITS[CAP_SPLITS];
static u32 LEN_SPLITS = 0;

#define CAP_NODES (1 << 6)
static Line NODES[CAP_NODES];
static u32  LEN_NODES = 0;

#define CAP_DOORS (1 << 5)
static Vector2i DOORS[CAP_DOORS];
static u32      LEN_DOORS = 0;

static void rects_push(const Rectangle rect) {
    assert((0 < rect.width) && (0 < rect.height));

    assert(LEN_RECTS < CAP_RECTS);
    RECTS[LEN_RECTS++] = rect;
}

static void subset_push(const Rectangle rect) {
    assert(LEN_SUBSET < CAP_SUBSET);
    SUBSET[LEN_SUBSET++] = rect;
}

static void horizontals_push(const Horizontal horizontal) {
    assert(horizontal.x[0] < horizontal.x[1]);

    assert(LEN_HORIZONTALS < CAP_HORIZONTALS);
    HORIZONTALS[LEN_HORIZONTALS++] = horizontal;
}

static void verticals_push(const Vertical vertical) {
    assert(vertical.y[0] < vertical.y[1]);

    assert(LEN_VERTICALS < CAP_VERTICALS);
    VERTICALS[LEN_VERTICALS++] = vertical;
}

static void splits_push(const i32 split) {
    assert(LEN_SPLITS < CAP_SPLITS);
    SPLITS[LEN_SPLITS++] = split;
}

static void nodes_push(const Line node) {
    assert(LEN_NODES < CAP_NODES);
    assert(node.points[0].x < node.points[1].x);
    assert(node.points[0].y < node.points[1].y);
    NODES[LEN_NODES++] = node;
}

static void doors_push(const Vector2i door) {
    assert(LEN_DOORS < CAP_DOORS);
    DOORS[LEN_DOORS++] = door;
}

static Rays rays_new(void) {
    const u32 cap = (1u << 5u);
    void*     buffer = calloc(cap, sizeof(Vector2));
    assert(buffer != NULL);
    return (Rays){cap, 0, (Vector2*)buffer};
}

static void rays_push(Rays* rays, const Vector2 ray) {
    if (rays->cap <= rays->len) {
        rays->cap <<= 1u;
        rays->buffer = reallocarray(rays->buffer, rays->cap, sizeof(Vector2));
    }

    rays->buffer[(rays->len)++] = ray;
}

static void generate_horizontal(const i32, const i32, const i32, const i32);

static void generate_vertical(const i32 x_min, const i32 x_max, const i32 y_min, const i32 y_max) {
    if ((x_max - x_min) <= WALL_DISTANCE) {
        nodes_push((Line){{{x_min, y_min}, {x_max, y_max}}});
        return;
    }

    const i32 x = GetRandomValue(x_min + (WALL_DISTANCE / 2), x_max - (WALL_DISTANCE / 2));

    verticals_push((Vertical){x, {y_min, y_max}});

    generate_horizontal(x_min, x, y_min, y_max);
    generate_horizontal(x, x_max, y_min, y_max);
}

void generate_horizontal(const i32 x_min, const i32 x_max, const i32 y_min, const i32 y_max) {
    if ((y_max - WALL_DISTANCE) <= y_min) {
        nodes_push((Line){{{x_min, y_min}, {x_max, y_max}}});
        return;
    }

    const i32 y = GetRandomValue(y_min + (WALL_DISTANCE / 2), y_max - (WALL_DISTANCE / 2));

    horizontals_push((Horizontal){{x_min, x_max}, y});

    generate_vertical(x_min, x_max, y_min, y);
    generate_vertical(x_min, x_max, y, y_max);
}

static void splits_sort(void) {
    for (u32 i = 0; i < LEN_SPLITS; ++i) {
        const i32 split = SPLITS[i];

        u32 j = i;
        for (; (0 < j) && (split < SPLITS[j - 1]); --j) {
            SPLITS[j] = SPLITS[j - 1];
        }
        SPLITS[j] = split;
    }
}

static void split_verticals(void) {
    for (u32 i = 0; i < LEN_VERTICALS; ++i) {
        LEN_SPLITS = 0;

        splits_push(VERTICALS[i].y[0]);

        for (u32 j = 0; j < LEN_HORIZONTALS; ++j) {
            if ((HORIZONTALS[j].y < VERTICALS[i].y[0]) || (VERTICALS[i].y[1] < HORIZONTALS[j].y)) {
                continue;
            }
            if ((VERTICALS[i].x != HORIZONTALS[j].x[0]) && (VERTICALS[i].x != HORIZONTALS[j].x[1]))
            {
                continue;
            }
            splits_push(HORIZONTALS[j].y);
        }

        splits_push(VERTICALS[i].y[1]);

        splits_sort();

        const i32 x = VERTICALS[i].x;

        for (u32 j = 1; j < LEN_SPLITS; ++j) {
            const i32 y_min = SPLITS[j - 1];
            const i32 y_max = SPLITS[j];
            const i32 length = y_max - y_min;

            if (length <= DOOR_GAP) {
                rects_push((Rectangle){(f32)x, (f32)y_min, WALL_WIDTH, (f32)(length + WALL_WIDTH)});
            } else {
                const i32 y = GetRandomValue(y_min + (DOOR_GAP / 2), y_max - (DOOR_GAP / 2));

                doors_push((Vector2i){x, y});

                rects_push((Rectangle){(f32)x,
                                       (f32)y_min,
                                       WALL_WIDTH,
                                       (f32)(((y - (DOOR_GAP / 2)) - y_min) + WALL_WIDTH)});
                rects_push((Rectangle){(f32)x,
                                       (f32)(y + (DOOR_GAP / 2)),
                                       WALL_WIDTH,
                                       (f32)((y_max - (y + (DOOR_GAP / 2))) + WALL_WIDTH)});
            }
        }
    }
}

static void split_horizontals(void) {
    for (u32 i = 0; i < LEN_HORIZONTALS; ++i) {
        LEN_SPLITS = 0;

        splits_push(HORIZONTALS[i].x[0]);

        for (u32 j = 0; j < LEN_VERTICALS; ++j) {
            if ((VERTICALS[j].x < HORIZONTALS[i].x[0]) || (HORIZONTALS[i].x[1] < VERTICALS[j].x)) {
                continue;
            }
            if ((HORIZONTALS[i].y != VERTICALS[j].y[0]) && (HORIZONTALS[i].y != VERTICALS[j].y[1]))
            {
                continue;
            }
            splits_push(VERTICALS[j].x);
        }

        splits_push(HORIZONTALS[i].x[1]);

        splits_sort();

        const i32 y = HORIZONTALS[i].y;

        for (u32 j = 1; j < LEN_SPLITS; ++j) {
            const i32 x_min = SPLITS[j - 1];
            const i32 x_max = SPLITS[j];
            const i32 length = x_max - x_min;

            if (length <= DOOR_GAP) {
                rects_push((Rectangle){(f32)x_min, (f32)y, (f32)(length + WALL_WIDTH), WALL_WIDTH});
            } else {
                const i32 x = GetRandomValue(x_min + (DOOR_GAP / 2), x_max - (DOOR_GAP / 2));

                doors_push((Vector2i){x, y});

                rects_push((Rectangle){(f32)x_min,
                                       (f32)y,
                                       (f32)((((x - (DOOR_GAP / 2))) - x_min) + WALL_WIDTH),
                                       WALL_WIDTH});
                rects_push((Rectangle){(f32)(x + (DOOR_GAP / 2)),
                                       (f32)y,
                                       (f32)((x_max - (x + (DOOR_GAP / 2))) + WALL_WIDTH),
                                       WALL_WIDTH});
            }
        }
    }
}

static Vector2 rotate(const Vector2 from, const Vector2 to, const f32 radians) {
    const Vector2 delta = {to.x - from.x, to.y - from.y};
    const f32     s = sinf(radians);
    const f32     c = cosf(radians);
    return (Vector2){
        from.x + (delta.x * c) + (delta.y * s),
        from.y + (-delta.x * s) + (delta.y * c),
    };
}

static Vector2 normalize(const Vector2 v) {
    const f32 l = sqrtf((v.x * v.x) + (v.y * v.y)) + EPSILON;
    return (Vector2){v.x / l, v.y / l};
}

static Vector2 extend(const Vector2 a, const Vector2 b, const f32 ac) {
    const Vector2 v = normalize((Vector2){b.x - a.x, b.y - a.y});
    return (Vector2){a.x + (v.x * ac), a.y + (v.y * ac)};
}

// NOTE: See `https://stackoverflow.com/questions/1211212/how-to-calculate-an-angle-from-three-points/31334882#31334882`.
static f32 polar_angle(const Vector2 a, const Vector2 b, const Vector2 c) {
    return atan2f(c.y - a.y, c.x - a.x) - atan2f(b.y - a.y, b.x - a.x);
}

static void intersects_at(const Vector2 a[2], const Vector2 b[2], Vector2* point) {
    const Vector2 deltas[3] = {
        {a[0].x - a[1].x, a[0].y - a[1].y},
        {a[0].x - b[0].x, a[0].y - b[0].y},
        {b[0].x - b[1].x, b[0].y - b[1].y},
    };

    const f32 denominator = (deltas[0].x * deltas[2].y) - (deltas[0].y * deltas[2].x);
    if (denominator == 0.0f) {
        return;
    }

    const f32 t = ((deltas[1].x * deltas[2].y) - (deltas[1].y * deltas[2].x)) / denominator;
    const f32 u = -((deltas[0].x * deltas[1].y) - (deltas[0].y * deltas[1].x)) / denominator;

    if ((t < 0.0f) | (1.0f < t) | (u < 0.0f) | (1.0f < u)) {
        return;
    }

    point->x = a[0].x + (t * (a[1].x - a[0].x));
    point->y = a[0].y + (t * (a[1].y - a[0].y));
}

static f32 center(f32 radians) {
    if (radians < -PI) {
        radians += 2.0f * PI;
    }
    if (PI < radians) {
        radians -= 2.0f * PI;
    }
    return radians;
}

static bool within_fov(const Vector2 from, const Vector2 to, const Vector2 ray) {
    const f32 radians = center(polar_angle(from, ray, to));
    return ((-FOV) < radians) && (radians < FOV);
}

static void update_inputs(Vector2* speed, Vector2* position, f32* direction) {
    Vector2 move = {0};
    bool    flag = false;

    if (IsKeyDown(KEY_A)) {
        move.x -= 1.0f;
        flag = true;
    }
    if (IsKeyDown(KEY_D)) {
        move.x += 1.0f;
        flag = true;
    }
    if (IsKeyDown(KEY_W)) {
        move.y -= 1.0f;
        flag = true;
    }
    if (IsKeyDown(KEY_S)) {
        move.y += 1.0f;
        flag = true;
    }

    if (flag) {
        move = normalize(move);
    }

    speed->x += move.x * RUN;
    speed->y += move.y * RUN;

    speed->x *= FRICTION;
    speed->y *= FRICTION;

    position->x += speed->x;
    position->y += speed->y;

    *direction = polar_angle(*position,
                             GetMousePosition(),
                             (Vector2){position->x + (SCREEN_X * 2.0f), position->y});
}

static Rectangle triangle_to_rect(const Vector2 points[3]) {
    Vector2 min = points[0];
    Vector2 max = points[0];

    for (u32 i = 1; i < 3; ++i) {
        min.x = points[i].x < min.x ? points[i].x : min.x;
        min.y = points[i].y < min.y ? points[i].y : min.y;
        max.x = max.x < points[i].x ? points[i].x : max.x;
        max.y = max.y < points[i].y ? points[i].y : max.y;
    }

    return (Rectangle){min.x, min.y, max.x - min.x, max.y - min.y};
}

static bool no_overlap(Rectangle a, Rectangle b) {
    return ((a.x + a.width) < b.x) | ((b.x + b.width) < a.x) | ((a.y + a.height) < b.y) |
           ((b.y + b.height) < a.y);
}

static void update_rays(const Vector2 position, const f32 direction, Rays* rays, u32* steps) {
    const f32 length = sqrtf((SCREEN_X * SCREEN_X) + (SCREEN_Y * SCREEN_Y));

    const Vector2 from =
        rotate(position, (Vector2){position.x + (PLAYER_X * 0.75f), position.y}, direction);
    const Vector2 to = rotate(position, (Vector2){position.x + length, position.y}, direction);

    const Vector2 right = rotate(from, to, -FOV);
    const Vector2 left = rotate(from, to, FOV);

    const Rectangle bounds = triangle_to_rect((Vector2[3]){from, left, right});

    rays->len = 0;

    rays_push(rays, from);
    rays_push(rays, right);

    LEN_SUBSET = 0;

    for (u32 i = 0; i < LEN_RECTS; ++i) {
        if (no_overlap(bounds, RECTS[i])) {
            continue;
        }

        subset_push(RECTS[i]);

        const Vector2 points[4] = {
            {RECTS[i].x, RECTS[i].y},
            {RECTS[i].x + RECTS[i].width, RECTS[i].y},
            {RECTS[i].x + RECTS[i].width, RECTS[i].y + RECTS[i].height},
            {RECTS[i].x, RECTS[i].y + RECTS[i].height},
        };

        for (u32 j = 0; j < 4; ++j) {
            if (within_fov(from, to, points[j])) {
                rays_push(rays, points[j]);
                {
                    const Vector2 ray = extend(from, rotate(from, points[j], -EPSILON), length);
                    if (within_fov(from, to, ray)) {
                        rays_push(rays, ray);
                    }
                }
                {
                    const Vector2 ray = extend(from, rotate(from, points[j], EPSILON), length);
                    if (within_fov(from, to, ray)) {
                        rays_push(rays, ray);
                    }
                }
                *steps += 3;
            } else {
                *steps += 1;
            }
        }
    }

    for (u32 i = 1; i < rays->len; ++i) {
        const Vector2 ray = rays->buffer[i];
        const f32     radians = center(polar_angle(from, ray, to));

        u32 j = i;
        for (; (1 < j) && (radians < center(polar_angle(from, rays->buffer[j - 1], to))); --j) {
            rays->buffer[j] = rays->buffer[j - 1];
            *steps += 1;
        }
        rays->buffer[j] = ray;
    }

    rays_push(rays, left);

    for (u32 i = 0; i < LEN_SUBSET; ++i) {
        const Vector2 points[4] = {
            {SUBSET[i].x, SUBSET[i].y},
            {SUBSET[i].x + SUBSET[i].width, SUBSET[i].y},
            {SUBSET[i].x + SUBSET[i].width, SUBSET[i].y + SUBSET[i].height},
            {SUBSET[i].x, SUBSET[i].y + SUBSET[i].height},
        };

        f32 min = center(polar_angle(from, points[0], to));
        f32 max = min;

        for (u32 j = 1; j < 4; ++j) {
            const f32 candidate = center(polar_angle(from, points[j], to));
            min = candidate < min ? candidate : min;
            max = max < candidate ? candidate : max;
        }

        for (u32 j = 1; j < rays->len; ++j) {
            const f32 radians = center(polar_angle(from, rays->buffer[j], to));

            if (radians < min) {
                continue;
            }
            if (max < radians) {
                break;
            }

            intersects_at((Vector2[2]){from, rays->buffer[j]},
                          (Vector2[2]){points[0], points[1]},
                          &rays->buffer[j]);
            intersects_at((Vector2[2]){from, rays->buffer[j]},
                          (Vector2[2]){points[1], points[2]},
                          &rays->buffer[j]);
            intersects_at((Vector2[2]){from, rays->buffer[j]},
                          (Vector2[2]){points[2], points[3]},
                          &rays->buffer[j]);
            intersects_at((Vector2[2]){from, rays->buffer[j]},
                          (Vector2[2]){points[3], points[0]},
                          &rays->buffer[j]);

            *steps += 4;
        }
    }
}

static void draw(const Vector2 position, const f32 direction, const Rays rays, const u32 steps) {
    BeginDrawing();

    ClearBackground((Color){0x40, 0x40, 0xB0, 0xFF});

    DrawRectanglePro((Rectangle){position.x, position.y, PLAYER_X, PLAYER_Y},
                     (Vector2){PLAYER_X / 2.0f, PLAYER_Y / 2.0f},
                     -direction * (180.0f / PI),
                     ORANGE);

    for (u32 i = 0; i < LEN_RECTS; ++i) {
        DrawRectangleRec(RECTS[i], (Color){LIGHTGRAY.r, LIGHTGRAY.g, LIGHTGRAY.b, 0x80});
    }

    for (u32 i = 0; i < LEN_NODES; ++i) {
        const f32 x =
            (f32)(NODES[i].points[0].x + ((NODES[i].points[1].x - NODES[i].points[0].x) / 2) +
                  (NODE_RADIUS / 2));
        const f32 y =
            (f32)(NODES[i].points[0].y + ((NODES[i].points[1].y - NODES[i].points[0].y) / 2) +
                  (NODE_RADIUS / 2));

        DrawCircleV((Vector2){x, y}, NODE_RADIUS, SKYBLUE);

        for (u32 j = 0; j < LEN_DOORS; ++j) {
            if ((((DOORS[j].x == NODES[i].points[0].x) || (DOORS[j].x == NODES[i].points[1].x)) &&
                 (NODES[i].points[0].y <= DOORS[j].y) && (DOORS[j].y <= NODES[i].points[1].y)) ||
                (((DOORS[j].y == NODES[i].points[0].y) || (DOORS[j].y == NODES[i].points[1].y)) &&
                 (NODES[i].points[0].x <= DOORS[j].x) && (DOORS[j].x <= NODES[i].points[1].x)))
            {
                DrawLineV((Vector2){x, y},
                          (Vector2){(f32)DOORS[j].x, (f32)DOORS[j].y},
                          (Color){0xFF, 0xFF, 0xFF, 0x40});
            }
        }
    }

    for (u32 i = 0; i < LEN_DOORS; ++i) {
        DrawCircleV((Vector2){(f32)DOORS[i].x, (f32)DOORS[i].y}, DOOR_RADIUS, PINK);
    }

    DrawTriangleFan(rays.buffer, (i32)rays.len, (Color){0xFF, 0xFF, 0xFF, 0x40});

    DrawFPS(FPS_X, FPS_Y);
    DrawText(TextFormat("%u LEN_RECTS\n%u rays.len\n%u steps\n%.2f direction",
                        LEN_RECTS,
                        rays.len,
                        steps,
                        (f64)direction),
             10,
             40,
             20,
             GREEN);

    EndDrawing();
}

i32 main(void) {
    SetTraceLogLevel(LOG_WARNING);

    InitWindow(SCREEN_X, SCREEN_Y, "raysight");
    SetTargetFPS(60);

    Vector2 position = {(SCREEN_X / 2.0f), (SCREEN_Y / 2.0f)};
    Vector2 speed = {0};

    Rays rays = rays_new();

    generate_vertical(0, SCREEN_X - 1, 0, SCREEN_Y - 1);

    split_verticals();
    split_horizontals();

    while (!WindowShouldClose()) {
        f32 direction = 0.0f;
        update_inputs(&speed, &position, &direction);

        u32 steps = 0;
        update_rays(position, direction, &rays, &steps);
        draw(position, direction, rays, steps);
    }

    CloseWindow();

    free(rays.buffer);

    return 0;
}
