/*
  Simple DirectMedia Layer
  Copyright (C) 2017 BlackBerry Limited

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"
#include "../SDL_sysvideo.h"
#include "SDL_syswm.h"
#include "sdl_qnx.h"
#include "screen_consts.h"

#include <errno.h>

static int initialized = 0;

/**
 * Initializes the QNX video plugin.
 * Creates the Screen context and event handles used for all window operations
 * by the plugin.
 * Note: Display driverdata is NOT set
 * @param   _THIS
 * @return  0 if successful, -1 on error
 */
static int
videoInit(_THIS)
{
    SDL_VideoDisplay display;
    initialized = 1;

    if (screen_create_context(&context, SCREEN_APPLICATION_CONTEXT) < 0) {
        printf("qnx/video.c: | Context creation failure with errno %d\n", errno);
        return -1;
    }

    if (screen_create_event(&event) < 0) {
        printf("qnx/video.c: | Event creation failure with errno %d\n", errno);
        return -1;
    }

    SDL_zero(display);

    if (SDL_AddVideoDisplay(&display, SDL_FALSE) < 0) {
        return -1;
    }

    _this->num_displays = 1;
    return 0;
}

static void
videoQuit(_THIS)
{
}

/**
 * Creates a new native Screen window and associates it with the given SDL
 * window.
 * @param   _THIS
 * @param   window  SDL window to initialize
 * @return  0 if successful, -1 on error
 */
static int
createWindow(_THIS, SDL_Window *window)
{
    window_impl_t   *impl;
    int             size[2];
    int             pos[2] = {0,0};
    int             interval = 1;
    int             numbufs;
    int             format;
    int             usage;
    int             has_focus_i;

    impl = SDL_calloc(1, sizeof(*impl));
    if (impl == NULL) {
        return -1;
    }

    impl->is_fullscreen = SDL_FALSE;
    impl->fs_lastsize[0] = 0;
    impl->fs_lastsize[1] = 1;

    // Create a native window.
    if (screen_create_window_type(&(impl->window), context, SCREEN_APPLICATION_WINDOW) < 0) {
        printf("qnx/video.c: | Creating window of type SCREEN_APPLICATION_WINDOW failed with errno %d\n", errno);
        goto fail;
    }

    // Set the native window's size to match the SDL window.
    size[0] = window->w;
    size[1] = window->h;
    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE,
                                      size) < 0) {
        printf("qnx/video.c: | Setting SCREEN_PROPERTY_SIZE failed with errno %d\n", errno);
        goto fail;
    } //Sets buffer size and source size implicitly

    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SWAP_INTERVAL,
                                      &interval) < 0) {
        printf("qnx/video.c: | Setting SCREEN_PROPERTY_SWAP_INTERVAL failed with errno %d\n", errno);
        goto fail;
    }

    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_POSITION,
                                      pos) < 0) {
        printf("qnx/video.c: | Setting SCREEN_PROPERTY_POSITION failed with errno %d\n", errno);
        goto fail;
    }

    // Create window buffer(s).
    if (window->flags & SDL_WINDOW_OPENGL) {

        if (glGetConfig(&impl->conf, &format) < 0) {
            printf("qnx/video.c: | SDL Could not get GL configs \n");
            goto fail;
        }

        numbufs = 2;
        format = SCREEN_FORMAT_RGBX8888;

        usage = SCREEN_USAGE_OPENGL_ES2 | SCREEN_USAGE_OPENGL_ES3;
        if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_USAGE,
                                          &usage) < 0) {
            printf("qnx/video.c: | SDL could not set screen usage to OPENGL_ES2, OPENGL_ES3 \n");
            return -1;
        }
    } else {
        format = SCREEN_FORMAT_RGBX8888;
        numbufs = 2;
    }

    // Set pixel format 
    if (screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_FORMAT,
                                      &format) < 0) {
        printf("qnx/video.c: | Setting SCREEN_PROPERTY_FORMAT failed with errno %d\n", errno);
        goto fail;
    }

    // Create buffer(s).
    if (screen_create_window_buffers(impl->window, numbufs>0?numbufs:1) < 0) {
        printf("qnx/video.c: | Creating window buffers failed with errno %d\n", errno);
        goto fail;
    }

    // Get initial focus state. Fallback to true.
    if(screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_FOCUS, &has_focus_i) < 0){
        impl->has_focus = SDL_TRUE;
    } else {
        impl->has_focus = has_focus_i ? SDL_TRUE : SDL_FALSE;
    }

    window->driverdata = impl;
    return 0;

fail:
    if (impl->window) {
        screen_destroy_window(impl->window);
    }
    SDL_free(impl);
    return -1;
}

