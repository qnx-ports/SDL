#include "sdl_qnx.h"
#include "../../SDL_internal.h"
#include "SDL_mouse.h"
#include "SDL_events.h"
#include "../../events/SDL_mouse_c.h"

static int previous = 0;

int screenToMouseButton(int x){
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

void handlePointerEvent(screen_event_t event){
	int buttons = 0,
		mouse_wheel = 0,
		mouse_h_wheel = 0,
		pos[2] = {0,0},
		displacement[2] = {0,0};
	SDL_Mouse *mouse;

	screen_get_event_property_iv(event, SCREEN_PROPERTY_BUTTONS, &buttons);
	screen_get_event_property_iv(event, SCREEN_PROPERTY_MOUSE_WHEEL, &mouse_wheel);
	screen_get_event_property_iv(event, SCREEN_PROPERTY_MOUSE_HORIZONTAL_WHEEL, &mouse_h_wheel);
	screen_get_event_property_iv(event, SCREEN_PROPERTY_POSITION, pos);
	screen_get_event_property_iv(event, SCREEN_PROPERTY_DISPLACEMENT, displacement);
	
	printf("b:%02x w:%d wh:%d x:%d y:%d diffs:%d %d\n", buttons, mouse_wheel, mouse_h_wheel, pos[0], pos[1], displacement[0], displacement[1]);
	
	mouse = SDL_GetMouse();

	for(int i = 0; i < 3; i++){
		int ret = screenToMouseButton((buttons^previous) & (1 << i));
		if(ret>0) SDL_SendMouseButton(mouse->focus, mouse->mouseID, (buttons & (1 << i))?SDL_PRESSED:SDL_RELEASED, ret);
	}
		
	SDL_SendMouseMotion(mouse->focus, mouse->mouseID, 0, pos[0], pos[1]);
	previous = buttons;
}