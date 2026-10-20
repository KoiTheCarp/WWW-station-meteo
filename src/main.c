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

/* Les valeurs de Erreur sont rangées par priorité : la plus petite (hors AUCUNE) passe devant */
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

/* Entrée et sortie du mode maintenance : la carte SD n'est plus touchée pendant ce mode */
static void Gerer_Maintenance(Mode mode, Mode mode_precedent)
{
    if (mode == MODE_MAINTENANCE && mode_precedent != MODE_MAINTENANCE)
    {
        Stockage_Arreter();
    }
    else if (mode != MODE_MAINTENANCE && mode_precedent == MODE_MAINTENANCE)
    {
        /* Livrable 1 : en sortant, on initialise la carte présente ; absente = erreur SD */
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

    /* Diagramme d'activité : bouton rouge enfoncé au démarrage -> configuration, sinon standard */
    Modes_Demarrer();
    Mode mode_precedent = Modes_Courant();

    while (1)
    {
        /* Ne jamais bloquer ici : la boucle doit tourner vite pour voir les appuis de 5 s */
        Modes_MettreAJour();

        Mode mode = Modes_Courant();
        Gerer_Maintenance(mode, mode_precedent);
        mode_precedent = mode;

        /* Mode configuration : commandes tapées sur le port série */
        Console_MettreAJour(mode);

        /* Une mesure toutes les LOG_INTERVAL (sans bloquer) */
        if (Mesures_MettreAJour(mode))
        {
            printf("%s", Mesures_Ligne());

            /* En maintenance, les mesures sont seulement affichées */
            if (mode != MODE_MAINTENANCE)
            {
                bool date_valide;
                const RTC_DateTime *date = Mesures_Date(&date_valide);
                Stockage_EcrireLigne(date, date_valide, Mesures_Entete(), Mesures_Ligne(),
                                     (uint32_t)Parametre(P_FILE_MAX_SIZE));
            }
        }

        /* LED : erreur la plus prioritaire (RTC, GPS, capteur, incohérent, SD), sauf en configuration */
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
