#include <stdio.h>
#include "stm32l4xx_hal.h"
#include "modes.h"
#include "boutons.h"
#include "grove_rgb.h"

/* Sortie automatique du mode configuration après 30 min sans activité */
#define CONFIG_INACTIVITE_MS  (30U * 60U * 1000U)

static Mode mode_courant = MODE_STANDARD;

/* Heure de la dernière activité en configuration (remise à jour à chaque caractère tapé) */
static uint32_t derniere_activite_config_ms = 0U;

/* Change de mode et met la LED à la couleur du mode (tableau du livrable 1) */
static void Modes_Changer(Mode nouveau)
{
    mode_courant = nouveau;

    switch (nouveau)
    {
        case MODE_STANDARD:
            GroveRGB_SetColor(0, 255, 0);       /* vert */
            printf("Mode STANDARD\r\n");
            break;

        case MODE_CONFIGURATION:
            GroveRGB_SetColor(255, 255, 0);     /* jaune */
            printf("Mode CONFIGURATION\r\n");
            break;

        case MODE_ECONOMIQUE:
            GroveRGB_SetColor(0, 0, 255);       /* bleu */
            printf("Mode ECONOMIQUE\r\n");
            break;

        case MODE_MAINTENANCE:
            GroveRGB_SetColor(255, 80, 0);      /* orange */
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
            /* Rouge 5 s ramène toujours au standard, jamais en maintenance (choix du livrable 1) */
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
            /* Les boutons ne font rien : on sort seulement après 30 min sans activité */
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
