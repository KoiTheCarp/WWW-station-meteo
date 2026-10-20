#ifndef LED_H
#define LED_H

#include "modes.h"

/*
 * Erreurs signalées par la LED (tableau de la spécification).
 * Si plusieurs erreurs arrivent en même temps, on affiche la première de la liste.
 */
typedef enum
{
    ERREUR_AUCUNE = 0,
    ERREUR_RTC,                 /* rouge / bleu, durées égales */
    ERREUR_GPS,                 /* rouge / jaune, durées égales */
    ERREUR_CAPTEUR,             /* rouge / vert, durées égales */
    ERREUR_DONNEES_INCOHERENTES,/* rouge / vert, vert 2 fois plus long */
    ERREUR_SD_PLEINE,           /* rouge / blanc, durées égales */
    ERREUR_SD_ECRITURE          /* rouge / blanc, blanc 2 fois plus long */
} Erreur;

/*
 * À appeler à chaque tour de la boucle principale.
 * Fait clignoter la LED à 1 Hz tant qu'il y a une erreur, sans bloquer.
 * Quand l'erreur disparaît, la LED reprend la couleur du mode.
 */
void Led_MettreAJour(Mode mode, Erreur erreur);

#endif
