/**
 * TODO:
 *  - Impl pills
 *      - Collision (AABB)
 *      - Semi-random spawning.
 *          - Same-pill auto-collide
 *      - Remove old pills? (circular buffer might make this redundant)
 *  - Integrate stb truetype
 *      - Print multiplier.
 *  - End state. (Print score)
 *  - Window resizing?
 *
 */

#include <atomic>
#include <thread>

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <Windows.h>
#endif

static int g_win_width;
static int g_win_height;

static void die_gracefully(char* msg);

#include <glew.h>

#define SGL_GL_HELPERS_IMPLEMENTATION
#include "gl_helpers.h"

#include "audio.h"

#include "text.h"

#include "vector.hh"


#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "stb/stb_vorbis.c"

#include "glfw/glfw3.h"


static bool g_any = false;  // Any key pressed.
static bool g_should_quit = false;

enum class ImageIndex {
    BACKGROUND,
    JAW,
    HEADTOP,
    INSIDES,
    CIRCLE,
    GUM_ORANGE,
    GUM_BLUE,
    DEAD_SCREEN,
    COUNT,
};

enum class AudioIndex {
    DUKE,
    LOOP,

    COUNT,
};

enum class AudioOpts {
    NOTHING,
    LOOP_FOREVER,
};

struct ImageInfo {
    int w,h,num_components;
    uint8_t* bits;
    GLint texid;
};

struct AudioInfo {
    int num_channels;
    int rate;
    int num_samples;
    short* samples;
};


enum InputFlags {
    NONE,
    GOT_LEFT  = 1 << 0,
    GOT_RIGHT = 1 << 1,
};

enum class ButtonState {
    NORMAL,
    GOING_UP,
    COMING_DOWN,
};

static const int k_eatable_queue_items = 16;
static const float k_eatable_width     = 0.20;

enum class EatableColor {
    ORANGE,
    BLUE,
    COUNT,
};

struct Eatable {
    float value;
    float height;
    EatableColor color;
};

struct EatableQueue {
    Eatable items[k_eatable_queue_items];
    int head;
    int tail;
};


static GLuint quad_program;

static int g_input_flags;

static const float k_btn_y                = -0.70f;
static const float k_btn_x_from_center    = 0.65f;
static const float k_normal_btn_radius    = 0.20f;
static const float k_max_btn_radius       = 0.40f;
static const float k_eatable_speed        = 0.010f;
static const float k_eatable_begin_y      = 0.90f;

static const float k_default_shrink_speed = 0.001f;

struct GameState {
    static const float beat_length;
    static const float rhythm_period;

    int score = 0;
    int spree_count = 0;

    float head_scale = 1.0f;
    float shrink_speed = k_default_shrink_speed;

    float dt_accum_spawn;
    float dt_accum_rhythm;

    float accum_speedup;

    float eatable_speed = k_eatable_speed;

    float spawn_threshold = 1.5f;

    float btn_radius[2] = { k_normal_btn_radius, k_normal_btn_radius };

    ButtonState btn_states[2];

    union {
        struct {
            EatableQueue left_queue;
            EatableQueue right_queue;
        };
        EatableQueue eatable_queues[2];
    };

    EatableColor last_color = EatableColor::COUNT;
    bool dead;
};

const float GameState::beat_length   = 0.005;
const float GameState::rhythm_period = 0.28571428;

static ImageInfo    g_image_info[ImageIndex::COUNT];
static AudioInfo    g_audio_items[AudioIndex::COUNT];
static int          g_enabled_tex2d;

static const float k_jaw_up_position   = -0.5;
static const float k_jaw_down_position = -1.0;
static const float kPi                 = 3.141592654f;

// Jaw data
static float g_jaw_vpos = k_jaw_down_position;
static float g_jaw_angle;


// Immediate mode scaling
static v2f      g_scale_center;
static float    g_scale_factor;

static void begin_scale(v2f center, float factor)
{
    g_scale_center = center;
    g_scale_factor = factor;
}

static void end_scale()
{
    g_scale_center = {};
    g_scale_factor = 1;
}

static void die_gracefully(char* msg)
{
    puts(msg);
    exit(EXIT_FAILURE);
}

// Cursor position func
static void cursor_pos_callback(GLFWwindow* win, double x, double y)
{
//    printf("%f %f\n", x, y);
}

