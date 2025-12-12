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
#include "../../events/SDL_mouse_c.h"

#include <errno.h>


// TODO: Might need to iterate all windows and attach this cursor to each of
//       them.
static SDL_Cursor *
createCursor(SDL_Surface * surface, int hot_x, int hot_y)
{
    SDL_Cursor          *cursor;
    cursor_impl_t       *impl;
    screen_session_t    session;

    cursor = SDL_calloc(1, sizeof(SDL_Cursor));
    if (cursor) {
        impl = SDL_calloc(1, sizeof(cursor_impl_t));;
        if (impl == NULL) {
            free(cursor);
            SDL_OutOfMemory();
        }
        impl->realized_shape = SCREEN_CURSOR_SHAPE_ARROW;

        screen_create_session_type(&session, context, SCREEN_EVENT_POINTER);
        screen_set_session_property_iv(session, SCREEN_PROPERTY_CURSOR, &impl->realized_shape);

        impl->session = session;
        impl->is_visible = 1;
        cursor->driverdata = (void*)impl;
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static SDL_Cursor *
createSystemCursor(SDL_SystemCursor id)
{
    SDL_Cursor          *cursor;
    cursor_impl_t       *impl;
    screen_session_t    session;
    int shape;

    switch(id)
    {
    default:
        SDL_assert(0);
        return NULL;
    case SDL_SYSTEM_CURSOR_ARROW:     shape = SCREEN_CURSOR_SHAPE_ARROW; break;
    case SDL_SYSTEM_CURSOR_IBEAM:     shape = SCREEN_CURSOR_SHAPE_IBEAM; break;
    case SDL_SYSTEM_CURSOR_WAIT:      shape = SCREEN_CURSOR_SHAPE_WAIT; break;
    case SDL_SYSTEM_CURSOR_CROSSHAIR: shape = SCREEN_CURSOR_SHAPE_CROSS; break;
    case SDL_SYSTEM_CURSOR_WAITARROW: shape = SCREEN_CURSOR_SHAPE_WAIT; break;
    case SDL_SYSTEM_CURSOR_SIZENWSE:  shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_SIZENESW:  shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_SIZEWE:    shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_SIZENS:    shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_SIZEALL:   shape = SCREEN_CURSOR_SHAPE_MOVE; break;
    case SDL_SYSTEM_CURSOR_NO:        shape = SCREEN_CURSOR_SHAPE_ARROW; break;
    case SDL_SYSTEM_CURSOR_HAND:      shape = SCREEN_CURSOR_SHAPE_HAND; break;
    }

    cursor = SDL_calloc(1, sizeof(SDL_Cursor));
    if (cursor) {
        impl = SDL_calloc(1, sizeof(cursor_impl_t));;
        if (impl == NULL) {
            free(cursor);
            SDL_OutOfMemory();
        }
        impl->realized_shape = shape;

        screen_create_session_type(&session, context, SCREEN_EVENT_POINTER);
        screen_set_session_property_iv(session, SCREEN_PROPERTY_CURSOR, &shape);

        impl->session = session;
        impl->is_visible = 1;
        cursor->driverdata = (void*)impl;
    } else {
        SDL_OutOfMemory();
    }

    return cursor;
}

static int
showCursor(SDL_Cursor * cursor)
{
    cursor_impl_t       *impl;
    screen_session_t    session;
    int shape;

    // SDL does not provide information about previous visibility to its
    // drivers. We need to track that ourselves.
    if (cursor) {
        impl = (cursor_impl_t*)cursor->driverdata;
        if (impl->is_visible) {
            return 0;
        }
        session = impl->session;
        shape = impl->realized_shape;
        impl->is_visible = 1;
    } else {
        cursor = SDL_GetCursor();
        if (cursor == NULL) {
            return -1;
        }
        impl = (cursor_impl_t*)cursor->driverdata;
        if (!impl->is_visible) {
            return 0;
        }
        session = impl->session;
        shape = SCREEN_CURSOR_SHAPE_NONE;
        impl->is_visible = 0;
    }

    if (screen_set_session_property_iv(session, SCREEN_PROPERTY_CURSOR, &shape) < 0) {
        return -1;
    }

    return 0;
}

static void
freeCursor(SDL_Cursor * cursor)
{
    cursor_impl_t *impl = (cursor_impl_t*)cursor->driverdata;

    screen_destroy_session(impl->session);
    SDL_free(impl);
    SDL_free(cursor);
}

void
initMouse(_THIS)
{
    SDL_Mouse *mouse = SDL_GetMouse();

    mouse->CreateCursor = createCursor;
    mouse->CreateSystemCursor = createSystemCursor;
    mouse->ShowCursor = showCursor;
    mouse->FreeCursor = freeCursor;

    SDL_SetDefaultCursor(createCursor(NULL, 0, 0));
}
