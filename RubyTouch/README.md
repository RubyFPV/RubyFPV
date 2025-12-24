### README
1. apt install libevdev-dev pigpio dtach
2. compile the code:
   g++ -std=c++11 -o touch_handler touch_handler.cpp -lpigpio -lpthread
3. Add the below to the /root/.profile:
   dtach -n /dev/shm/touch_handler.dtach /root/devel/touch_handler

### NOTE
1. If there is a keyboard plugged in, the event id will be changed. 
2. Below is the illustration of the touch screen become virtual buttons:<pre>
   +-----+-----+----+
   | ESC | NIL | UP |
   +-----+-----+----+
   | QA1 | QA2 | OK |
   +-----+-----+----+
   | QA3 | NIL | DN |
   +-----+-----+----+</pre>
3. Basically the code converts the touch event and trigger it into GPIO signals
   for the Pi to gets the button pressed as if GPIO trigger. I tried to use
   virtual keyboard signals but it sent to different layer. So, temporay put
   to GPIO mode first.
4. Make sure the main display DO NOT FLICKER. Once flicker it will change the
   event input id from 0 to 1 and vice versa. When display flickers usually
   the power is not enough to drive the display. Always runs the display from
   the main power source. Try NOT to power up the display from the Raspberry
   Pi USB port.

### COMPILE
g++ -std=c++11 touch_handler.cpp -o touch_handler