enum class ChewDir {
    LEFT,
    RIGHT,
};

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
#error implement
#endif

}

static void chew_input(ChewDir dir);

// Key callback
static void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {

        if (key == GLFW_KEY_ESCAPE) {
            g_should_quit = true;
        }

        if (key == GLFW_KEY_LEFT) {
            chew_input(ChewDir::LEFT);
            g_input_flags |= GOT_LEFT;
        } else if (key == GLFW_KEY_RIGHT) {
            chew_input(ChewDir::RIGHT);
            g_input_flags |= GOT_RIGHT;
        } else {
            g_any = true;
        }
    }
}

static void load_image(ImageIndex idx, char* fname)
{
    int i = (int)idx;

    int w,h,num_components;
    uint8_t* data = stbi_load(fname, &w, &h, &num_components, 0);

    if (!data) {
        die_gracefully("Could not read file.");
    }
    if ( g_image_info[(int)idx].bits != NULL ) {
        die_gracefully("already loaded");
    }

    g_image_info[i].w = w;
    g_image_info[i].h = h;
    g_image_info[i].num_components = num_components;
    g_image_info[i].bits = data;

    // Create texture
    {
        GLCHK (glActiveTexture (GL_TEXTURE0) );

        GLuint texture = 0;
        GLCHK (glGenTextures   (1, &texture));

        assert (texture > 0);
        g_image_info[i].texid = texture;

        GLCHK (glBindTexture   (GL_TEXTURE_2D, texture));

        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCHK (glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        GLCHK (glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                            g_image_info[i].w, g_image_info[i].h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                            (GLvoid*)g_image_info[i].bits));
    }
}

static void load_audio(AudioIndex idx, char* fname, int sample_padding = 0)
{
    int i = (int)idx;
    if (g_audio_items[i].samples != NULL) {
        die_gracefully("Trying to load audio twice.");
    }

    int num_channels, sample_rate;
    short* samples;

    int num_samples = stb_vorbis_decode_filename(fname, &num_channels, &sample_rate, &samples);
    if (num_samples == -1)  {
        printf("trying to open file %s\n", fname);
        die_gracefully("stb vorbis could not open or decode");
    }

    if ( sample_padding ) {
        size_t new_size = (num_samples*num_channels + sample_padding*num_channels) * sizeof(short);
        short* new_samples = (short*)calloc(1, new_size);
        if (!new_samples) {
            die_gracefully("could not allocate memory for sample padding ");
        }
        memcpy(new_samples, samples, num_samples*num_channels * sizeof(short));
        free(samples);
        samples = new_samples;
        num_samples = num_samples + sample_padding;
    }

    // These assumptions might change later, so data structures still keep the info..
    assert ( num_channels == 2 );
    assert ( sample_rate == 44100 );

    // We are good to go.
    g_audio_items[i].samples = samples;
    g_audio_items[i].rate = sample_rate;
    g_audio_items[i].num_channels = num_channels;
    g_audio_items[i].num_samples = num_samples;
}

static void push_audio(int queue_i, AudioIndex idx, AudioOpts opts = AudioOpts::NOTHING)
{
    int i = (int)idx;
    AudioInfo* ai = &g_audio_items[i];

    if (!ai->samples) {
        die_gracefully("audio not loaded.");
    }

    switch ( opts ) {
    case AudioOpts::NOTHING:
        audio_push_sample(queue_i, ai->samples, ai->num_samples);
        break;
    case AudioOpts::LOOP_FOREVER:
        audio_push_sample(queue_i, ai->samples, ai->num_samples, -1);
        break;
    default:
        assert (!"not implemented\n");
    }

}


static void enable_image(ImageIndex idx)
{
    auto texid = g_image_info[(int) idx].texid;
    GLCHK (glBindTexture (GL_TEXTURE_2D, texid));
}

//
//  a ------- d
//  |         |
//  |        |
//  b--------c

static void draw_sprite(ImageIndex idx,
                        v2f a, v2f b, v2f c, v2f d)
{
    enable_image(idx);

    a -= g_scale_center;
    a = a * g_scale_factor;
    b -= g_scale_center;
    b = b * g_scale_factor;
    c -= g_scale_center;
    c = c * g_scale_factor;
    d -= g_scale_center;
    d = d * g_scale_factor;

    glBegin(GL_QUADS);
    glTexCoord2f(0, 1);
    glVertex3f(a.x,a.y,0);
    glTexCoord2f(0, 0);
    glVertex3f(b.x,b.y,0);
    glTexCoord2f(1, 0);
    glVertex3f(c.x,c.y,0);
    glTexCoord2f(1, 1);
    glVertex3f(d.x,d.y,0);
    glEnd();
}

