#define GS_IMPL
#include <gs.h>

#define GS_IMMEDIATE_DRAW_IMPL
#include <util/gs_idraw.h>


struct BoxCollider {
    gs_vec2 extents;
};

enum ColliderType {
    COLLIDER_TYPE_AXIS_ALIGNED_BOX,
};

enum ColliderFlags {
    COLLIDER_FLAG_STATIC = 0x1
};

struct Collider {
    gs_vec2 pos; /* center of mass */
    gs_vec2 vel;
    gs_vec2 accel;
    float mass;
    uint32_t flags;
    enum ColliderType collider_type;
    union {
        struct BoxCollider box;
    } s;
};

struct Sprite {
    gs_color_t color;
};

// visible entity with sprite/color
struct Entity {
    uint32_t id;
    struct Collider collider;
    struct Sprite sprite;
};

static uint32_t get_next_entity_id(void) {
    static uint32_t id = 0;
    return id++;
}

struct Character {
    struct Entity base;
    char name[30];
};

struct GameState {
    gs_immediate_draw_t gi;
    gs_command_buffer_t cb;
    gs_camera_t cam;
    uint32_t player_id;
    gs_dyn_array(struct Character) characters;
    gs_dyn_array(struct Entity) entities;
};

struct GameState game_state = {0};

void app_init(void) {
    game_state.gi = gs_immediate_draw_new();
    game_state.cb = gs_command_buffer_new();
    game_state.cam = gs_camera_default();

    uint32_t player_id = get_next_entity_id();

    // Add main player
    gs_dyn_array_push(game_state.characters, ((struct Character){
        .base = (struct Entity) {
            .id = player_id,
            .collider = (struct Collider){
                .pos = gs_v2(0.f, 0.f),
                .flags = 0,
                .collider_type = COLLIDER_TYPE_AXIS_ALIGNED_BOX,
                .s.box = (struct BoxCollider){gs_v2s(.125f)} // half size
            },
            .sprite = (struct Sprite) {
                .color = gs_color(255,0,0,255)
            }
        }
        })
    );

    game_state.player_id = player_id;

    // Add level entities
    gs_dyn_array_push(game_state.entities, ((struct Entity) {
        .id = get_next_entity_id(),
        .collider = (struct Collider){
            .pos = gs_v2(0.f,-.25f),
            .flags = COLLIDER_FLAG_STATIC,
            .collider_type = COLLIDER_TYPE_AXIS_ALIGNED_BOX,
            .s.box = (struct BoxCollider){gs_v2(1.f,.125f)}
        },
        .sprite = (struct Sprite) {
            .color = gs_color(50,50,50,255)
        }
    })
    );
}

#define PLAYER_SPEED 1.f
#define GRAVITY 0.981f

void update_collider(struct Collider* c, float dt)
{
    if (~c->flags & COLLIDER_FLAG_STATIC) {
        c->accel.y = -GRAVITY;

        // Integrate acceleration to get velocity (Euler integration)
        c->vel = gs_vec2_add(c->vel, gs_vec2_mul(c->accel, gs_v2s(dt)));

        // Integrate velocity
        c->pos = gs_vec2_add(c->pos, gs_vec2_mul(c->vel, gs_v2s(dt)));
    
        // Reset acceleration
        c->accel = gs_v2s(0.f);
    }
}

int find_character(struct Character* characters, uint32_t entity_id)
{
    for (uint32_t i = 0; i < gs_dyn_array_size(characters); ++i) {
        if (characters[i].base.id == entity_id) {
            return i;
        }
    }

    return -1;
}

void draw_entity(gs_immediate_draw_t *gi, struct Entity* e)
{
    gsi_rectvx(gi, gs_vec2_sub(e->collider.pos, e->collider.s.box.extents),
                    gs_vec2_add(e->collider.pos, e->collider.s.box.extents), gs_v2s(0.f), gs_v2s(1.f), e->sprite.color, GS_GRAPHICS_PRIMITIVE_TRIANGLES);
}

