#include "../base/base.h"
#include "../base/config.h"
#include "../base/hardware.h"
#include "../base/hardware_procs.h"
#include "../base/models.h"
#include "../base/ctrl_interfaces.h"
#include "../base/ctrl_settings.h"

Model* g_pModelVehicle = NULL;

int main(int argc, char *argv[])
{
   log_enable_stdout();
   log_init("TestModelLoad");
   log_enable_stdout();

   if ( argc <2 )
   {
      log_line("Please provide the model file name to load.");
      exit(0);
   }



   g_pModelVehicle = new Model();
   if ( ! g_pModelVehicle->loadFromFile(argv[argc-1]) )
   {
      log_softerror_and_alarm("Can't load current model vehicle.");
      g_pModelVehicle = NULL;
   }
   else
      log_line("Model loaded correctly.");
   return (0);
}