static void draw_square_sprite(ImageIndex idx, float x, float y, float w)
{
    float ar = (float)g_win_width / g_win_height;
    draw_sprite(idx,
                {x - w, y - ar*w},
                {x - w, y + ar*w},
                {x + w, y + ar*w},
                {x + w, y - ar*w});
}

static bool collide_squares(float x1, float y1, float w1,
                            float x2, float y2, float w2)
{
    bool not =
            (x2 > x1 + w1) ||
            (y2 > y1 + w1) ||
            (y2 + w2 < y1) ||
            (x2 + w2 < x1);
    return !not;
}

static void chew_input(ChewDir dir)
{
    g_jaw_vpos = k_jaw_up_position;

    if ( dir == ChewDir::LEFT ) {
        g_jaw_angle += kPi / 4;
    }
    if ( dir == ChewDir::RIGHT ) {
        g_jaw_angle -= kPi / 4;
    }
}


static void add_eatable(EatableQueue* queue, Eatable e)
{
    queue->items[queue->tail] = e;
    queue->tail = (queue->tail + 1) % k_eatable_queue_items;
}

static Eatable* peek_eatable(EatableQueue* queue)
{
    if (queue->tail == queue->head) {
        return NULL;
    }
    return &queue->items[queue->head];
}


static Eatable* pop_eatable(EatableQueue* queue)
{
    Eatable* res = peek_eatable(queue);

    queue->head = (queue->head + 1) % k_eatable_queue_items;

    return res;
}

static Eatable* pop_eatable_until(EatableQueue* queue, int to_pop)
{
    Eatable* res;
    do {
        res = peek_eatable(queue);
        queue->head = (queue->head + 1) % k_eatable_queue_items;
    } while (queue->head != to_pop);

    return res;
}

