//===================================================================================================
//= Multi GPU Rendering project                                                                     =
//===================================================================================================
//= In this project, we are going to use GLX to use two different gpus to render a simple triangle. =
//===================================================================================================

// STD
#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<string>
#include<vector>
#include<time.h>

// Boost
#include <boost/thread.hpp>

// GL
#include <GL/glew.h>
#ifdef  __linux
#include <GL/glxew.h>
# include<GL/glx.h>
# include<GL/glxext.h>

#endif

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#ifdef  __linux
// X11
# include<X11/X.h>
# include<X11/Xlib.h>
#elif _WIN32

#endif

#include "helper.h"

using namespace std;

//#define MANUAL_COPY_TEXTURE

/**********************************************************************************
 **********************************************************************************
 * Function Prototypes
 **********************************************************************************
 **********************************************************************************/
struct ConnectedDisplay {
  std::string           dpy_name;
  std::string           gpu_gl_ver;
  std::string           gpu_vendor;
  std::string           gpu_renderer;

  Display*              dpy;
  Window                root;
  XVisualInfo*          vi;
  Colormap              cmap;
  XSetWindowAttributes  swa;
  Window                win;
  GLXContext            glctx;
  XWindowAttributes     attribs;
  XEvent                xev;
};

GLint att[] = {
  GLX_RGBA,
  GLX_DEPTH_SIZE, 16,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DOUBLEBUFFER,
  None
};

int windowWidth = 400, windowHeight = 400;

ConnectedDisplay            main_display;
ConnectedDisplay            display[2];

GLuint                      color_texture[2], depth_texture[2];
std::vector<glm::vec4>      vertices;
std::vector<glm::vec4>      quad_verts;
GLuint                      combine_shader;
GLuint                      program_shader[2];
GLuint                      gbuffer[2];
GLuint                      gbuffer_tex[2][2];
GLuint                      triangle_buf[2];
GLuint                      quad_buf;
glm::vec4                   colors[2];

boost::mutex                framebuffer_mutex[2];

glm::vec3 eyePos(0.0f, 0.0f, 3.0f);
glm::vec3 lookAtPos(0.0f, 0.0f, 0.0f);
glm::vec3 upVec(0.0f, 1.0f, 0.0f);
glm::mat4 world_mat[2], view_mat, proj_mat;

//
// Display Function Prototypes
//
void create_display(const char* display_string, ConnectedDisplay* display);
void event_handler(int gbuffer_idx, ConnectedDisplay* display);

//
// Rendering Prototypes
//
void createGBuffer(int gbuffer_idx, ConnectedDisplay* display);
void render(int gbuffer_idx, ConnectedDisplay* display);
void render_main(ConnectedDisplay* display);
void generate_rand_texture(GLuint& textureId);
void generate_rand_depth_texture(GLuint& textureId);
void read_copy_texture(ConnectedDisplay* display_src, GLuint tex_src, ConnectedDisplay* display_dest, GLuint tex_dest);
void read_copy_depth_texture(ConnectedDisplay* display_src, GLuint tex_src, ConnectedDisplay* display_dest, GLuint tex_dest);

/**********************************************************************************
 **********************************************************************************
 * Create Display using GLX
 **********************************************************************************
 **********************************************************************************/
