#include "video_rpi.h"
#include <SDL2/SDL_syswm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <GLES2/gl2ext.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <cassert>

VideoRpi::VideoRpi(const Config& config)
    : config_(config)
    , window_(nullptr)
    , gl_context_(nullptr)
    , kms_framebuffer_(nullptr)
    , egl_display_(EGL_NO_DISPLAY)
    , egl_surface_(EGL_NO_SURFACE)
    , egl_config_(nullptr)
    , egl_context_(EGL_NO_CONTEXT)
    , kms_available_(false)
{
}

VideoRpi::~VideoRpi() {
    shutdown();
}

bool VideoRpi::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!setupPowerManagement()) {
        std::cerr << "Failed to set power management" << std::endl;
    }

    if (!initKMSDRM()) {
        std::cerr << "KMSDRM init failed, falling back to SDL2 windowed" << std::endl;
    }

    if (!initOpenGLES()) {
        std::cerr << "OpenGL ES initialization failed" << std::endl;
        return false;
    }

    int sw, sh;
    computeScaling(&sw, &sh);
    std::cout << "[VideoRpi] Target: " << config_.target_width << "x" << config_.target_height
              << " | Window: " << sw << "x" << sh
              << " | Integer scaling: " << (config_.integer_scaling ? "ON" : "OFF") << std::endl;

    return true;
}

void VideoRpi::shutdown() {
    if (egl_context_ != EGL_NO_CONTEXT) {
        eglDestroyContext(egl_display_, egl_context_);
        egl_context_ = EGL_NO_CONTEXT;
    }
    if (egl_surface_ != EGL_NO_SURFACE) {
        eglDestroySurface(egl_display_, egl_surface_);
        egl_surface_ = EGL_NO_SURFACE;
    }
    if (egl_display_ != EGL_NO_DISPLAY) {
        eglTerminate(egl_display_);
        egl_display_ = EGL_NO_DISPLAY;
    }

    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    if (kms_framebuffer_) {
        SDL_FreeSurface(kms_framebuffer_);
        kms_framebuffer_ = nullptr;
    }

    SDL_Quit();
}

bool VideoRpi::setupPowerManagement() {
    int fd = open("/sys/class/backlight/intel_backlight/bl_power", O_WRONLY);
    if (fd < 0) {
        fd = open("/sys/class/backlight/rpi_backlight/bl_power", O_WRONLY);
    }
    if (fd >= 0) {
        write(fd, "1", 1);
        close(fd);
        return true;
    }
    return false;
}

bool VideoRpi::initKMSDRM() {
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) {
        kms_available_ = false;
        return false;
    }

    drmModeRes* res = drmModeGetResources(fd);
    if (!res) {
        close(fd);
        kms_available_ = false;
        return false;
    }

    drmModeCrtc* crtc = nullptr;
    for (int i = 0; i < res->count_crtcs; i++) {
        crtc = drmModeGetCrtc(fd, res->crtcs[i]);
        if (crtc) {
            break;
        }
    }
    drmModeFreeResources(res);

    if (!crtc) {
        close(fd);
        kms_available_ = false;
        return false;
    }
    drmModeFreeCrtc(crtc);
    close(fd);

    kms_available_ = true;
    return true;
}

bool VideoRpi::initOpenGLES() {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (config_.fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    int sw, sh;
    computeScaling(&sw, &sh);

    window_ = SDL_CreateWindow(
        "Ocarina of Pi",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        sw, sh,
        flags
    );
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        return false;
    }

    if (config_.vsync_enabled) {
        SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }

    egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display_ == EGL_NO_DISPLAY) {
        std::cerr << "eglGetDisplay failed" << std::endl;
        return false;
    }

    if (!eglInitialize(egl_display_, nullptr, nullptr)) {
        std::cerr << "eglInitialize failed" << std::endl;
        return false;
    }

    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(egl_display_, attribs, &egl_config_, 1, &numConfigs) || numConfigs == 0) {
        std::cerr << "eglChooseConfig failed" << std::endl;
        return false;
    }

    egl_surface_ = eglCreateWindowSurface(egl_display_, egl_config_, window_, nullptr);
    if (egl_surface_ == EGL_NO_SURFACE) {
        std::cerr << "eglCreateWindowSurface failed: " << eglGetError() << std::endl;
        return false;
    }

    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    egl_context_ = eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, contextAttribs);
    if (egl_context_ == EGL_NO_CONTEXT) {
        std::cerr << "eglCreateContext failed" << std::endl;
        return false;
    }

    if (!eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_)) {
        std::cerr << "eglMakeCurrent failed" << std::endl;
        return false;
    }

    glEnable(GL_TEXTURE_2D);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    return true;
}

void VideoRpi::computeScaling(int* out_window_w, int* out_window_h) const {
    if (config_.integer_scaling) {
        int scale = 1;
        while (config_.target_width * (scale + 1) <= config_.window_width &&
               config_.target_height * (scale + 1) <= config_.window_height) {
            scale++;
        }
        *out_window_w = config_.target_width * scale;
        *out_window_h = config_.target_height * scale;
    } else {
        *out_window_w = config_.window_width;
        *out_window_h = config_.window_height;
    }
}

bool VideoRpi::beginFrame() {
    glClear(GL_COLOR_BUFFER_BIT);
    return true;
}

bool VideoRpi::endFrame() {
    eglSwapBuffers(egl_display_, egl_surface_);
    return true;
}

GLuint VideoRpi::createTexture(int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    return texture;
}

void VideoRpi::updateTexture(GLuint texture, const uint32_t* pixels, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}
