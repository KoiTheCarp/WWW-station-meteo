#include <stdio.h>
#include "setup.h"
#include "grove_rgb.h"
#include "boutons.h"
#include "modes.h"
#include "mesures.h"
#include "led.h"
#include "horloge.h"
#include "parametres.h"
#include "stockage.h"
#include "console.h"

static Erreur Plus_Prioritaire(Erreur a, Erreur b)
{
    if (a == ERREUR_AUCUNE)
    {
        return b;
    }
    if (b == ERREUR_AUCUNE)
    {
        return a;
    }
    return (a < b) ? a : b;
}

static void Gerer_Maintenance(Mode mode, Mode mode_precedent)
{
    if (mode == MODE_MAINTENANCE && mode_precedent != MODE_MAINTENANCE)
    {
        Stockage_Arreter();
    }
    else if (mode != MODE_MAINTENANCE && mode_precedent == MODE_MAINTENANCE)
    {
        (void)Stockage_Demarrer();
    }
}

int main(void)
{
    Global_Init();
    GroveRGB_Init();
    Boutons_Init();
    Parametres_Init();
    Horloge_Init();
    Mesures_Init();
    (void)Stockage_Demarrer();
    Console_Init();

    printf("WWW : demarrage\r\n");

    Modes_Demarrer();
    Mode mode_precedent = Modes_Courant();

    while (1)
    {
        Modes_MettreAJour();

        Mode mode = Modes_Courant();
        Gerer_Maintenance(mode, mode_precedent);
        mode_precedent = mode;

        Console_MettreAJour(mode);

        if (Mesures_MettreAJour(mode))
        {
            Mesures_Afficher();

            if (mode != MODE_MAINTENANCE)
            {
                bool date_valide;
                const RTC_DateTime *date = Mesures_Date(&date_valide);
                Stockage_EcrireLigne(date, date_valide, Mesures_Entete(), Mesures_Ligne(),
                                     (uint32_t)Parametre(P_FILE_MAX_SIZE));
            }
        }

        Erreur erreur = ERREUR_AUCUNE;
        if (mode != MODE_CONFIGURATION)
        {
            erreur = Mesures_Erreur();
            if (mode != MODE_MAINTENANCE)
            {
                erreur = Plus_Prioritaire(erreur, Stockage_Erreur());
            }
        }
        Led_MettreAJour(mode, erreur);
    }
}