void create_display(const char* display_string, ConnectedDisplay* display){

  // Creating the display name
  display->dpy_name = display_string;

  // Opening the display
  display->dpy = XOpenDisplay(display->dpy_name.c_str());
  int display_idx = DefaultScreen(display->dpy);

  // Check if the display exists.
  if(display->dpy == NULL) {
  }

  display->root = RootWindow(display->dpy, display_idx);
  display->vi = glXChooseVisual(display->dpy, display_idx, att );
  if(display->vi == NULL){
    printf("\n\tno appropriate visual found\n\n");
    exit(0);
  } else {
    std::cout << "=====================================\n";
    printf("\tvisual %p selected\n", (void *)display->vi->visualid); /* %p creates hexadecimal output like in glxinfo */
  }

  display->cmap = XCreateColormap(display->dpy, display->root, display->vi->visual, AllocNone);
  display->swa.colormap = display->cmap;
  display->swa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;


  display->win = XCreateWindow(display->dpy, display->root, 0, 0, windowWidth, windowHeight, 0, display->vi->depth, InputOutput, display->vi->visual, CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &display->swa);
  if(display->win){
    std::cout << "Display Window Created Successfully!\n";
    XSizeHints sizehints;
    sizehints.x = 0;
    sizehints.y = 0;
    sizehints.width  = windowWidth;
    sizehints.height = windowHeight;
    sizehints.flags = USSize | USPosition;
    XSetNormalHints(display->dpy, display->win, &sizehints);
    XSetStandardProperties(display->dpy, display->win, display->dpy_name.c_str() , display->dpy_name.c_str(), None, (char **)NULL, 0, &sizehints);
  }

  display->glctx = glXCreateContext(display->dpy, display->vi, NULL, GL_TRUE);

  XMapWindow(display->dpy, display->win);
  XStoreName(display->dpy, display->win, display->dpy_name.c_str());

  if(!glXMakeCurrent(display->dpy, display->win, display->glctx))
    std::cout << "Cannot Make Current Main\n";

  // Disable V-Sync
  //glXSwapIntervalEXT(display->dpy, glXGetCurrentDrawable(), 0);

  // Initialize the Glew
  if(glewInit() != GLEW_OK){
      std::cout << "Cannot initialize the glew!\n";
      return ;
  }

  glEnable(GL_DEPTH_TEST);

  view_mat         = glm::lookAt(eyePos, lookAtPos, upVec);
  proj_mat         = glm::perspective(glm::pi<float>() / 4.0f, static_cast<float>(windowWidth) / windowHeight, 0.0f, 1000.0f);
  program_shader[display_idx]   = compile_link_vs_fs("/home/smostaja/DeferredShading/simple.vert", "/home/smostaja/DeferredShading/simple.frag");

  char version[201], vendor[201], renderer[201];
  strcpy(version, (char *) glGetString(GL_VERSION));
  strcpy(vendor, (char *) glGetString(GL_VENDOR));
  strcpy(renderer, (char *) glGetString(GL_RENDERER));
  display->gpu_gl_ver = version;
  display->gpu_vendor = vendor;
  display->gpu_renderer = renderer;
  std::cout << "=====================================\n";
  std::cout << "Version: " << version << std::endl;
  std::cout << "Vendor: " << vendor << std::endl;
  std::cout << "Renderer: " << renderer << std::endl;
  std::cout << "=====================================\n";
}

void main_event_handler(ConnectedDisplay* mymain_display, ConnectedDisplay* display0, ConnectedDisplay* display1){
  glXMakeCurrent(mymain_display->dpy, mymain_display->win, mymain_display->glctx);
  while (1) {
//    while (XPending(mymain_display->dpy)) {
//      XEvent event;
//      XNextEvent(mymain_display->dpy, &event);
//      if (event.xany.window == mymain_display->win) {
//        switch (event.type) {
//        case Expose:
//          //render_main(now, mymain_display);
//          break;
//        case ConfigureNotify:
//          //Resize(h, event.xconfigure.width, event.xconfigure.height);
//          break;
//        case KeyPress:
//          return;
//        default:
//          /*no-op*/ ;
//        }
//      }
//      else {
//        printf("window mismatch\n");
//      }
//    }

    clock_t start_time = clock();

    while(!framebuffer_mutex[0].try_lock());
        read_copy_texture(display0, gbuffer_tex[0][0], mymain_display, color_texture[0]);
        read_copy_texture(display0, gbuffer_tex[0][1], mymain_display, depth_texture[0]);
#ifdef MANUAL_COPY_TEXTURE
        read_copy_depth_texture(display0, gbuffer_tex[0][0], mymain_display, color_texture[0]);
#endif
    framebuffer_mutex[0].unlock();

    while(!framebuffer_mutex[1].try_lock());
        read_copy_texture(display1, gbuffer_tex[1][0], mymain_display, color_texture[1]);
        read_copy_texture(display1, gbuffer_tex[1][1], mymain_display, depth_texture[1]);
#ifdef MANUAL_COPY_TEXTURE
        read_copy_depth_texture(display1, gbuffer_tex[1][0], mymain_display, color_texture[1]);
#endif
    framebuffer_mutex[1].unlock();

    render_main(mymain_display);

    clock_t end_time = clock();
    float duration = (float)(end_time - start_time) / CLOCKS_PER_SEC;
    std::cout << "Overall Duration = " << 1.0f/ duration << std::endl;
  }
}

