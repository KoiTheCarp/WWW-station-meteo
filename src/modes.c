#include <stdio.h>
#include "stm32l4xx_hal.h"
#include "modes.h"
#include "boutons.h"
#include "grove_rgb.h"
#include "parametres.h"

#if MESURES_TEST
#define CONFIG_INACTIVITE_MS  (20U * 1000U)
#else
#define CONFIG_INACTIVITE_MS  (30U * 60U * 1000U)
#endif

static Mode mode_courant = MODE_STANDARD;

static uint32_t derniere_activite_config_ms = 0U;

static void Modes_Changer(Mode nouveau)
{
    mode_courant = nouveau;

    switch (nouveau)
    {
        case MODE_STANDARD:
            GroveRGB_SetColor(0, 255, 0);
            printf("Mode STANDARD\r\n");
            break;

        case MODE_CONFIGURATION:
            GroveRGB_SetColor(255, 255, 0);
            printf("Mode CONFIGURATION\r\n");
            break;

        case MODE_ECONOMIQUE:
            GroveRGB_SetColor(0, 0, 255);
            printf("Mode ECONOMIQUE\r\n");
            break;

        case MODE_MAINTENANCE:
            GroveRGB_SetColor(255, 80, 0);
            printf("Mode MAINTENANCE\r\n");
            break;
    }
}

void Modes_Demarrer(void)
{
    if (Boutons_RougeEnfonce())
    {
        derniere_activite_config_ms = HAL_GetTick();
        Modes_Changer(MODE_CONFIGURATION);
    }
    else
    {
        Modes_Changer(MODE_STANDARD);
    }
}

void Modes_MettreAJour(void)
{
    bool rouge = Boutons_RougeAppuiLong();
    bool vert = Boutons_VertAppuiLong();

    switch (mode_courant)
    {
        case MODE_STANDARD:
            if (rouge)
            {
                Modes_Changer(MODE_MAINTENANCE);
            }
            else if (vert)
            {
                Modes_Changer(MODE_ECONOMIQUE);
            }
            break;

        case MODE_ECONOMIQUE:
            if (rouge)
            {
                Modes_Changer(MODE_STANDARD);
            }
            break;

        case MODE_MAINTENANCE:
            if (rouge)
            {
                Modes_Changer(MODE_STANDARD);
            }
            break;

        case MODE_CONFIGURATION:
            if ((HAL_GetTick() - derniere_activite_config_ms) >= CONFIG_INACTIVITE_MS)
            {
                Modes_Changer(MODE_STANDARD);
            }
            break;
    }
}

Mode Modes_Courant(void)
{
    return mode_courant;
}

void Modes_SignalerActivite(void)
{
    derniere_activite_config_ms = HAL_GetTick();
}
