#include "../../SDL_internal.h"
#include "../../joystick/SDL_joystick_c.h"
#include "../../joystick/SDL_sysjoystick.h"
#include "SDL.h"
#include "SDL_events.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"


/*===========  SDL Headers  =============*/
#include "sdl_qnx.h"
#include "errno.h"
#include "screen_consts.h"

#include "sys/usbdi.h"

//#ifdef SDL_JOYSTICK_QNX
#define SCREEN_MASK_LENGTH 20

// QNX-specific struct
typedef struct joystick_hwdata{
	unsigned last_buttons, current_buttons; //as screen buttons
	int	device; //SCREEN_PROPERTY_ID
	int analog0_x, analog0_y; //as screen outputs
	int analog1_x, analog1_y; //see above
	int a0x_up, a0y_up, a1x_up, a1y_up;
	struct joystick_hwdata* next;
	SDL_Joystick* attached; //back n forth attachment
} js_hwdata_t;

// Globals for our use.
js_hwdata_t* data_list = NULL;
SDL_JoystickID next_id = 0;
int has_init = 0;

Uint8 button_to_SDL(unsigned button){
	if( button & SCREEN_A_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_A;
	if( button & SCREEN_B_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_B;
	if( button & SCREEN_X_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_X;
	if( button & SCREEN_Y_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_Y;
	if( button & SCREEN_MENU1_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_START;
	if( button & SCREEN_MENU2_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_BACK;
	if( button & SCREEN_MENU3_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_GUIDE;
	if( button & SCREEN_L1_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
	if( button & SCREEN_L2_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
	if( button & SCREEN_L3_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_LEFTSTICK;
	if( button & SCREEN_R1_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
	if( button & SCREEN_R2_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
	if( button & SCREEN_R3_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
	if( button & SCREEN_DPAD_UP_GAME_BUTTON) return	SDL_CONTROLLER_BUTTON_DPAD_UP;
	if( button & SCREEN_DPAD_DOWN_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
	if( button & SCREEN_DPAD_LEFT_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
	if( button & SCREEN_DPAD_RIGHT_GAME_BUTTON) return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
	return 0xFF;
}

//##################################################################
//Helper functions

SDL_JoystickID deviceid_to_joystickid(int device){
	struct joystick_hwdata* next;

	next = data_list;
	while(next){
		if(next->device == device)
			return (next->attached)? next->attached->instance_id: -1;
		next = next->next;
	}
	return -1; 
}

struct joystick_hwdata* deviceid_to_hwdata(int device){
	struct joystick_hwdata * hwdata = NULL;

	hwdata = data_list;
	while(hwdata){
		if(hwdata->device == device) return hwdata;
		hwdata = hwdata->next;
	}

	return NULL;	
}

SDL_Joystick* deviceid_to_joystick(int device){
	struct joystick_hwdata* next;

	next = data_list;
	while(next){
		if(next->device == device)
			return (next->attached)? next->attached: NULL;
		next = next->next;
	}
	return -1;
}

SDL_Joystick* index_to_joystick(int index){
	struct joystick_hwdata* next;
	int check = 0;

	check = SDL_SYS_NumJoysticks();
	if(index > check) return NULL;

	while(next){
		if(index < 0) return NULL;
		if(index == 0) return (next->attached)? next->attached: NULL;
		index--;
		next = next->next;
	}

	return NULL;
}

struct joystick_hwdata* index_to_hwdata(int index){
	struct joystick_hwdata* next;
	int check = 0;

	check = SDL_SYS_NumJoysticks();
	if(index > check) return NULL;

	next = data_list;
	while(next != NULL){
		if(index < 0) return NULL;
		if(index == 0) return next;
		index--;
		next = next->next;
	}

	return NULL;
}

struct joystick_hwdata * alloc_hwdata(int dev){
	struct joystick_hwdata *ptr, *temp;
	int index = 0;

	ptr = malloc(sizeof(struct joystick_hwdata));
	ptr->device = dev;
	ptr->next = NULL;
	ptr->attached = NULL;

	ptr->current_buttons = 0;
	ptr->last_buttons = 0;

	ptr->analog0_x = 0;
	ptr->analog0_y = 0;
	ptr->analog1_x = 0;
	ptr->analog1_y = 0;

	ptr->a0x_up = 0;
	ptr->a0y_up = 0;
	ptr->a1x_up = 0;
	ptr->a1y_up = 0;

	if(data_list){
		temp = data_list;
		while(temp->next) temp = temp->next; index++;
		temp->next = ptr;
	}else{
		data_list = ptr;
	}
	SDL_PrivateJoystickAdded(index);

	return ptr;
}

int convert_to_SDL_sticksize(int convert, int screen_max){
	convert = convert - (screen_max/2); //centre
	screen_max = screen_max/2;
	while(screen_max > SDL_JOYSTICK_AXIS_MAX){
		screen_max = screen_max/2;
		convert = convert >> 1;
	}

	// if(screen_max < (SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN +1))
		//convert = (convert * (SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN +1))/screen_max; //scaleup
	// if(screen_max > (SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN +1))
	// 	convert = convert / (screen_max/(SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN +1));

	// if(convert < SDL_JOYSTICK_AXIS_MIN) convert =  SDL_JOYSTICK_AXIS_MIN;
	// if(convert > SDL_JOYSTICK_AXIS_MAX) convert =  SDL_JOYSTICK_AXIS_MAX;

	return convert;
}

/**
 * Handles screen joystick or gamepad.
 */
void handleJoystickEvent(screen_event_t event){
	struct joystick_hwdata * stick;
	unsigned screen_button_out=0;
	screen_device_t temp;
	int index, deviceid, size;
	int analog[3] = {0, 0, 0};

	if(has_init < 10){has_init++; return;}

	if(!event) return;

	screen_get_event_property_iv(event, SCREEN_PROPERTY_BUTTONS, &screen_button_out);
	if(screen_get_event_property_pv(event, SCREEN_PROPERTY_DEVICE, &temp)!=0) return;
	if(!temp) return;

	if(screen_get_device_property_iv(temp, SCREEN_PROPERTY_ID, &deviceid)) return;
	
	stick = deviceid_to_hwdata(deviceid);

	if(!stick)
		stick = alloc_hwdata(deviceid);
	if(!stick) return;

	stick->current_buttons = screen_button_out;

	screen_get_event_property_iv(event, SCREEN_PROPERTY_SIZE, &size);

	if(size > 0){
	if(screen_get_event_property_iv(event, SCREEN_PROPERTY_ANALOG0, analog)==0){
		stick->analog0_x = convert_to_SDL_sticksize(analog[0], size); stick->a0x_up = 1;
		stick->analog0_y = convert_to_SDL_sticksize(analog[1], size); stick->a0y_up = 1;
	}
	if(screen_get_event_property_iv(event, SCREEN_PROPERTY_ANALOG1, analog)==0){
		stick->analog1_x = convert_to_SDL_sticksize(analog[0], size); stick->a1x_up = 1;
		stick->analog1_y = convert_to_SDL_sticksize(analog[1], size); stick->a1y_up = 1;
	}
	}

	if(stick->attached){
		if(SDL_PrivateJoystickValid(stick->attached)){
			SDL_SYS_JoystickUpdate(stick->attached);
		}
	}
}


//###################################################################
void SDL_SYS_JoystickDetect(void){}

//##################################################################

// Detects for joysticks and returns a count.
int SDL_SYS_JoystickInit(void){
	//SDL_joystick_allows_background_events = SDL_TRUE;
	return SDL_SYS_NumJoysticks;
}

int SDL_SYS_NumJoysticks(void){
	struct joystick_hwdata* sticks = data_list;
	int count = 0;

	while(sticks){
		count++;
		sticks = sticks->next;
	}

	return count;
}

void SDL_SYS_JoystickQuit(void){
	//free all?
}

const char * SDL_SYS_JoystickNameForDeviceIndex(int device_index){
	return "qnx_joypad"; //TODO: Actual device name
}

SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index){
	SDL_Joystick* stick;
	stick = index_to_joystick(device_index);
	return (stick)? stick->instance_id: -1;
}

int SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index){
	struct joystick_hwdata* data;

	if(!joystick) return -1;
	if(!SDL_PrivateJoystickValid(joystick)) return 1;
	
	data = index_to_hwdata(device_index);

	if(data == NULL) return -1;
	if(data->attached != NULL) return -1;

	joystick->hwdata = data;
	data->attached = joystick;

	joystick->naxes = 4; //HARD CODED BY SCREEN //DISABLED FOR NOW TODO
	joystick->nballs = 0;
	joystick->nbuttons = 15; //SET BY SDL BUTTON MAX INDEX (0to14 = 15)
	return 0;
}

SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick){
	return joystick->hwdata? SDL_TRUE: SDL_FALSE;
}

SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID(int device_index){
	SDL_JoystickGUID toReturn;

	for (int i = 0; i < 16; i++) toReturn.data[i] = 0;

	return toReturn;
}

void SDL_SYS_JoystickClose(SDL_Joystick* joystick){
	struct joystick_hwdata* data = joystick->hwdata;
	joystick->hwdata = NULL;
	data->attached = NULL;
	SDL_PrivateJoystickRemoved(joystick->instance_id);
}

void SDL_SYS_JoystickUpdate(SDL_Joystick * joystick){
	int index;
	unsigned diff, release, press;

	if(!joystick) return;
	if(!(joystick->hwdata)) return;

	diff = joystick->hwdata->current_buttons ^ joystick->hwdata->last_buttons;
	release = joystick->hwdata->last_buttons & diff;
	press = joystick->hwdata->current_buttons & diff;

	if(joystick->hwdata->a0x_up == 1){
		joystick->hwdata->a0x_up = 0;
		SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, joystick->hwdata->analog0_x);
	}
	if(joystick->hwdata->a0y_up == 1){
		joystick->hwdata->a0y_up = 0;
		SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, joystick->hwdata->analog0_y);
	}
	if(joystick->hwdata->a1x_up == 1){
		joystick->hwdata->a1x_up = 0;
		SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, joystick->hwdata->analog1_x);
	}
	if(joystick->hwdata->a1y_up == 1){
		joystick->hwdata->a1y_up = 0;
		SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, joystick->hwdata->analog1_y);
	}

	for (int i = 0; i < SCREEN_MASK_LENGTH; i++){
		int button;
		button = button_to_SDL(1 << i);
		if(button == 0xFF) continue;
		if((release >> i) & 0b1){
			SDL_PrivateJoystickButton(joystick, button, SDL_RELEASED);
		}
		if((press >> i) & 0b1){
			SDL_PrivateJoystickButton(joystick, button, SDL_PRESSED);
			
		}
	}

	joystick->hwdata->last_buttons = joystick->hwdata->current_buttons;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick){
	return SDL_SYS_JoystickGetDeviceGUID(0); //temporary.
}

//#endif