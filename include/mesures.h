#ifndef MESURES_H
#define MESURES_H

#include "modes.h"
#include "led.h"

/* Prépare les capteurs (BME280 et lumière). À appeler une fois au démarrage */
void Mesures_Init(void);

/*
 * À appeler à chaque tour de la boucle principale.
 * Fait une mesure quand LOG_INTERVAL est écoulé (x2 en mode économique,
 * toutes les 2 s en maintenance), sans jamais bloquer la boucle.
 * Rien en mode configuration.
 */
void Mesures_MettreAJour(Mode mode);

/* Erreur à signaler d'après la dernière mesure : capteur en erreur, données hors seuils, ou aucune */
Erreur Mesures_Erreur(void);

#endif