/**********************************************************************************
 **********************************************************************************
 * Rendering Stuff
 **********************************************************************************
 **********************************************************************************/

void genBuffers(int idx, ConnectedDisplay* display) {
    glXMakeCurrent(display->dpy, display->win, display->glctx);
    glGenBuffers(1, &triangle_buf[idx]);
    glBindBuffer(GL_ARRAY_BUFFER, triangle_buf[idx]);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec4), vertices.data(), GL_STATIC_DRAW);
}

void read_copy_texture(ConnectedDisplay* display_src, GLuint tex_src, ConnectedDisplay* display_dest, GLuint tex_dest){

    // Copy With GLX Features

#ifndef MANUAL_COPY_TEXTURE
    glXCopyImageSubDataNV(display_src->dpy, display_src->glctx, tex_src, GL_TEXTURE_2D, 0, 0, 0, 0, display_dest->glctx, tex_dest, GL_TEXTURE_2D, 0, 0, 0, 0, windowWidth, windowHeight, 1);

#else

    // Previous state of copying texture -> cpu -> other context but replaced with Copy with GLX
    std::vector<float> tex_data(windowWidth * windowHeight * 4);
    glXMakeCurrent(display_src->dpy, display_src->win, display_src->glctx);
    glBindTexture(GL_TEXTURE_2D, tex_src);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, tex_data.data());

    glBindTexture(GL_TEXTURE_2D, tex_dest);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, windowWidth, windowHeight, GL_RGBA, GL_FLOAT, tex_data.data());
#endif
}

#ifdef MANUAL_COPY_TEXTURE
// Previous state of copying texture -> cpu -> other context but replaced with Copy with GLX
void read_copy_depth_texture(ConnectedDisplay* display_src, GLuint tex_src, ConnectedDisplay* display_dest, GLuint tex_dest){
    std::vector<float> tex_data(windowWidth * windowHeight);
    glXMakeCurrent(display_src->dpy, display_src->win, display_src->glctx);
    glBindTexture(GL_TEXTURE_2D, tex_src);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, tex_data.data());

    glXMakeCurrent(display_dest->dpy, display_dest->win, display_dest->glctx);
    glBindTexture(GL_TEXTURE_2D, tex_dest);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, windowWidth, windowHeight, GL_DEPTH_COMPONENT, GL_FLOAT, tex_data.data());
}
#endif

