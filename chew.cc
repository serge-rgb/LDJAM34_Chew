/**
 * TODO:
 *  - Window resizing.
 *  - Actual background
 *
 */

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

static void die_gracefully(char* msg);

#include <glew.h>
#include <portaudio.h>

#define SGL_GL_HELPERS_IMPLEMENTATION
#include "gl_helpers.h"

#include "portaudio_helpers.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "stb_vorbis.c"

#include "glfw/glfw3.h"




static bool g_should_quit = false;

enum class ImageIndex {
    BACKGROUND,
    SPRITE_JAW,
    SPRITE_HEADTOP,
    SPRITE_INSIDES,
    COUNT,
};

enum class AudioIndex {
    START,

    COUNT,
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


struct v2f {
    float x,y;
};

static ImageInfo    g_image_info[ImageIndex::COUNT];
static AudioInfo    g_audio_items[AudioIndex::COUNT];
static int          g_enabled_tex2d;

static const float k_jaw_up_position   = -0.5;
static const float k_jaw_down_position = -1.0;
static const float kPi                 = 3.141592654f;

// Jaw data
static float g_jaw_vpos = k_jaw_down_position;
static float g_jaw_angle;

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
        }
        if (key == GLFW_KEY_RIGHT) {
            chew_input(ChewDir::RIGHT);
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

static void load_audio(AudioIndex idx, char* fname)
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

    // These assumptions might change later, so data structures still keep the info..
    assert ( num_channels == 2 );
    assert ( sample_rate == 44100 );

    // We are good to go.
    g_audio_items[i].samples = samples;
    g_audio_items[i].rate = sample_rate;
    g_audio_items[i].num_channels = num_channels;
    g_audio_items[i].num_samples = num_samples;


    // TEST. Push audio
    sgl_PA_push_sample(samples, num_samples);
}


static void enable_image(ImageIndex idx)
{
    auto texid = g_image_info[(int) idx].texid;
    GLCHK (glBindTexture (GL_TEXTURE_2D, texid));
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


static void game_tick(float dt)
{

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

}

//
//  a ------- d
//  |         |
//  |        |
//  b--------c
//
static void draw_sprite(ImageIndex idx,
                        v2f a, v2f b, v2f c, v2f d)
{
    enable_image(idx);

    glColor3f(0,1,0);
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

static void draw_circle()
{

}

static void game_render()
{
    // draw_sprite(ImageIndex::BACKGROUND, {-1, -1}, {-1, 1}, {1, 1}, {1, -1});

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

    draw_sprite(ImageIndex::SPRITE_INSIDES,
                {-0.8f * jaw_width, g_jaw_vpos +0.7f +0.7f*jaw_height },
                {-0.8f * jaw_width, g_jaw_vpos +0.7f -0.7f*jaw_height },
                {0.8f * jaw_width,  g_jaw_vpos +0.7f -0.7f*jaw_height },
                {0.8f * jaw_width,  g_jaw_vpos +0.7f +0.7f*jaw_height });

    draw_sprite(ImageIndex::SPRITE_JAW,
                rotated(a, g_jaw_angle), rotated(b, g_jaw_angle), rotated(c, g_jaw_angle), rotated(d, g_jaw_angle));

    draw_sprite(ImageIndex::SPRITE_HEADTOP,
                {-headtop_width, 0.45f + -headtop_height},
                {-headtop_width, 0.45f +  headtop_height},
                {headtop_width,  0.45f +  headtop_height},
                {headtop_width,  0.45f + -headtop_height});
}


int main()
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
        return -1;
    }

    int win_width = 800;
    int win_height = 600;
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(win_width, win_height, "Chew Gum!!!", NULL, NULL);
    if (!window)
    {
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

    sgl_init_PA();

    load_image(ImageIndex::BACKGROUND, "background.png");
    load_image(ImageIndex::SPRITE_JAW, "jaw.png");
    load_image(ImageIndex::SPRITE_HEADTOP, "headtop.png");
    load_image(ImageIndex::SPRITE_INSIDES, "insides.png");
    load_audio(AudioIndex::START, "start.ogg");

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
    GLint quad_program = glCreateProgramObjectARB();
    gl_link_program(quad_program, shader_objects, 2);

    GLCHK (glUseProgramObjectARB(quad_program));

    GLint sampler_loc = glGetUniformLocationARB(quad_program, "raster_buffer");
    assert (sampler_loc >= 0);
    GLCHK (glUniform1iARB(sampler_loc, 0 /*GL_TEXTURE0*/));

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    glClearColor(1,1,1,1);

    double then = glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float dt = now - then;

        game_tick(dt);

        game_render();


        glfwSwapBuffers(window);
        glfwPollEvents();

        if (g_should_quit) {
            glfwDestroyWindow(window);
        }

        then = now;
    }

    sgl_deinit_PA();
    glfwTerminate();
    return 0;
}