/**
 * Gets a pointer to the Screen buffer associated with the given window. Note
 * that the buffer is actually created in createWindow().
 * @param       _THIS
 * @param       window  SDL window to get the buffer for
 * @param[out]  pixels  Holds a pointer to the window's buffer
 * @param[out]  format  Holds the pixel format for the buffer
 * @param[out]  pitch   Holds the number of bytes per line
 * @return  0 if successful, -1 on error
 */
static int
createWindowFramebuffer(_THIS, SDL_Window * window, Uint32 * format,
                        void ** pixels, int *pitch)
{
    int buffer_count;
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    screen_buffer_t *buffer;

    if (screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_BUFFER_COUNT,
        &buffer_count) < 0) {
        printf("qnx/video.c: | Getting SCREEN_PROPERTY_BUFFER_COUNT failed with errno %d\n", errno);
        return -1;
    }
    buffer = calloc(buffer_count, sizeof(screen_buffer_t));

    // Get a pointer to the buffer's memory.
    if (screen_get_window_property_pv(impl->window, SCREEN_PROPERTY_BUFFERS,
                                      buffer) < 0) {
        printf("qnx/video.c: | Getting SCREEN_PROPERTY_BUFFERS failed with errno %d\n", errno);
        return -1;
    }

    if (screen_get_buffer_property_pv(*buffer, SCREEN_PROPERTY_POINTER,
                                      pixels) < 0) {
        printf("qnx/video.c: | Getting SCREEN_PROPERTY_POINTER failed with errno %d\n", errno);
        return -1;
    }

    // Set format and pitch.
    if (screen_get_buffer_property_iv(*buffer, SCREEN_PROPERTY_STRIDE,
                                      pitch) < 0) {
        printf("qnx/video.c: | Getting SCREEN_PROPERTY_STRIDE failed with errno %d\n", errno);
        return -1;
    }


    *format = SDL_PIXELFORMAT_RGBX8888;
    return 0;
}

/**
 * Informs the window manager that the window needs to be updated.
 * @param   _THIS
 * @param   window      The window to update
 * @param   rects       An array of reectangular areas to update
 * @param   numrects    Rect array length
 * @return  0 if successful, -1 on error
 */
static int
updateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects,
                        int numrects)
{
    int buffer_count, *rects_int;
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    screen_buffer_t *buffer;

    if (screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_BUFFER_COUNT,
        &buffer_count) < 0) {
        printf("qnx/video.c: | Getting SCREEN_PROPERTY_BUFFER_COUNT failed with errno %d\n", errno);
        return -1;
    }
    buffer = calloc(buffer_count, sizeof(screen_buffer_t));

    if (screen_get_window_property_pv(impl->window, SCREEN_PROPERTY_BUFFERS,
                                      buffer) < 0) {
        printf("qnx/video.c: | Getting SCREEN_PROPERTY_BUFFERS failed with errno %d\n", errno);
        return -1;
    }

    if(numrects>0){
        rects_int = calloc(4*numrects, sizeof(int));

        for(int i = 0; i < numrects; i++){
            rects_int[4*i]   = rects[i].x;
            rects_int[4*i+1] = rects[i].y;
            rects_int[4*i+2] = rects[i].w;
            rects_int[4*i+3] = rects[i].h;
        }

        if(screen_post_window(impl->window, buffer[0], numrects, rects_int, 0)) 
            printf("qnx/video.c: | Screen Post Window error - errno %d\n", errno);
        if(screen_flush_context(context, 0)) 
            printf("qnx/video.c: | Screen flush context error - errno %d\n", errno);
    }
    return 0;
}

/**
 * Runs the main event loop.
 * @param   _THIS
 */
static void
pumpEvents(_THIS)
{
    SDL_Window      *window;
    window_impl_t   *impl;
    int             type;
    int             has_focus_i;
    SDL_bool        has_focus;

    // Let apps know the state of focus.
    for (window = _this->windows; window; window = window->next) {
        impl = (window_impl_t *)window->driverdata;
        if (screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_FOCUS, &has_focus_i) < 0){
            printf("qnx/video.c: | Get focus failed with errno %d\n", errno);
            continue;
        }
        has_focus = has_focus_i ? SDL_TRUE : SDL_FALSE;

        if (impl->has_focus != has_focus) {
            // Assume here that we are always inside the window, since there is
            // currently no x/y positioning.
            SDL_SendWindowEvent(window, (has_focus ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST), 0, 0);
            SDL_SendWindowEvent(window, (has_focus ? SDL_WINDOWEVENT_ENTER : SDL_WINDOWEVENT_LEAVE), 0, 0);
        }
    }

    for (;;) {
        if(!context) break;
        if (screen_get_event(context, event, 0) < 0) {
            break;
        }

        if(!event) break;

        if (screen_get_event_property_iv(event, SCREEN_PROPERTY_TYPE, &type)
            < 0) {
            break;
        }

        if (type == SCREEN_EVENT_NONE) {
            break;
        }

        switch (type) {
        case SCREEN_EVENT_KEYBOARD:
            handleKeyboardEvent(event);
            break;

        case SCREEN_EVENT_POINTER:
            handlePointerEvent(event);
            break;

        //#ifdef SDL_JOYSTICK_QNX
        case SCREEN_EVENT_GAMEPAD:
        case SCREEN_EVENT_JOYSTICK:
            handleJoystickEvent(event);
            break;
        //#endif
        default:
            break;
        }
    }
}

