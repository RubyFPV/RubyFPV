#include "../base/shared_mem.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"

#include <time.h>
#include <sys/resource.h>

#include "../renderer/render_engine.h"

bool bQuit = false;

void handle_sigint(int sig)
{
   log_line("Caught signal to stop: %d\n", sig);
   bQuit = true;
}

int main(int argc, char *argv[])
{
   signal(SIGINT, handle_sigint);
   signal(SIGTERM, handle_sigint);
   signal(SIGQUIT, handle_sigint);

   log_init("TestUI");
   log_enable_stdout();
   log_line("\nStarted.\n");

   RenderEngine* g_pRenderEngine = render_init_engine();

   u32 idFontMenu = g_pRenderEngine->loadFont("res/f1reg.ttf");
   int x = 0;
   int y = 0;
   while (! bQuit )
   {
      hardware_sleep_ms(100);
      g_pRenderEngine->startFrame();
      g_pRenderEngine->setFill(255,255,255,1);
      g_pRenderEngine->setStrokeWidth(0);
      g_pRenderEngine->drawRect((float)x/g_pRenderEngine->getScreenWidth(), 0.1, 0.1, 0.1);
      g_pRenderEngine->endFrame();

      x++;
      if ( x > 500 ) x = 0;
   }
   log_line("\nEnded\n");
   exit(0);
}
