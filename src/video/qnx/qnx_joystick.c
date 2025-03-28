#include "../../SDL_internal.h"
#include "../../joystick/SDL_joystick_c.h"
#include "../../joystick/SDL_sysjoystick.h"
#include "SDL_events.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"


/*===========  SDL Headers  =============*/
#include "sdl_qnx.h"
#include "errno.h"
#include "screen_consts.h"

#ifdef SDL_JOYSTICK_QNX

typedef struct _device_storage{
	screen_device_t device;
	SDL_Joystick * joystick;
	unsigned 		last_buttons, current_buttons; //as screen buttons
	
	int num_buttons, num_axis; //needed?

	int analog0_x, analog0_y; //as screen outputs
	int analog1_x, analog1_y; //see above	
} device_storage;

#define NUM_DEVICES 8

device_storage devices[NUM_DEVICES];

static int button_to_SDL[] = {
	[SCREEN_A_GAME_BUTTON]=			SDL_CONTROLLER_BUTTON_A,
	[SCREEN_B_GAME_BUTTON]=			SDL_CONTROLLER_BUTTON_B,
	[SCREEN_X_GAME_BUTTON]=			SDL_CONTROLLER_BUTTON_X,
	[SCREEN_Y_GAME_BUTTON]=			SDL_CONTROLLER_BUTTON_Y,
	[SCREEN_MENU1_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_START,
	[SCREEN_MENU2_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_BACK,
	[SCREEN_MENU3_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_GUIDE,
	[SCREEN_L1_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
	[SCREEN_L2_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
	[SCREEN_L3_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_LEFTSTICK,
	[SCREEN_R1_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
	[SCREEN_R2_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
	[SCREEN_R3_GAME_BUTTON]=		SDL_CONTROLLER_BUTTON_RIGHTSTICK,
	[SCREEN_DPAD_UP_GAME_BUTTON]=	SDL_CONTROLLER_BUTTON_DPAD_UP,
	[SCREEN_DPAD_DOWN_GAME_BUTTON]=	SDL_CONTROLLER_BUTTON_DPAD_DOWN,
	[SCREEN_DPAD_LEFT_GAME_BUTTON]=	SDL_CONTROLLER_BUTTON_DPAD_LEFT,
	[SCREEN_DPAD_RIGHT_GAME_BUTTON]=SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};

//##################################################################
//Helper functions

int get_device_index(screen_device_t device){
	printf("get_device_index\n");
	for(int i = 0; i < NUM_DEVICES; i++){
		if(devices[i].device == device) 
		return i;
	}
	return -1;
}

int get_joystick_index(SDL_Joystick * joystick){
	printf("get_joystick_index\n");
	for (int i = 0; i < NUM_DEVICES; i++){
		if (devices[i].joystick == joystick) return i;
	}
	return -1;
}

int insertScreenDevice(screen_device_t device){
	int index;
	printf("insertScreenDevice\n");
	index = get_device_index(NULL);


	if(index != -1) devices[index].device = device;
	return index;
}

/**
 * Handles screen joystick or gamepad.
 */
void handleJoystickEvent(screen_event_t event){
	unsigned screen_button_out=0;
	int index;
	screen_device_t temp;

	printf("handleJoystickEvent\n");

	screen_get_event_property_iv(event, SCREEN_PROPERTY_BUTTONS, &screen_button_out);

	//logic here.
	//find relevant device, and update its button mapping
	if(screen_get_event_property_pv(event, SCREEN_PROPERTY_DEVICE, &temp)!=0) return;
	
	index = get_device_index(temp);
	if(index == -1) index = insertScreenDevice(temp);
	if(index == -1) return;

	devices[index].current_buttons = screen_button_out;

	//printf("Num Joysticks: %d\n", SDL_SYS_NumJoysticks());
}


//###################################################################
void SDL_SYS_JoystickDetect(void){}

//##################################################################

// Detects for joysticks and returns a count.
int SDL_SYS_JoystickInit(void){
	int device_count = 0;
	int joystick_count = 0;

	printf("SDL_SYS_JoystickInit\n");

	if(screen_get_context_property_iv(context, SCREEN_PROPERTY_DEVICE_COUNT, &device_count)){ 
		printf("Error can't query for device count w errno %d\n", errno);
		printf("context val %d \n", context);
		return -1;
	}

	printf("device count: %d\n", device_count);

	// Initialization
	for (int i = 0; i < NUM_DEVICES; i++){ 
		devices[i].device= NULL; 
		devices[i].joystick= NULL;
		devices[i].num_buttons=0;
		devices[i].num_axis=0;
		devices[i].analog0_x=0;
		devices[i].analog0_y=0;
		devices[i].analog1_x=0;
		devices[i].analog1_y=0;
		devices[i].last_buttons=0;
		devices[i].current_buttons=0;
		}
	
	/*
	if(device_count>0){
		screen_device_t* temp = calloc(device_count, sizeof(screen_device_t));
		if(screen_get_context_property_pv(context, SCREEN_PROPERTY_DEVICES, &temp)){
			printf("Failed to read in ")
		}else{ 
			for (int i = 0; i < device_count; i++){
				int type = 0;
				printf("iteration %d\n", i);
				screen_get_device_property_iv(temp[i], SCREEN_PROPERTY_TYPE, &type);
				if(type == SCREEN_EVENT_GAMEPAD || type == SCREEN_EVENT_JOYSTICK){
					if (insertScreenDevice(temp[i]) >0) joystick_count++;
				}
			}
		}
		//free(temp);
	}*/

	printf("Device Count %d Joystick Count %d \n", device_count, joystick_count);

	return device_count;
}

int SDL_SYS_NumJoysticks(void){
	int count = 0;
	printf("SDL_SYS_NumJoysticks\n");

	return NUM_DEVICES; //debug

	for (int i = 0; i < NUM_DEVICES; i++) count += devices[i].device != NULL;
	return count;
}

void SDL_SYS_JoystickQuit(void){
	printf("SDL_SYS_JoystickQuit\n");
	//nothing dynamically allocated needs to be freed. should be all good all around!
}

const char * SDL_SYS_JoystickNameForDeviceIndex(int device_index){
	printf("SDL_SYS_JoystickNameForDeviceIndex\n");
	return "qnx_placeholder";
}

SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index){
	printf("SDL_SYS_GetInstanceIdOfDeviceIndex\n");
	return (devices[device_index].joystick->instance_id);
}

int SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index){
	printf("SDL_SYS_JoystickOpen\n");

    if (devices[device_index].joystick != NULL) {
        return SDL_SetError("Joystick already opened");
    }

	devices[device_index].joystick = joystick;

	joystick->nbuttons = devices[device_index].num_buttons;
	joystick->naxes = devices[device_index].num_axis;
}

SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick){
	printf("SDL_SYS_JoystickAttached\n");
	for (int i = 0; i< NUM_DEVICES; i++){if(devices[i].joystick == joystick) return SDL_TRUE;}
	return SDL_FALSE;
}

SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID(int device_index){
	SDL_JoystickGUID toReturn;

	printf("SDL_SYS_JoystickGetDeviceGUID\n");

	for (int i = 0; i < 16; i++) toReturn.data[i] = 0;

	if(device_index >= 0 && device_index < NUM_DEVICES){
		if(devices[device_index].device){
			int pid=0, vid=0;
			
			screen_get_device_property_iv(devices[device_index].device, SCREEN_PROPERTY_VENDOR, &vid); 
			screen_get_device_property_iv(devices[device_index].device, SCREEN_PROPERTY_PRODUCT, &pid);

			// GUID: [00 00 00 00 00 00 00 00 VV VV VV VV PP PP PP PP]
			toReturn.data[8] = vid >> 6 & 0xFF;
			toReturn.data[9] = vid >> 4 & 0xFF;
			toReturn.data[10]= vid >> 2 & 0xFF;
			toReturn.data[11]= vid 		& 0xFF;
			toReturn.data[12]= pid >> 6 & 0xFF;
			toReturn.data[13]= pid >> 4 & 0xFF;
			toReturn.data[14]= pid >> 2 & 0xFF;
			toReturn.data[15]= pid 		& 0xFF;
		}
	}

	return toReturn;
}

void SDL_SYS_JoystickClose(SDL_Joystick* joystick){
	int index;

	printf("SDL_SYS_JoystickClose\n");
	index = get_joystick_index(joystick);
	if(index < 0 || index > NUM_DEVICES) return;

	devices[index].joystick = NULL;
}

void SDL_SYS_JoystickUpdate(SDL_Joystick * joystick){
	int index;
	unsigned diff, release, press;

	printf("SDL_SYS_JoystickUpdate\n");

	index = get_joystick_index(joystick);
	if(index<0 || index > NUM_DEVICES) return;

	diff = devices[index].current_buttons ^ devices[index].last_buttons;
	release = devices[index].last_buttons & diff;
	press = devices[index].current_buttons & diff;

	for (int i = 0; i < sizeof(unsigned); i++){
		if((release >> i) & 0b1)
			SDL_PrivateJoystickButton(joystick, button_to_SDL[1 << i], SDL_RELEASED);
		if((press >> i) & 0b1)
			SDL_PrivateJoystickButton(joystick, button_to_SDL[1 << i], SDL_PRESSED);
	}

	devices[index].last_buttons = devices[index].current_buttons;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick){
	int device_index;

	printf("SDL_SYS_JoystickGetGUID\n");
	
	device_index = get_joystick_index(joystick);
	return SDL_SYS_JoystickGetDeviceGUID(device_index);
}

#endif