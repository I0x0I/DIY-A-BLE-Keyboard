#DIY A BLE Keyboard

##Introduction
This is a simple open source BLE keyboard firmware for the nRF51 bluetooth chip.

##How to use
* Follow the [tutorials](https://devzone.nordicsemi.com/tutorials/) to set up development environment
and test the chip.
* Download the [nRF51 SDK](https://developer.nordicsemi.com/) and 
the [S110 SoftDevice](http://www.nordicsemi.com/eng/Products/Bluetooth-Smart-Bluetooth-low-energy/nRF51822)
. In this case the nRF51 SDK v9.0 and a 51822 module with S110 v8 are used.
* Find the *ble_app_hids_keyboard* floder in the SDK, put the *.c* files in this project 
in the root directory and the *.h* files in the *config* floder 
* If you want to use the nRF51 chip as both a keyboard controller and a BLE controller, 
you need to connect the keyboard matrix to it. After doing that, set variables of your 
keyboard in *keyboard_map.h* and compile the project. After flashing, you should be able
to find a *Nrodic Keyboard* device and that is your BLE keyboard. The lookup matrix uses 
HID key code defined in
[Simplified USB HID keycode Table](http://www.mindrunway.ru/IgorPlHex/USBKeyScan.pdf)
or [USB HID to PS/2 Scan Code Translation Table](http://www.hiemalis.org/~keiji/PC/scancode-translate.pdf)
* If you only want to use the nRF51 chip as a BLE controller, beside setting the variables, 
you would need to write some code for the nRF51 to communicate with the keyboard controller,
examples can be find in the SDK and more detail information is in
[here](http://infocenter.nordicsemi.com/index.jsp)

##How does it work
These codes originated from the hid_keyboard example and the cherry example, a timer is 
setted to scan the keyboard every 25ms. If there are key presses, it will create a report 
packet in HID keyboard format and send it to the host, when all keys are released, a packet
filled with 0 will be sent. If a key is hold, no packet will be sent until it's release, 
the host can recongnize that the key is holding as it does not recive a released packet
(full 0). Doing so is also power saving as there's no need to send packet continously when
keys are holded, know more about the HID keyboard packet format in
[Device Class Definition for Human Interface Devices (HID)](http://www.usb.org/developers/hidpage/HID1_11.pdf)
 and  [HID Usage Table](http://www.usb.org/developers/hidpage/Hut1_12v2.pdf). Codes on 
 the battery service has been removed and the whole frame work has been simplified.
 
##Future work
 * Add support to the sleep mode
 * Add support to touchpad