static void game_tick(float dt, GameState* gs)
{

    gs->accum_speedup += dt;

    if ( gs->accum_speedup > 30 ) {
        printf( "Moar speeeed.\n" );
        gs->eatable_speed += k_eatable_speed;
        gs->spawn_threshold *= 0.75f;;
        gs->accum_speedup = 0;
    }

    if (gs->dead) {
        if (g_any) {
            gs->dead = false;
            gs->score = 0;
            gs->spree_count = 0;
        }
        return;
    }
    g_any = false;
    gs->dt_accum_spawn += dt;


    if (g_input_flags & GOT_LEFT) {
        gs->btn_states[0] = ButtonState::GOING_UP;
        g_input_flags ^= GOT_LEFT;
    }
    if (g_input_flags & GOT_RIGHT) {
        gs->btn_states[1] = ButtonState::GOING_UP;
        g_input_flags ^= GOT_RIGHT;
    }

    float btn_shape_change = 0.1f;

    for (int i = 0; i < 2; ++i) {
        if ( gs->btn_states[i] == ButtonState::GOING_UP ) {
            gs->btn_radius[i] += btn_shape_change;
            if ( gs->btn_radius[i] > k_max_btn_radius ) {
                gs->btn_states[i] = ButtonState::COMING_DOWN;
            }
        } else if  (gs -> btn_states[i] == ButtonState::COMING_DOWN ) {
            gs->btn_radius[i] -= btn_shape_change;
            if ( gs->btn_radius[i] <= k_normal_btn_radius ) {
                gs->btn_radius[i] = k_normal_btn_radius;
                gs->btn_states[i] = ButtonState::NORMAL;
            }
        }
    }

    float height_constant = 0.05f;
    float angle_constant  = 0.1f;

    if (g_jaw_vpos > k_jaw_down_position) {
        g_jaw_vpos -= height_constant;
        if (g_jaw_vpos < k_jaw_down_position) {
            g_jaw_vpos = k_jaw_down_position;
        }
    }


    if ( g_jaw_angle < 0 ) {
        g_jaw_angle += angle_constant;
        if (g_jaw_angle > 0) {
            g_jaw_angle = 0;
        }
    }
    if ( g_jaw_angle > 0 ) {
        g_jaw_angle -= angle_constant;
        if (g_jaw_angle < 0) {
            g_jaw_angle = 0;
        }
    }

    // Spawn?
    if ( gs->dt_accum_spawn >  gs->spawn_threshold) {
        gs->dt_accum_spawn = 0;
        int side = rand() % 2;

        EatableColor color = (EatableColor)(rand() % (int)EatableColor::COUNT);

        float value = 0.0f;
        switch ( color ) {
        case EatableColor::ORANGE:
            value = 1.0f;
            break;
        case EatableColor::BLUE:
            value = 1.0f;
            break;
        default:
            assert (!"nope");
            break;
        }
        Eatable e = { value, k_eatable_begin_y, color};
        add_eatable(&gs->eatable_queues[side], e);
    }


    // Iterate through eatables.

    bool collided = false;
    for (int ei = 0; ei < 2; ++ei) {
        EatableQueue* eq = &gs->eatable_queues[ei];
        for (int i = eq->head; !collided && i != eq->tail; i = (i+1)%k_eatable_queue_items) {
            Eatable* eatable = &eq->items[i];

            // Make eatables fall
            eatable->height -= gs->eatable_speed;

            // Collide against respective circles.
            if ( gs->btn_states[ei] != ButtonState::NORMAL ) {
                float btn_x = ei == 0? -k_btn_x_from_center : k_btn_x_from_center;
                float btn_y = k_btn_y;
#if 0
                float btn_w = k_normal_btn_radius;
#else
                float btn_w = gs->btn_radius[ei];
#endif

                float eat_x = btn_x;
                float eat_y = eatable->height;
                float eat_w = k_eatable_width;

                if ( collide_squares(btn_x, btn_y, btn_w,
                                     eat_x, eat_y, eat_w) ) {

                    if ( gs->last_color != eatable->color ) {
                        gs->spree_count = 0;
                        gs->score += 10;
                    }
                    else {
                        gs->spree_count++;
                        gs->score += 10 * 1+gs->spree_count;
                    }

                    float factor = 1;
                    switch ( eatable->color ) {
                    case EatableColor::ORANGE:
                        if ( gs->shrink_speed > 0 )
                            gs->shrink_speed += k_default_shrink_speed;
                        else
                            gs->shrink_speed = k_default_shrink_speed;
                        printf("Collide: Orange ( %f )\n", gs->shrink_speed);
                        //gs->head_scale += 0.05f;
                        break;
                    case EatableColor::BLUE:
                        if ( gs->shrink_speed < 0 )
                            gs->shrink_speed -= k_default_shrink_speed;
                        else
                            gs->shrink_speed = -k_default_shrink_speed;
                        printf("Collide: Blue ( %f )\n", gs->shrink_speed);
                        //gs->head_scale -= 0.05f;
                        break;

                    }

                    gs->head_scale *= eatable->value;
                    pop_eatable_until(eq, (i+1)%k_eatable_queue_items);
                    gs->last_color = eatable->color;
                    collided = true;
                    break;
                }
            }
        }
    }

    // Seems to be more challenging when growth is non-linear.
    gs->head_scale -= gs->shrink_speed; // * gs->head_scale;
    if ( gs->head_scale <= 0.001f || gs->head_scale > 2.0f ) {
        gs->head_scale = 1;
        gs->shrink_speed = k_default_shrink_speed;
        gs->eatable_speed = k_eatable_speed;
        gs->spawn_threshold = 1.5f;
        gs->accum_speedup = 0;
        gs->dt_accum_spawn = 0;
        gs->dt_accum_rhythm = 0;
        for ( int ei = 0; ei < 2; ++ei ) // Reset queues
            gs->eatable_queues[ei].head = gs->eatable_queues[ei].tail = 0;
        gs->dead = true;
    }

}

static void render_score(GameState* gs, bool with_multiplier = true)
{
    // Text test
    //glDisable(GL_BLEND);

    glUseProgramObjectARB(0);
    char buffer[1024];
    if (gs->spree_count == 0 || !with_multiplier)
        sprintf(buffer, "Score: %d", gs->score);
    else
        sprintf(buffer, "Score: %d ( %dX! )", gs->score, 1+gs->spree_count);

    int pad = with_multiplier? 0 : 100;
    my_stbtt_print(pad + 0.3f* g_win_width ,0.05f * g_win_height, buffer);
    glUseProgramObjectARB(quad_program);
    //glEnable(GL_BLEND);
}

