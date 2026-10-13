#include <stdio.h>
#include "setup.h"
#include "cesi_types.h"
#include "grove_rgb.h"
#include "grove_button.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "grove_bme680.h"
#include "sd_logger.h"

int main(void)
{
    Global_Init();

    /* Palier 0 : vérifier que la chaîne compilation -> téléversement marche */
    GroveRGB_Init();
    GroveRGB_SetColor(0, 255, 0);
    printf("WWW : demarrage OK\r\n");

    while (1)
    {

    }
}