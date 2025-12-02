#include <SDL2/SDL.h>

// This program opens a joystick and tells you
// when a button is pressed or an axis is moved.
// This demsontrates how to read from the joystick
// using an event-based system. In another example
// I will show how to poll the state of each button.
int main()
{ 
    // Initialize the joystick subsystem for SDL2
    int joysticks = SDL_Init(SDL_INIT_JOYSTICK);

    // If there was an error setting up the joystick subsystem, quit.
    if (joysticks < 0) {
        printf("Unable to initialize the joystick subsystem.\n");
        return -1;
    }

    // Check how many joysticks are connected.
    joysticks = SDL_NumJoysticks();
    printf("There are %d joysticks connected.\n", joysticks);

    SDL_JoystickEventState(SDL_ENABLE);

    SDL_Joystick *js = NULL;
    // If there are joysticks connected, open one up for reading
    if (joysticks > 0)
    {

        js = SDL_JoystickOpen(0);
        if (js == NULL) {
            printf("There was an error reading from the joystick.\n");
            return -1;
        }
    }
    // If there are no joysticks connected, exit the program.
    else {
        printf("There are no joysticks connected. Exiting...\n");
        return -1;
    }

    int js_axes = 0;
    int js_hats = 0;
    int js_buttons = 0;
    js_axes = SDL_JoystickNumAxes(js);
    js_hats = SDL_JoystickNumHats(js);
    js_buttons = SDL_JoystickNumButtons(js);
    const char *name = SDL_JoystickName(js);
    printf("Joystick connected: %s\n", name ? name : "unknown");
    printf("Joystick axes: %d, hats: %d, buttons: %d\n", js_axes, js_hats, js_buttons);

    int quit = 0;
    SDL_Event event;

    // An infinite loop that keeps going until we set
    // the quit variable to a non-zero value. We
    // put this loop so that we can keep on listening to
    // the joystick until we are done with it.
    while (!quit) {
        // The event variable stores a list of events.
        // The inner loop keeps reading the events
        // one-by-one until there are no events left
        // to read. SDL_PollEvent(&event) just checks
        // if any new events have happend, and stores them
        // inside of the event variable.
     
        static int kkk = 0;
        kkk++;
        if ( (kkk % 10000) == 0 )
        {
        int i1 = SDL_JoystickGetAxis(js, 0);
        int i2 = SDL_JoystickGetAxis(js, 1);
        int i3 = SDL_JoystickGetAxis(js, 2);
        int i4 = SDL_JoystickGetAxis(js, 3);
        int i5 = SDL_JoystickGetButton(js, 0);
        int i6 = SDL_JoystickGetButton(js, 1);
        int i7 = SDL_JoystickGetButton(js, 2);
        int i8 = SDL_JoystickGetButton(js, 3);
        printf("%d %d %d %d %d %d %d %d\n", i1, i2, i3, i4, i5, i6, i7, i8);
        }
        while (SDL_PollEvent(&event) != 0) {
            // A switch statement conditionally runs different
            // code depending on the value it is given.
            // Here we check the type of event that happened,
            // and do something different depending on what type of
            // event it was.
            switch (event.type) {
            case SDL_QUIT:
                quit = 1;
                break;
            
            case SDL_JOYAXISMOTION:
                printf("The value of axis %d was changed to %d.\n", event.jaxis.axis, event.jaxis.value);
                break;

            case SDL_JOYHATMOTION:
                printf("The hat with index %d was moved to position %d.\n", event.jhat.hat, event.jhat.value);
                break;

            case SDL_JOYBUTTONDOWN:
                printf("The button with index %d was pressed.\n", event.jbutton.button);
                break;

            case SDL_JOYBUTTONUP:
                printf("The button with index %d was released.\n", event.jbutton.button);
                break;

            case SDL_JOYDEVICEADDED:
                printf("A Joystick was connected and assigned index %d.\n", event.jdevice.which);
                break;

            case SDL_JOYDEVICEREMOVED:
                printf("The Joystick with index %d was removed.\n", event.jdevice.which);
                break;
            }
        }
    }

    // Free up any resources that SDL allocated.
    SDL_Quit();
    return 0;
}
