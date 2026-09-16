#ifndef VIDEO_RPI_H
#define VIDEO_RPI_H

#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <string>
#include <cstdint>

class VideoRpi {
public:
    struct Config {
        int target_width;
        int target_height;
        int window_width;
        int window_height;
        bool fullscreen;
        bool integer_scaling;
        bool vsync_enabled;
    };

    explicit VideoRpi(const Config& config);
    ~VideoRpi();

    bool initialize();
    void shutdown();

    bool beginFrame();
    bool endFrame();

    GLuint createTexture(int width, int height);
    void updateTexture(GLuint texture, const uint32_t* pixels, int width, int height);

    SDL_Window* getWindow() const { return window_; }
    SDL_GLContext getGLContext() const { return gl_context_; }
    EGLDisplay getEGLDisplay() const { return egl_display_; }
    EGLSurface getEGLSurface() const { return egl_surface_; }
    EGLConfig getEGLConfig() const { return egl_config_; }

    const Config& getConfig() const { return config_; }

private:
    bool initKMSDRM();
    bool initOpenGLES();
    bool setupPowerManagement();
    void computeScaling(int* out_window_w, int* out_window_h) const;

    Config config_;
    SDL_Window* window_;
    SDL_GLContext gl_context_;
    SDL_Surface* kms_framebuffer_;
    EGLDisplay egl_display_;
    EGLSurface egl_surface_;
    EGLConfig egl_config_;
    EGLContext egl_context_;
    bool kms_available_;
};

#endif