static void game_render(float dt, GameState* gs)
{
    if (gs->dead) {
        draw_sprite(ImageIndex::DEAD_SCREEN,
                    {-1, -1},
                    {-1, 1},
                    {1, 1},
                    {1, -1});
        render_score(gs, false);
        return;
    }

    auto to_positive = [](float f) -> float {
        float res = (f + 1)/2;
        return res;
    };

    float left_height  = g_jaw_vpos;
    float right_height = g_jaw_vpos;

    float jaw_width = 0.4;
    float jaw_height = 0.7;

    float headtop_width = jaw_width * 1.1;
    float headtop_height = jaw_height * 0.8;


    v2f a = {-jaw_width, left_height};
    v2f b = {-jaw_width, left_height + jaw_height};
    v2f c = {jaw_width, right_height + jaw_height};
    v2f d = {jaw_width, right_height};


    float pendulum_height = 0.3f;

    ////////////////////////////////////////////////////////////
    // Render face

    v2f center = { 0, g_jaw_vpos + (jaw_height / 2) + pendulum_height };

    auto rotated = [&](v2f p, float a) -> v2f {
        v2f res;

        float c = cosf(a);
        float s = sinf(a);

        p.x -= center.x;
        p.y -= center.y;

        res.x = c*(p.x) + s*(p.y);
        res.y = c*(p.y) - s*(p.x);

        res.x += center.x;
        res.y += center.y;

        return res;
    };

    begin_scale(center, gs->head_scale);

    draw_sprite(ImageIndex::INSIDES,
                {-0.8f * jaw_width, g_jaw_vpos +0.7f +0.7f*jaw_height },
                {-0.8f * jaw_width, g_jaw_vpos +0.7f -0.7f*jaw_height },
                {0.8f * jaw_width,  g_jaw_vpos +0.7f -0.7f*jaw_height },
                {0.8f * jaw_width,  g_jaw_vpos +0.7f +0.7f*jaw_height });

    draw_sprite(ImageIndex::JAW,
                rotated(a, g_jaw_angle), rotated(b, g_jaw_angle), rotated(c, g_jaw_angle), rotated(d, g_jaw_angle));

    draw_sprite(ImageIndex::HEADTOP,
                {-headtop_width, 0.45f + -headtop_height},
                {-headtop_width, 0.45f +  headtop_height},
                {headtop_width,  0.45f +  headtop_height},
                {headtop_width,  0.45f + -headtop_height});

    end_scale();

    // End of Render Face
    ////////////////////////////////////////////////////////////



    // Render collision circles with flash.

    gs->dt_accum_rhythm += dt;
    if (gs->dt_accum_rhythm < GameState::rhythm_period - GameState::beat_length) {
        draw_square_sprite(ImageIndex::CIRCLE,
                           -k_btn_x_from_center, k_btn_y, gs->btn_radius[0]);
        draw_square_sprite(ImageIndex::CIRCLE,
                           k_btn_x_from_center, k_btn_y, gs->btn_radius[1]);
    } else if (gs->dt_accum_rhythm > GameState::rhythm_period) {
        gs->dt_accum_rhythm = 0;
    }

    // Render eatable queues
    for (int ei = 0; ei < 2; ++ei) {
        EatableQueue* eq = &gs->eatable_queues[ei];

        for (int i = eq->head; i != eq->tail; i = (i+1)%k_eatable_queue_items) {
            Eatable* eatable = &eq->items[i];
            ImageIndex img_idx;
            switch (eatable->color) {
            case EatableColor::ORANGE:
                img_idx = ImageIndex::GUM_ORANGE;
                break;
            case EatableColor::BLUE:
                img_idx = ImageIndex::GUM_BLUE;
                break;
            default:
                assert (!"not implemented");
                break;
            }
            draw_square_sprite(img_idx,
                               (ei == 0) ? -k_btn_x_from_center : k_btn_x_from_center,  // left or right.
                               eatable->height,
                               k_eatable_width);
        }
    }

    render_score(gs);
}


#ifdef RELEASE_CHEW
int CALLBACK WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow
        )
