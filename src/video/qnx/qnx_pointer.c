#include "sdl_qnx.h"
#include "../../SDL_internal.h"
#include "SDL_mouse.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include "../../events/SDL_mouse_c.h"

static int previous = 0;

int screenToMouseButton(int x)
{
    //Screen only supports 3 mouse buttons.
    switch(x){
        case SCREEN_LEFT_MOUSE_BUTTON: // 1 << 0
        return SDL_BUTTON_LEFT;
        case SCREEN_RIGHT_MOUSE_BUTTON: //1 << 1
        return SDL_BUTTON_RIGHT;
        case SCREEN_MIDDLE_MOUSE_BUTTON: //1 << 2
        return SDL_BUTTON_MIDDLE;
    }
    return 0;
}

void handlePointerEvent(screen_event_t event)
{
    int              buttons = 0;
    int              mouse_wheel = 0;
    int              mouse_h_wheel = 0;
    int              pos[2] = {0,0};
    int              x_win = 0;
    int              y_win = 0;

    SDL_Mouse        *mouse;
    SDL_Window       *window;
    SDL_VideoDisplay *display;
    SDL_DisplayMode  display_mode;

    screen_get_event_property_iv(event, SCREEN_PROPERTY_BUTTONS, &buttons);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_MOUSE_WHEEL, &mouse_wheel);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_MOUSE_HORIZONTAL_WHEEL, &mouse_h_wheel);
    screen_get_event_property_iv(event, SCREEN_PROPERTY_POSITION, pos);

    mouse = SDL_GetMouse();

    window = mouse->focus;
    display = SDL_GetDisplayForWindow(window);
    if (display == NULL) {
        return;
    }
    display_mode = display->current_mode;

    if ((window->x <= pos[0]) && (window->y <= pos[1])
        && (pos[0] < (window->x + window->w))
        && (pos[1] < (window->y + window->h))) {
        // Capture movement
        // Scale from position relative to display to position relative to window.
        // TODO: We're ignoring the positibility of centered windows.
        x_win = pos[0] - window->x;
        y_win = pos[1] - window->y;

        SDL_SendMouseMotion(window, mouse->mouseID, 0, x_win, y_win);

        // Capture button presses
        for(int i = 0; i < 3; ++i)
        {
            int ret = screenToMouseButton((buttons^previous) & (1 << i));
            if(ret > 0)
            {
                SDL_SendMouseButton(window, mouse->mouseID, (buttons & (1 << i))?SDL_PRESSED:SDL_RELEASED, ret);
            }
        }

        // Capture mouse wheel
        // TODO: Verify this. I can at least confirm that this behaves the same
        //       way as x11.
        SDL_SendMouseWheel(window, 0, (float) mouse_wheel, (float) mouse_h_wheel, SDL_MOUSEWHEEL_NORMAL);
    }

    previous = buttons;
}