/**
 * Updates the size of the native window using the geometry of the SDL window.
 * @param   _THIS
 * @param   window  SDL window to update
 */
static void
setWindowSize(_THIS, SDL_Window *window)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    int             size[2];

    size[0] = window->w;
    size[1] = window->h;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE, size);
    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SOURCE_SIZE,
                                  size);
}

/**
 * Makes the native window associated with the given SDL window visible.
 * @param   _THIS
 * @param   window  SDL window to update
 */
static void
showWindow(_THIS, SDL_Window *window)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    const int       visible = 1;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_VISIBLE,
                                  &visible);
}

/**
 * Makes the native window associated with the given SDL window invisible.
 * @param   _THIS
 * @param   window  SDL window to update
 */
static void
hideWindow(_THIS, SDL_Window *window)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;
    const int       visible = 0;

    screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_VISIBLE,
        &visible);
}

/**
 * Destroys the native window associated with the given SDL window.
 * @param   _THIS
 * @param   window  SDL window that is being destroyed
 */
static void
destroyWindow(_THIS, SDL_Window *window)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;

    if (impl) {
        screen_destroy_window(impl->window);
        window->driverdata = NULL;
    }
}

/**
 * Returns information about the window type.
 * @param   _THIS
 * @param   window  SDL window to get info for
 * @param   info    The resulting info
 */
static SDL_bool
getWindowWMInfo(_THIS, SDL_Window * window, SDL_SysWMinfo * info)
{
    window_impl_t   *impl = (window_impl_t *)window->driverdata;

    if (!impl) {
        SDL_SetError("Window not initialized");
        return SDL_FALSE;
    }

    if (info->version.major == SDL_MAJOR_VERSION &&
        info->version.minor == SDL_MINOR_VERSION) {
        info->subsystem = SDL_SYSWM_QNX;
        info->info.qnx.window = &impl->window;
        info->info.qnx.surface = impl->surface;
        return SDL_TRUE;
    } else {
        SDL_SetError("Application not compiled with SDL %d.%d",
                     SDL_MAJOR_VERSION, SDL_MINOR_VERSION);
        return SDL_FALSE;
    }
}

/**
 * Frees the plugin object created by createDevice().
 * @param   device  Plugin object to free
 */
static void
deleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}


void setWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen){
    window_impl_t       *impl = (window_impl_t*) window->driverdata;
    screen_display_t    *disp;
    int                 fullscreen_size[2];
    int                 ndevices;


    if(fullscreen == impl->is_fullscreen) return;

    if(screen_get_context_property_iv(context, SCREEN_PROPERTY_DISPLAY_COUNT, &ndevices)){
        printf("qnx/video.c: |  qnx getDisplayDPI Failed to query for display count w errno %d\n", errno);
        return ;
    }
    disp = (screen_display_t*)calloc(ndevices, sizeof(screen_display_t));
    if(screen_get_context_property_pv(context, SCREEN_PROPERTY_DISPLAYS, disp)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display w errno %d\n", errno);
        free(disp);
        return ;
    }
    if(screen_get_display_property_iv(disp[0], SCREEN_PROPERTY_SIZE, &fullscreen_size)){
        printf("qnx/video.c: | qnx getDisplayBounds Failed to query for size w errno %d\n", errno);
        free(disp);
        return ;
    }

    if(fullscreen == SDL_TRUE){
        if(screen_get_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE, impl->fs_lastsize)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display w errno %d\n", errno);
        free(disp);
        return ;
        }
        if(screen_get_display_property_iv(disp, SCREEN_PROPERTY_SIZE, fullscreen_size)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display w errno %d\n", errno);
        free(disp);
        return ;
        }
        if(screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE, fullscreen_size)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display w errno %d\n", errno);
        free(disp);
        return ;
        }
        //screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_BUFFER_SIZE, );
        //screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SOURCE_SIZE, );
        impl->is_fullscreen = SDL_TRUE;
    }else{
        if(screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SIZE, impl->fs_lastsize)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display w errno %d\n", errno);
        free(disp);
        return ;
        }
        //screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_BUFFER_SIZE, impl->fs_lastsize);
        //screen_set_window_property_iv(impl->window, SCREEN_PROPERTY_SOURCE_SIZE, impl->fs_lastsize);
        impl->is_fullscreen = SDL_FALSE;
    }

    free(disp);
}

