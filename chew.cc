#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "GL/glew.h"
#include "glfw/glfw3.h"

#define SGL_GL_HELPERS_IMPLEMENTATION
#include "gl_helpers.h"

static bool g_should_quit = false;


static void die_gracefully(char* msg)
{
    puts(msg);
    exit(EXIT_FAILURE);
}

// Cursor position func
static void cursor_pos_callback(GLFWwindow* win, double x, double y)
{
    printf("%f %f\n", x, y);
}

// Key callback
static void key_callback(GLFWwindow* win, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            g_should_quit = true;
        }
    }
}


int main()
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
        return -1;
    }

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(800, 600, "Chew Gum!!!", NULL, NULL);
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
            "   vec4 color = vec4(0,0,1,1); \n"
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


    glClearColor(1,1,1,1);

    while (!glfwWindowShouldClose(window))
    {

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glColor3f(0,1,0);
        glBegin(GL_QUADS);
        glVertex3f(-1,-1,0);
        glVertex3f(-1,1,0);
        glVertex3f(1,1,0);
        glVertex3f(1,-1,0);
        glEnd();

        glfwSwapBuffers(window);
        glfwPollEvents();

        if (g_should_quit) {
            glfwDestroyWindow(window);
        }
    }

    glfwTerminate();
    return 0;
}