void createGBuffer(int gbuffer_idx, ConnectedDisplay* display){// Initialize the Glew

    glXMakeCurrent(display->dpy, display->win, display->glctx);

    glGenFramebuffers(1, &gbuffer[gbuffer_idx]);
    glBindFramebuffer(GL_FRAMEBUFFER, gbuffer[gbuffer_idx]);

    glGenTextures(2, gbuffer_tex[gbuffer_idx]);
    glBindTexture(GL_TEXTURE_2D, gbuffer_tex[gbuffer_idx][0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, windowWidth, windowHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, gbuffer_tex[gbuffer_idx][1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, windowWidth, windowHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gbuffer_tex[gbuffer_idx][0], 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, gbuffer_tex[gbuffer_idx][1], 0);

    static const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0 };

    glDrawBuffers(1, draw_buffers);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer is complete.\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void generate_rand_texture(GLuint& textureId) {
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, windowWidth, windowHeight);

    std::vector<float> data(windowWidth*windowHeight*4);
    for(int i = 0; i < windowWidth * windowHeight; i++){
        data[4*i+0] = (rand() % 1000) / 1000.0f;
        data[4*i+1] = (rand() % 1000) / 1000.0f;
        data[4*i+2] = (rand() % 1000) / 1000.0f;
        data[4*i+3] = 1.0f;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, windowWidth, windowHeight, GL_RGBA, GL_FLOAT, data.data());
}

void generate_rand_depth_texture(GLuint& textureId) {
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, windowWidth, windowHeight);

    std::vector<float> data(windowWidth*windowHeight);
    for(int i = 0; i < windowWidth * windowHeight; i++){
        data[i] = 0.0f;
    }

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, windowWidth, windowHeight, GL_DEPTH_COMPONENT, GL_HALF_FLOAT, data.data());
}

void render(int gbuffer_idx, ConnectedDisplay* display){

    glXMakeCurrent(display->dpy, display->win, display->glctx);

    static const GLfloat float_zeros[] = { 0.0f, 0.0f, 0.5f, 1.0f };
    static const GLfloat float_one = 1.f;

    clock_t start_time = clock();
    float currTime = 0.0f;
    while(true){

        clock_t start_frame_time = clock();

        while(!framebuffer_mutex[gbuffer_idx].try_lock());

        glViewport(0, 0, windowWidth, windowHeight);


        glBindFramebuffer(GL_FRAMEBUFFER, gbuffer[gbuffer_idx]);

        glClearBufferfv(GL_COLOR, 0, float_zeros);
        glClearBufferfv(GL_DEPTH, 0, &float_one);

        glUseProgram(program_shader[gbuffer_idx]);

        GLint world_mat_loc = glGetUniformLocation(program_shader[gbuffer_idx], "world_mat");
        GLint view_mat_loc  = glGetUniformLocation(program_shader[gbuffer_idx], "view_mat");
        GLint proj_mat_loc  = glGetUniformLocation(program_shader[gbuffer_idx], "proj_mat");
        GLint color_loc     = glGetUniformLocation(program_shader[gbuffer_idx], "color");

        currTime = (float)(clock() - start_time) / 100000.0f;
        if(gbuffer_idx == 0)
            world_mat[gbuffer_idx] = glm::rotate(glm::mat4(1.0f), -static_cast<float>(currTime) / 10.0f, glm::vec3(0.0f, 0.0f, 1.0f));
        else if(gbuffer_idx == 1)
            world_mat[gbuffer_idx] = glm::rotate(glm::mat4(1.0f), static_cast<float>(currTime) / 20.0f, glm::vec3(0.0f, 0.0f, 1.0f));

        glUniformMatrix4fv(world_mat_loc, 1, GL_FALSE, (GLfloat*)&world_mat[gbuffer_idx][0]);
        glUniformMatrix4fv(view_mat_loc, 1, GL_FALSE, (GLfloat*)&view_mat[0]);
        glUniformMatrix4fv(proj_mat_loc, 1, GL_FALSE, (GLfloat*)&proj_mat[0]);
        glUniform4fv(color_loc, 1, (GLfloat*)&colors[gbuffer_idx]);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, triangle_buf[gbuffer_idx]);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableVertexAttribArray(0);

        glFinish();

        framebuffer_mutex[gbuffer_idx].unlock();



    #define SHOW_EACH_GPU_RESULT
    #ifdef SHOW_EACH_GPU_RESULT
        glXMakeCurrent(display->dpy, display->win, display->glctx);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program_shader[gbuffer_idx]);

        glUniformMatrix4fv(world_mat_loc, 1, GL_FALSE, (GLfloat*)&world_mat[0]);
        glUniformMatrix4fv(view_mat_loc, 1, GL_FALSE, (GLfloat*)&view_mat[0]);
        glUniformMatrix4fv(proj_mat_loc, 1, GL_FALSE, (GLfloat*)&proj_mat[0]);
        glUniform4fv(color_loc, 1, (GLfloat*)&colors[gbuffer_idx]);

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, triangle_buf[gbuffer_idx]);

        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableVertexAttribArray(0);

        glXSwapBuffers(display->dpy, display->win);

        glFinish();
    #endif

        clock_t end_frame_time = clock();
        float duration = (float)(end_frame_time - start_frame_time) / CLOCKS_PER_SEC;

        std::cout << gbuffer_idx << " .FPS: " << 1.0f / duration << std::endl;

    }
}