int colliders_intersect(struct Collider* c1, struct Collider *c2)
{
    if (c1->collider_type == COLLIDER_TYPE_AXIS_ALIGNED_BOX && c2->collider_type == COLLIDER_TYPE_AXIS_ALIGNED_BOX)
    {
        gs_vec2 w1 = c1->s.box.extents;
        gs_vec2 w2 = c2->s.box.extents;
        return (w1.x + w2.x >= fabsf(c2->pos.x - c1->pos.x)) && (w1.y + w2.y >= fabsf(c2->pos.y - c1->pos.y));  
    }

    // collider pair not taken into account
    return 0;
}

void game_update(struct GameState *state, float dt)
{
    gs_dyn_array(struct Collider*) colliders = NULL;

    for (uint32_t i = 0; i < gs_dyn_array_size(state->entities); ++i) {
        gs_dyn_array_push(colliders, &state->entities[i].collider);
    }
    for (uint32_t i = 0; i < gs_dyn_array_size(state->characters); ++i) {
        gs_dyn_array_push(colliders, &state->characters[i].base.collider);
    }

    // Resolve collisions
    struct Collider *c1, *c2;
    for (uint32_t i = 0; i < gs_dyn_array_size(colliders); ++i) {
        for (uint32_t j = 0; j < gs_dyn_array_size(colliders); ++j) {
            c1 = colliders[i], c2 = colliders[j];
            if (c1 != c2) {
                if (colliders_intersect(c1, c2)) {
                    printf("Collided!\n");
                    c1->flags |= COLLIDER_FLAG_STATIC;
                    c2->flags |= COLLIDER_FLAG_STATIC;
                }
            }
        }
    }


    for (uint32_t i = 0; i < gs_dyn_array_size(colliders); ++i) {
        update_collider(colliders[i], dt);
    }
}

void game_draw(struct GameState *state)
{
    for (uint32_t i = 0; i < gs_dyn_array_size(state->entities); ++i) {
        draw_entity(&state->gi, &state->entities[i]);
    }

    for (uint32_t i = 0; i < gs_dyn_array_size(state->characters); ++i) {
        draw_entity(&state->gi, &state->characters[i].base);
    }
}

void app_update(void) {
    float dt = gs_platform_delta_time();
    // printf("DeltaTime: %03.4f\n", dt);

    gs_vec2 ws = gs_platform_framebuffer_sizev(gs_platform_main_window());
    gs_immediate_draw_t *gi = &game_state.gi;
    gs_command_buffer_t *cb = &game_state.cb;

    int player_index = find_character(game_state.characters, game_state.player_id);
    if (player_index != -1) {
        struct Collider *c = &game_state.characters[player_index].base.collider;
        ///////////////// Input handling ////////////////////
        c->accel = gs_v2s(0.f); // reset accel every frame
        if (gs_platform_key_down(GS_KEYCODE_A)) {
            c->accel.x = -PLAYER_SPEED;
        }

        if (gs_platform_key_down(GS_KEYCODE_D)) {
            c->accel.x = PLAYER_SPEED;
        }
    }

    game_update(&game_state, dt);

    ///////////////// Rendering /////////////////////////
    gsi_depth_enabled(gi, false);
    gsi_face_cull_enabled(gi, true);
    gsi_reset(gi);

    {
        gsi_camera(gi, &game_state.cam, ws.x, ws.y);

        // Draw entities
        game_draw(&game_state);
    }

    gsi_renderpass_submit(gi, cb, gs_v4(0, 0, ws.x, ws.y), gs_color(216, 216, 255, 255));
    gs_graphics_command_buffer_submit(cb);
}

void app_shutdown(void) {
    gs_command_buffer_free(&game_state.cb);
    gs_immediate_draw_free(&game_state.gi);
}

gs_app_desc_t gs_main(int32_t argc, char** argv)
{
    return (gs_app_desc_t){
        .init = &app_init,
        .update = &app_update,
        .shutdown = &app_shutdown,
        .window = (gs_platform_window_desc_t){
            .title = "Platformer",
            .vsync = true,
        },
    };
}