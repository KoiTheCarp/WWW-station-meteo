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
#include "board_pins.h"

/* Diagnostic temporaire du montage : mettre à 0 une fois la LED et les boutons vérifiés */
#define DIAGNOSTIC 1

#if DIAGNOSTIC
static void Diagnostic(void)
{
    int ancien1 = -1;
    int ancien2 = -1;

    /* Test LED : chaque couleur demandée pendant 1 s */
    printf("DIAG LED : rouge demande\r\n");
    GroveRGB_SetColor(255, 0, 0);
    HAL_Delay(1000);
    printf("DIAG LED : vert demande\r\n");
    GroveRGB_SetColor(0, 255, 0);
    HAL_Delay(1000);
    printf("DIAG LED : bleu demande\r\n");
    GroveRGB_SetColor(0, 0, 255);
    HAL_Delay(1000);

    /* Test boutons : on affiche l'état brut des broches quand il change */
    printf("DIAG boutons : appuie sur chaque bouton (RESET pour quitter)\r\n");
    while (1)
    {
        int etat1 = (HAL_GPIO_ReadPin(BUTTON1_PORT, BUTTON1_PIN) == GPIO_PIN_SET);
        int etat2 = (HAL_GPIO_ReadPin(BUTTON2_PORT, BUTTON2_PIN) == GPIO_PIN_SET);

        if ((etat1 != ancien1) || (etat2 != ancien2))
        {
            printf("DIAG BTN1 (PA10) = %d   BTN2 (PB3) = %d\r\n", etat1, etat2);
            ancien1 = etat1;
            ancien2 = etat2;
        }
        HAL_Delay(20);
    }
}
#endif

int main(void)
{
    Global_Init();
    GroveRGB_Init();
    Boutons_Init();

#if DIAGNOSTIC
    Diagnostic();
#endif

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