/*
* Get the bounds of a display
*/
int getDisplayBounds(_THIS, SDL_VideoDisplay * display, SDL_Rect * rect){
    screen_display_t    *disp;
    window_impl_t       *sdl_win;
    int                 size[2];
    int                 ndevices;

    if(screen_get_context_property_iv(context, SCREEN_PROPERTY_DISPLAY_COUNT, &ndevices)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display count w errno %d\n", errno);
        return -1;
    }
    disp = (screen_display_t*)calloc(ndevices, sizeof(screen_display_t));
    if(screen_get_context_property_pv(context, SCREEN_PROPERTY_DISPLAYS, disp)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display w errno %d\n", errno);
        free(disp);
        return -1;
    }
    if(screen_get_display_property_iv(disp[0], SCREEN_PROPERTY_SIZE, &size)){
        printf("qnx/video.c: | qnx getDisplayBounds Failed to query for size w errno %d\n", errno);
        free(disp);
        return -1;
    }

    rect->w = size[0];
    rect->h = size[1];
    rect->x = 0;
    rect->y = 0;

    free(disp);
    return 0;
}

/**
 * Hidden function for calculating ddpi from hdpi, vdpi, and screen size
 */
int _calculateDDPI(float* ret, int* screen_dpi, int* screen_size){
    float dh_2, dv_2, ph_2, pv_2;

    if(screen_dpi[0]==0||screen_dpi[1]==0) return -1;
    if(screen_size[0] + screen_size[1] == 0) return -1;

    ph_2 = screen_size[0] * screen_size[0];
    pv_2 = screen_size[0] * screen_size[0];
    dh_2 = screen_dpi[0] * screen_dpi[0];
    dv_2 = screen_dpi[1] * screen_dpi[1];

    *ret = sqrt((ph_2+pv_2)/(ph_2/dh_2+pv_2/dv_2));
    return 0;
}

/*
* Get the dots/pixels-per-inch of a display
*/
int getDisplayDPI(_THIS, SDL_VideoDisplay * display, float * ddpi, float * hdpi, float * vdpi){
    screen_display_t*    disp;
    int                 ndevices;
    int                 dpi_as_int[2];
    int                 size[2];

    if(screen_get_context_property_iv(context, SCREEN_PROPERTY_DISPLAY_COUNT, &ndevices)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display count w errno %d\n", errno);
        return -1;
    }
    disp = (screen_display_t*)calloc(ndevices, sizeof(screen_display_t));
    

    if(screen_get_context_property_pv(context, SCREEN_PROPERTY_DISPLAYS, disp)){
        printf("qnx/video.c: | qnx getDisplayDPI Failed to query for display w errno %d\n", errno);
        free(disp);
        return -1;
    }

    if(screen_get_display_property_iv(disp[0], SCREEN_PROPERTY_DPI, &dpi_as_int)){
        printf("qnx/video.c: | qnx getDisplayDPi Failed to query for dpi w errno %d\n", errno);
        free(disp);
        return -1;
    }

    if(!dpi_as_int){ free(disp); return -1;}

    if(ddpi){
        if(screen_get_display_property_iv(disp[0], SCREEN_PROPERTY_SIZE, &size)){
            printf("qnx/video.c: | qnx getDisplayDPi Failed to query for size w errno %d\n", errno);
            free(disp);
            return -1;
        }
        if(!size) {free(disp); return -1;}
        if(_calculateDDPI(ddpi, dpi_as_int, size) < 0){free(disp);return -1;}
    }

    if(hdpi) *hdpi = dpi_as_int[0];
    if(vdpi) *vdpi = dpi_as_int[1];

    free(disp);
    return 0;
}


