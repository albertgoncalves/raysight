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
    u32      cap;
    u32      len;
    Vector2* buffer;
} Rays;

#define FPS_X 10
#define FPS_Y FPS_X

#define SCREEN_X 1536
#define SCREEN_Y 768

#define PLAYER_X 20
#define PLAYER_Y 30

#define RUN      6.5f
#define FRICTION 0.75f

#define FOV 0.5f

#define EPSILON 0.00001f

static Rectangle RECTS[] = {
    {100.0f, 200.0f, 500.0f, 25.0f},
    {150.0f, 500.0f, 425.0f, 15.0f},
    {1250.0f, 150.0f, 15.0f, 425.0f},
};
#define LEN_RECTS (sizeof(RECTS) / sizeof(RECTS[0]))

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

static void intersect(const Vector2 a[2], const Vector2 b[2], Vector2* point) {
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

    if ((t < 0.0f) || (1.0f < t) || (u < 0.0f) || (1.0f < u)) {
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

static void rays_push_if_fov(Rays* rays, const Vector2 from, const Vector2 to, const Vector2 ray) {
    const f32 radians = center(polar_angle(from, ray, to));
    if (((-FOV) < radians) && (radians < FOV)) {
        rays_push(rays, ray);
    }
}

static void update_position(Vector2* speed, Vector2* position, f32* direction) {
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

static void update_rays(const Vector2 position, const f32 direction, Rays* rays, u32* steps) {
    rays->len = 0;

    const Vector2 from =
        rotate(position, (Vector2){position.x + (PLAYER_X * 0.75f), position.y}, direction);
    const Vector2 to =
        rotate(position, (Vector2){position.x + (SCREEN_X * 2.0f), position.y}, direction);

    rays_push(rays, from);
    rays_push(rays, rotate(from, to, -FOV));

    for (u32 i = 0; i < LEN_RECTS; ++i) {
        const Vector2 points[4] = {
            {RECTS[i].x, RECTS[i].y},
            {RECTS[i].x + RECTS[i].width, RECTS[i].y},
            {RECTS[i].x + RECTS[i].width, RECTS[i].y + RECTS[i].height},
            {RECTS[i].x, RECTS[i].y + RECTS[i].height},
        };

        for (u32 j = 0; j < 4; ++j) {
            rays_push_if_fov(rays,
                             from,
                             to,
                             extend(from, rotate(from, points[j], EPSILON), SCREEN_X * SCREEN_Y));
            rays_push_if_fov(rays,
                             from,
                             to,
                             extend(from, rotate(from, points[j], -EPSILON), SCREEN_X * SCREEN_Y));

            *steps += 2;
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

    rays_push(rays, rotate(from, to, FOV));

    for (u32 i = 0; i < LEN_RECTS; ++i) {
        const Vector2 points[4] = {
            {RECTS[i].x, RECTS[i].y},
            {RECTS[i].x + RECTS[i].width, RECTS[i].y},
            {RECTS[i].x + RECTS[i].width, RECTS[i].y + RECTS[i].height},
            {RECTS[i].x, RECTS[i].y + RECTS[i].height},
        };

        for (u32 j = 1; j < rays->len; ++j) {
            intersect((Vector2[2]){from, rays->buffer[j]},
                      (Vector2[2]){points[0], points[1]},
                      &rays->buffer[j]);
            intersect((Vector2[2]){from, rays->buffer[j]},
                      (Vector2[2]){points[1], points[2]},
                      &rays->buffer[j]);
            intersect((Vector2[2]){from, rays->buffer[j]},
                      (Vector2[2]){points[2], points[3]},
                      &rays->buffer[j]);
            intersect((Vector2[2]){from, rays->buffer[j]},
                      (Vector2[2]){points[3], points[0]},
                      &rays->buffer[j]);

            *steps += 4;
        }
    }
}

static void draw(const Vector2 position, const f32 direction, const Rays rays, const u32 steps) {
    BeginDrawing();

    ClearBackground((Color){0x1A, 0x24, 0x33, 0xFF});

    DrawRectanglePro((Rectangle){position.x, position.y, PLAYER_X, PLAYER_Y},
                     (Vector2){PLAYER_X / 2.0f, PLAYER_Y / 2.0f},
                     -direction * (180.0f / PI),
                     ORANGE);

    for (u32 i = 0; i < LEN_RECTS; ++i) {
        DrawRectangleRec(RECTS[i], LIGHTGRAY);
    }

    DrawTriangleFan(rays.buffer, (i32)rays.len, (Color){0xFF, 0xFF, 0xFF, 0x40});

    DrawFPS(FPS_X, FPS_Y);
    DrawText(TextFormat("%u rays.len\n%u steps\n%.2f direction", rays.len, steps, (f64)direction),
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

    while (!WindowShouldClose()) {
        f32 direction = 0.0f;
        update_position(&speed, &position, &direction);

        u32 steps = 0;
        update_rays(position, direction, &rays, &steps);
        draw(position, direction, rays, steps);
    }

    CloseWindow();

    free(rays.buffer);

    return 0;
}
