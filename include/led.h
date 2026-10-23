#ifndef LED_H
#define LED_H

#include "modes.h"

typedef enum
{
    ERREUR_AUCUNE = 0,
    ERREUR_RTC,
    ERREUR_GPS,
    ERREUR_CAPTEUR,
    ERREUR_DONNEES_INCOHERENTES,
    ERREUR_SD_PLEINE,
    ERREUR_SD_ECRITURE
} Erreur;

void Led_MettreAJour(Mode mode, Erreur erreur);

#endif