void getDisplayModes(_THIS, SDL_VideoDisplay * display){
    int nmodes, ndisplays;
    
    screen_display_t* disp;
    screen_display_mode_t* modes;

    if(screen_get_context_property_iv(context, SCREEN_PROPERTY_DISPLAY_COUNT, &ndisplays)){
        printf("qnx/video.c: | qnx getDisplayModes Failed to query for display count w errno %d\n", errno);
        return -1;
    }
    disp = (screen_display_t*)calloc(ndisplays, sizeof(screen_display_t));
    
    if(screen_get_context_property_pv(context, SCREEN_PROPERTY_DISPLAYS, disp)){
        printf("qnx/video.c: | qnx getDisplayModes abs Failed to query for displays w errno %d\n", errno);
        free(disp);
        return -1;
    }

    if(screen_get_display_property_iv(disp[0], SCREEN_PROPERTY_MODE_COUNT, &nmodes)){
        printf("qnx/video.c: | qnx getDisplayModes Failed to query for mode count w errno %d\n", errno);
        free(disp);
        return -1;
    }
    modes = (screen_display_mode_t*)calloc(nmodes, sizeof(screen_display_mode_t));

    if(screen_get_display_modes(disp[0], nmodes, modes)){
        printf("qnx/video.c: | qnx getDisplayModes Failed to query for modes w errno %d\n", errno);
        free(disp);
        free(modes);
        return -1;
    }


    for(int i = 0; i < nmodes; i++){
        SDL_DisplayMode Mode;
        Mode.format = SDL_PIXELFORMAT_RGBX8888;
        Mode.w = modes[i].width;
        Mode.h = modes[i].height;
        Mode.refresh_rate = modes[i].refresh;
        Mode.driverdata = &(modes[i]);
        SDL_AddDisplayMode(display, &Mode);
    }

    free(disp);

    //SDL_AddDisplayMode(display, &mode);
}

// int setDisplayMode(_THIS, SDL_VideoDisplay * display, SDL_DisplayMode * mode){
//     int ndisplays, index;
//     screen_display_t* disp;

//     printf("QNXVid SetDisplayMode\n");

//     if(screen_get_context_property_iv(context, SCREEN_PROPERTY_DISPLAY_COUNT, &ndisplays)){
//         printf("qnx/video.c: | qnx getDisplayModes Failed to query for display count w errno %d\n", errno);
//         return -1;
//     }
//     disp = (screen_display_t*)calloc(ndisplays, sizeof(screen_display_t));
    
//     if(screen_get_context_property_pv(context, SCREEN_PROPERTY_DISPLAYS, disp)){
//         printf("qnx/video.c: | qnx getDisplayModes abs Failed to query for displays w errno %d\n", errno);
//         free(disp);
//         return -1;
//     }

//     index = ((screen_display_mode_t*)(mode->driverdata))->index;

//     if(screen_set_display_property_iv(disp[0], SCREEN_PROPERTY_MODE, &index)){
//         printf("qnx/video.c: | qnx getDisplayModes Failed to set mode w errno %d\n", errno);
//         free(disp);
//         return -1;
//     }

//     free(disp);
    
// }

/**
 * Creates the QNX video plugin used by SDL.
 * @param   devindex    Unused
 * @return  Initialized device if successful, NULL otherwise
 */
static SDL_VideoDevice *
createDevice(int devindex)
{
    SDL_VideoDevice *device;


    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (device == NULL) {
        return NULL;
    }

    device->driverdata = NULL;
    device->VideoInit = videoInit;
    device->VideoQuit = videoQuit;
    device->CreateSDLWindow = createWindow;
    device->CreateWindowFramebuffer = createWindowFramebuffer;
    device->UpdateWindowFramebuffer = updateWindowFramebuffer;
    device->SetWindowSize = setWindowSize;
    device->ShowWindow = showWindow;
    device->HideWindow = hideWindow;
    device->PumpEvents = pumpEvents;
    device->DestroyWindow = destroyWindow;
    device->GetWindowWMInfo = getWindowWMInfo;
    device->SetWindowFullscreen = setWindowFullscreen;

    device->GetDisplayBounds = getDisplayBounds;
    device->GetDisplayDPI = getDisplayDPI;
    device->GetDisplayUsableBounds = getDisplayBounds;
    //device->GetDisplayModes = getDisplayModes;
    //device->SetDisplayMode = setDisplayMode; //Segfaults

    device->GL_LoadLibrary = glLoadLibrary;
    device->GL_GetProcAddress = glGetProcAddress;
    device->GL_CreateContext = glCreateContext;
    device->GL_SetSwapInterval = glSetSwapInterval;
    device->GL_SwapWindow = glSwapWindow;
    device->GL_MakeCurrent = glMakeCurrent;
    device->GL_DeleteContext = glDeleteContext;
    device->GL_UnloadLibrary = glUnloadLibrary;

    device->free = deleteDevice;
    return device;
}

VideoBootStrap QNX_bootstrap = {
    "qnx", "QNX Screen",
    createDevice
};