#else
int main()
#endif
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
        return -1;
    }

    g_win_width = 800;
    g_win_height = 600;
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(g_win_width, g_win_height, "Chew Gum!!!", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetKeyCallback(window, key_callback);


    // Load extensions
    GLenum glew_err = glewInit();

    if (glew_err != GLEW_OK) {
        printf("glewInit failed with error: %s\nExiting.\n", glewGetErrorString(glew_err));
        exit(EXIT_FAILURE);
    }

    if (GLEW_VERSION_1_4) {
        if ( glewIsSupported("GL_ARB_shader_objects "
                             "GL_ARB_vertex_program "
                             "GL_ARB_fragment_program "
                             "GL_ARB_vertex_buffer_object ") ) {
            printf("[DEBUG] GL OK.\n");
        } else {
            die_gracefully("One or more OpenGL extensions are not supported.\n");
        }
    } else {
        die_gracefully("OpenGL 1.4 not supported.\n");
    }

    audio_init();

    load_image(ImageIndex::BACKGROUND, "background.png");
    load_image(ImageIndex::JAW, "jaw.png");
    load_image(ImageIndex::HEADTOP, "headtop.png");
    load_image(ImageIndex::INSIDES, "insides.png");
    load_image(ImageIndex::CIRCLE, "circle.png");
    load_image(ImageIndex::GUM_ORANGE, "gum_orange.png");
    load_image(ImageIndex::GUM_BLUE, "gum_blue.png");
    load_image(ImageIndex::DEAD_SCREEN, "dead.png");

    load_audio(AudioIndex::DUKE, "duke.ogg");
    load_audio(AudioIndex::LOOP, "loop.ogg", /*padding*/44100/16);

    my_stbtt_initfont();

    const char* shader_contents[2];
    shader_contents[0] =
            "#version 120\n"
            "attribute vec2 position;\n"
            "\n"
            "varying vec2 coord;\n"
            "\n"
            "void main()\n"
            "{\n"
            "   coord = (position + vec2(1.0,1.0))/2.0;\n"
            "   coord.y = 1.0 - coord.y;"
            "   // direct to clip space. must be in [-1, 1]^2\n"
            "   gl_Position = vec4(position, 0.0, 1.0);\n"
            "   gl_TexCoord[0] = gl_MultiTexCoord0;"
            "}\n";

    shader_contents[1] =
            "#version 120\n"
            "\n"
            "uniform sampler2D raster_buffer;\n"
            "uniform float aspect_ratio;\n"
            "varying vec2 coord;\n"
            "\n"
            "void main(void)\n"
            "{\n"
            //"   vec4 color = vec4(0,0,1,1); \n"
            //"   vec4 color = texture2D(raster_buffer, coord); \n"
            "   vec4 color = texture2D(raster_buffer, gl_TexCoord[0].st); \n"
            "   gl_FragColor = color; \n"
            //"   out_color = color; \n"
            "}\n";

    GLuint shader_objects[2] = {0};
    for ( int i = 0; i < 2; ++i ) {
        GLuint shader_type = (GLuint)((i == 0) ? GL_VERTEX_SHADER_ARB : GL_FRAGMENT_SHADER_ARB);
        shader_objects[i] = gl_compile_shader(shader_contents[i], shader_type);
    }
    quad_program = glCreateProgramObjectARB();
    gl_link_program(quad_program, shader_objects, 2);

    GLCHK (glUseProgramObjectARB(quad_program));

    GLint sampler_loc = glGetUniformLocationARB(quad_program, "raster_buffer");
    assert (sampler_loc >= 0);
    GLCHK (glUniform1iARB(sampler_loc, 0 /*GL_TEXTURE0*/));

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    glClearColor(1,1,1,1);

    // Push d7samurai's Duke Nukem quote remix of awesomeness.
    push_audio(1, AudioIndex::DUKE);
    push_audio(1, AudioIndex::LOOP, AudioOpts::LOOP_FOREVER);
    // Launch a thread that sleeps a while before adding the second duke nukem quote
    /* std::thread duke_thread([]() { */
    /*                             sleep_ms(6000); */
    /*                             push_audio(0, AudioIndex::DUKE); */
    /*                         }); */
    /* duke_thread.detach(); */

    double then = glfwGetTime();

    GameState gs = {};

    srand(time(NULL));

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float dt = now - then;

        game_tick(dt, &gs);

        game_render(dt, &gs);


        glfwSwapBuffers(window);
        glfwPollEvents();

        if (g_should_quit) {
            glfwDestroyWindow(window);
        }

        then = now;
    }

    audio_deinit();
    glfwTerminate();
    return 0;
}

#include "audio.cc"
#include "text.cc"
