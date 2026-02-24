#include <signal.h>

#include "SDL/SDL.h"
#include "SDL/SDL_getenv.h"

//
// GUIslice: simple SDL test in C (test-sdl.c)
// - Calvin Hass
// - https://www.impulseadventure.com/elec/
//
// This program is a simple test to ensure that the SDL
// library is installed correctly. If everything is
// working then you should see the external screen
// (such as PiTFT) display a red-to-blue gradient and
// a green region.
//

int bQuit = 0;
// Define the primary surface for display
SDL_Surface* scrMain = NULL;

void handle_sigint(int sig)
{
   bQuit = 1;
}


// Main entrypoint
int main(int argc, char* args[])
{

//signal(SIGINT, handle_sigint);
//   signal(SIGTERM, handle_sigint);
//   signal(SIGQUIT, handle_sigint);

        // --------------------------------------
        // Initialization
        // --------------------------------------

        // Update the environment variables for SDL to
        // work correctly with the external display on
        // LINUX frame buffer 1 (fb1).
        putenv((char*)"FRAMEBUFFER=/dev/fb0");
        putenv((char*)"SDL_FBDEV=/dev/fb0");

        // Initialize SDL
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
                fprintf(stderr,"ERROR in SDL_Init(): %s\n",SDL_GetError());
                return 0;
        }

        // Fetch the best video mode
        // - Note that the Raspberry Pi generally defaults
        //   to a 16bits/pixel framebuffer
        const SDL_VideoInfo* vInfo = SDL_GetVideoInfo();
        if (!vInfo) {
                fprintf(stderr,"ERROR in SDL_GetVideoInfo(): %s\n",SDL_GetError());
                return 0;
        }
        int     nResX = vInfo->current_w;
        int     nResY = vInfo->current_h;
        int     nDepth = vInfo->vfmt->BitsPerPixel;

        // Configure the video mode
        // - SDL_SWSURFACE appears to be most robust mode
        int     nFlags = SDL_SWSURFACE | SDL_FULLSCREEN;
        scrMain = SDL_SetVideoMode(nResX,nResY,nDepth,nFlags);
        if (scrMain == 0) {
                fprintf(stderr,"ERROR in SDL_SetVideoMode(): %s\n",SDL_GetError());
                return 0;
        }

int k = 0;
        while ( ! bQuit )
        {
          k++;
          if ( k > 50 ) k = 0;
        // --------------------------------------
        // Perform some simple drawing primitives
        // --------------------------------------

        // Draw a gradient from red to blue
        SDL_Rect        rectTmp;
        Uint32          nColTmp;
        for (Uint16 nPosX=0;nPosX<nResX;nPosX++) {
                rectTmp.x = nPosX+k;
                rectTmp.y = nResY/4+k;
                rectTmp.w = 1;
                rectTmp.h = nResY/4;
                nColTmp = SDL_MapRGB(scrMain->format,nPosX%256,0,255-(nPosX%256));
                SDL_FillRect(scrMain,&rectTmp,nColTmp);
        }

        // Draw a green box
        Uint32 nColGreen = SDL_MapRGB(scrMain->format,0,255,0);
        SDL_Rect rectBox = {0,k,nResX,nResY/4};
        SDL_FillRect(scrMain,&rectBox,nColGreen);


        // Now that we've completed drawing, update the main display
        SDL_Flip(scrMain);


        }
        // Close down SDL
        SDL_Quit();

        return 0;
}
