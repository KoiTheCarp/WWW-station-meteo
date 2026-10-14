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
#include "boutons.h"
#include "modes.h"

int main(void)
{
    Global_Init();
    GroveRGB_Init();
    Boutons_Init();

    printf("WWW : demarrage\r\n");

    /* Diagramme d'activité : bouton rouge enfoncé au démarrage -> configuration, sinon standard */
    Modes_Demarrer();

    while (1)
    {
        /* Ne jamais bloquer ici : la boucle doit tourner vite pour voir les appuis de 5 s */
        Modes_MettreAJour();

        /* Palier 1 : la mesure toutes les LOG_INTERVAL viendra ici */
    }
}