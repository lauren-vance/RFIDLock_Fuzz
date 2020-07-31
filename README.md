# RFIDLock_Fuzz

This project was created to practice fuzzing on embedded systems. The program is written in C for the ST Nucleo F429ZI (STM32F429ZIT6 MCU). It controls a motion sensor, RFID reader, servo motor, and LCD screen to function as a "lock" mechanism, which is protected by RFID Keycards. The keycards are registered or deleted using the Python client program, which communicates with the hardware via TCP protocol. The "main.c" file was written by me in mbed online IDE; the additional libraries were not written by me but were also found and imported from here. The Python 3 file was also written by me. 
The applications of this project are mostly hypothetical, but were programmed to handle some real scenarios as much as possible. The main purpose of creating this program was to learn fuzzing techniques and challenges on embedded systems by planting bugs in the original program and attempting to find them using boofuzz. Due to the nature of embedded systems, faults and crashes are harder to spot immediately and can lead to latent effects which are difficult for fuzzing to handle. In summary, this project was mostly used as a learning tool not only for fuzzing applications but as a first introduction to Ethernet and TCP protocol. Therefore, the code could be cleaner and more efficient but a lot of changes were made as it was developed. If I were starting over with the knowledge I've gained from this exercise, I would have a much better understanding of how to structure the code to be cleaner and more efficient.

## Getting Started

List of Equipment Used:
- ST Nucleo F429ZI
- PIR Motion Sensor
- RC522 RFID Sensor with 2 Keys
- SG90 Servo Motor (the "lock")
- 16x2 LCD
- 1x green LED, 1x red LED

When motion is detected, the LCD prompts the user to scan their RFID key; if it is a match, the green LED illuminates and the servo motor "unlocks," otherwise the red LED blinks and an access denied message is displayed.
The keys are programmed using the Python client program, which sends commands to the server via TCP to get various information or add/delete keys. To add a key, the key must be scanned with the system's RFID reader. To delete a key, the corresponding ID must be selected from the list of accepted keys (no scan required). 
The list of keys is maintained on both the client and server side, so that if either loses power the list can be restored. The client requests an updated list whenever it is first started, and continuously checks the list stored on the server side via a separate thread in order to ensure they are the same. If they do not match, the client sends its list of keys to the server to be updated (in case of reset or power loss).
