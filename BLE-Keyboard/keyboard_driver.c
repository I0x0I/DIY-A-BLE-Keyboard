/* Copyright (c) 2015 https://github.com/I0x0I
 *
 * The nordic license apply to the original code in this file.
 * The code that have been modified by I0x0I is under GPLv2 license.
 * 
 */
/* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include <stdint.h>
#include <stdbool.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "keyboard_map.h"
#include "keyboard_driver.h"

#define MODIFIER_HID_START 0xE0
#define MODIFIER_HID_END   0xE7

static uint8_t tmp_key_array[2][MAX_NUM_OF_PRESSED_KEYS];	//!< Array holding the keys that have already been transmitted and currently pressed.

static uint8_t *transmitted_keys = tmp_key_array[0];		//!These pointers deside which part of the tmp_key_array  
static uint8_t *currently_pressed_keys = tmp_key_array[1];	//!saves the current keys and which saves the transmitted one
static uint8_t transmitted_keys_num;		//!< Number of keys in transmitted_keys
static uint8_t currently_pressed_keys_num;	//!< Number of keys in currently_pressed_keys

static uint8_t key_packet[KEY_PACKET_SIZE]; //!< Stores last created key packet. One byte is used for modifier keys, one for OEMs. Key values are USB HID keycodes.
											//!See the HID Usage Tables for more detail.
static uint32_t input_scan_vector;			//!A trick to quickly detecte if any keys is pressed
static const uint8_t *matrix_lookup; 		//!< Pointer to the key lookup matrix in use

static bool have_keys_changed(void);
static bool keymatrix_read(uint8_t *pressed_keys, uint8_t *number_of_pressed_keys);
static void keypacket_addkey(uint8_t key);
static void keypacket_create(uint8_t *key_packet, uint8_t key_packet_size);
static void remap_fn_keys(uint8_t *keys, uint8_t number_of_keys);

bool keyboard_init(void)
{
	input_scan_vector = 0;
	if (row_pin_array == 0 || column_pin_array == 0){
		return false;	//return if pins have not been define
    }else{
		for (uint_fast8_t i = KEYBOARD_NUM_OF_COLUMNS; i--;){
			nrf_gpio_cfg_output((uint32_t)column_pin_array[i]);
			NRF_GPIO->PIN_CNF[(uint32_t)column_pin_array[i]] |= 0x400;	//Set pin to be "Disconnected 0 and standard 1"
			nrf_gpio_pin_clear((uint32_t)column_pin_array[i]);	//Set pin to low
		}
		for (uint_fast8_t i = KEYBOARD_NUM_OF_ROWS; i--;){
			nrf_gpio_cfg_input((uint32_t)row_pin_array[i],NRF_GPIO_PIN_PULLDOWN);
			input_scan_vector |= (1U << row_pin_array[i]);	//Prepare the magic number
		}
        if (((NRF_GPIO->IN)&input_scan_vector) != 0){	//Detect if any input pin is high
            return false;	//If inputs are not all low while output are, there must be something wrong
        }else{
			transmitted_keys_num = 0;	//Clear the arrays
			currently_pressed_keys_num = 0;
            for (uint_fast8_t i = MAX_NUM_OF_PRESSED_KEYS; i--;){
				transmitted_keys[i] = 0;
                currently_pressed_keys[i] = 0;
            }
        }
        matrix_lookup = default_matrix_lookup;
    }
    return true;
}

bool keyboard_new_packet(const uint8_t **p_key_packet, uint8_t *p_key_packet_size)
{
		uint8_t *p_tmp;
    bool is_new_packet;
	
	p_tmp = currently_pressed_keys;		//Save the currently pressed keys to transmitted keys by swapping two pointers
	currently_pressed_keys = transmitted_keys;
	transmitted_keys = p_tmp;
	transmitted_keys_num = currently_pressed_keys_num;	//Save the number of currently pressed keys

    if (keymatrix_read(currently_pressed_keys, &currently_pressed_keys_num)){	//Scan the key matrix, save the result in currently_pressed_keys
		if (have_keys_changed()){	//Some keys have been pressed, check if there is key changes
			keypacket_create(&key_packet[0], KEY_PACKET_SIZE);
			*p_key_packet       = &key_packet[0];
			*p_key_packet_size  = KEY_PACKET_SIZE;
			is_new_packet = true;
		}else{
            is_new_packet = false;	//No need to create a new packet if there is no changes
        }
    }
    else{
        is_new_packet = false;		//No key is pressed or ghosting detected. Don't create a packet.
    }
    return is_new_packet;
}

static bool keymatrix_read(uint8_t *pressed_keys, uint8_t *number_of_pressed_keys)
{
    uint_fast8_t row_state[KEYBOARD_NUM_OF_COLUMNS];
    uint_fast8_t blocking_mask = 0;
    *number_of_pressed_keys = 0;

    for (uint_fast8_t column =  KEYBOARD_NUM_OF_COLUMNS; column--;){	//Loop through each column
		nrf_gpio_pin_set((uint32_t) column_pin_array[column]);
        if (((NRF_GPIO->IN)&input_scan_vector) != 0){					//If any input is high
			uint_fast8_t pin_state;
			row_state[column] = 0;
            uint_fast8_t detected_keypresses_on_column = 0;
            for (uint_fast8_t row = KEYBOARD_NUM_OF_ROWS; row--;){		//Loop through each row
				pin_state = nrf_gpio_pin_read((uint32_t) row_pin_array[row]);
				row_state[column] |= (pin_state << row);				//Record pin state
                if (pin_state){											//If pin is high
                    if (*number_of_pressed_keys < MAX_NUM_OF_PRESSED_KEYS){
                        *pressed_keys = matrix_lookup[row * KEYBOARD_NUM_OF_COLUMNS + column];
                        if(*pressed_keys != 0){							//Record the pressed key if it's not 0
							pressed_keys++;
							(*number_of_pressed_keys)++;
						}
                    }
                    detected_keypresses_on_column++;
                }
            }
            if (detected_keypresses_on_column > 1){
                if (blocking_mask & row_state[column]){					//If there is ghosting
                    return false;
                }
            }
            blocking_mask |= row_state[column];
        }
		nrf_gpio_pin_clear((uint32_t) column_pin_array[column]);
    }
    return true;
}

static void keypacket_create(uint8_t *key_packet, uint8_t key_packet_size)
{
	bool fn_key_is_set = false;
	
    for (uint_fast8_t i = KEY_PACKET_KEY_INDEX; i < key_packet_size; i++){
        key_packet[i] = KEY_PACKET_NO_KEY;	    // Clear key_packet contents
    }
    key_packet[KEY_PACKET_MODIFIER_KEY_INDEX] = 0;
    key_packet[KEY_PACKET_RESERVED_INDEX]     = 0;

    for (uint_fast8_t i = 0; i < currently_pressed_keys_num; i++){	//Loop through the currently pressed keys array
        if (currently_pressed_keys[i] == 0xFF) {					//If Fn is pressed,
            fn_key_is_set = true;
        }else if (currently_pressed_keys[i] >= MODIFIER_HID_START && currently_pressed_keys[i] <= MODIFIER_HID_END) {
			//Detect and set modifier key, see HID Usage Tables for more detail
            key_packet[KEY_PACKET_MODIFIER_KEY_INDEX] |= (uint8_t)(1U << (currently_pressed_keys[i] - MODIFIER_HID_START));
        }
        else if (currently_pressed_keys[i] != 0){
            keypacket_addkey(currently_pressed_keys[i]);			//Add keys to the packsge 
        }
    }
    if (fn_key_is_set){
        remap_fn_keys(&key_packet[0], KEY_PACKET_MAX_KEYS);
    }
}

static void keypacket_addkey(uint8_t key){
    for (uint_fast8_t i = KEY_PACKET_KEY_INDEX; i < KEY_PACKET_SIZE; i++){
        if (key_packet[i] == key){	//If the key is already in the packet
            return;
        }
    }
    for (uint_fast8_t i = KEY_PACKET_KEY_INDEX; i < KEY_PACKET_SIZE; i++){
        if (key_packet[i] == KEY_PACKET_NO_KEY){
            key_packet[i] = key;
            return;
        }
    }
}

static bool have_keys_changed(void){
    if (currently_pressed_keys_num != transmitted_keys_num){
        return true;
    }else{
        for (uint_fast8_t i = currently_pressed_keys_num; i--;){
            if (currently_pressed_keys[i] != transmitted_keys[i]){
                return true;
            }
        }
    }
    return false;
}

static void remap_fn_keys(uint8_t *keys, uint8_t number_of_keys){
    for (uint_fast8_t i = KEY_PACKET_KEY_INDEX; i < number_of_keys; i++){
        switch (keys[i]){
            case 0x3A:
                keys[i] = 0x7F; break;	//F1 -> Mute
            case 0x3B:	
				keys[i] = 0x81; break;	//F2 -> VolumnDown
            case 0x3C:          
                keys[i] = 0x80; break;	//F3 -> VolumnUp
            case 0x3D:         
                keys[i] = 0x48; break; 	//F4 -> Pause
            case 0x52:         
                keys[i] = 0x4B; break;	//Up -> PageUp
            case 0x51:          
                keys[i] = 0x4E; break;	//Down -> PageDown
            case 0x50:         
                keys[i] = 0x4A; break; 	//Left -> Home
            case 0x4F:        
                keys[i] = 0x4D; break;	//Right -> End
            default: break;
        }
    }
}