void render_main(ConnectedDisplay* display){

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(combine_shader);

    glEnableVertexAttribArray(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_texture[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depth_texture[0]);
    glBindBuffer(GL_ARRAY_BUFFER, quad_buf);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, color_texture[1]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depth_texture[1]);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);

    glXSwapBuffers(display->dpy, display->win);
}

void init_main_display(ConnectedDisplay* display){

    glXMakeCurrent(display->dpy, display->win, display->glctx);

    combine_shader = compile_link_vs_fs("/home/smostaja/DeferredShading/quad.vert", "/home/smostaja/DeferredShading/combine.frag");
    glGenBuffers(1, &quad_buf);
    glBindBuffer(GL_ARRAY_BUFFER, quad_buf);
    glBufferData(GL_ARRAY_BUFFER, quad_verts.size() * sizeof(glm::vec4), quad_verts.data(), GL_STATIC_DRAW);

    generate_rand_texture(color_texture[0]);
    generate_rand_texture(color_texture[1]);
    generate_rand_depth_texture(depth_texture[0]);
    generate_rand_depth_texture(depth_texture[1]);
}

int main()
{
    vertices.push_back(glm::vec4(-1, -1, 0, 1));
    vertices.push_back(glm::vec4( 1, -1, 0, 1));
    vertices.push_back(glm::vec4( 0,  1, 0, 1));

    quad_verts.push_back(glm::vec4(-1, -1, 0, 1));
    quad_verts.push_back(glm::vec4( 1, -1, 0, 1));
    quad_verts.push_back(glm::vec4(-1,  1, 0, 1));
    quad_verts.push_back(glm::vec4( 1,  1, 0, 1));

    // Triangle Colors
    colors[0] = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    colors[1] = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);

    // Create a Display
    boost::thread create_display_thread(create_display, ":0.0", &main_display);
    create_display_thread.join();

    boost::thread init_main_display_thread(init_main_display, &main_display);
    init_main_display_thread.join();

    // Create Two other displays
    boost::thread create_display_thread0(create_display, ":0.0", &display[0]);
    boost::thread create_display_thread1(create_display, ":0.1", &display[1]);
    create_display_thread0.join();
    create_display_thread1.join();

    // Create Two different g buffers
    boost::thread create_gbuff_thread0( createGBuffer, 0, &display[0] );
    boost::thread create_gbuff_thread1( createGBuffer, 1, &display[1] );
    create_gbuff_thread0.join();
    create_gbuff_thread1.join();

    // Generate Buffers Thread
    boost::thread gen_buffers_thread0(genBuffers, 0, &display[0]);
    boost::thread gen_buffers_thread1(genBuffers, 1, &display[1]);
    gen_buffers_thread0.join();
    gen_buffers_thread1.join();

    boost::thread render_thread0(render, 0, &display[0]);
    boost::thread render_thread1(render, 1, &display[1]);
    boost::thread main_event_loop(main_event_handler, &main_display, &display[0], &display[1]);

    // Run the main window thread
    render_thread0.join();
    render_thread1.join();
    main_event_loop.join();




    return 0;
}

