#ifndef MESURES_H
#define MESURES_H

#include <stdbool.h>
#include "cesi_types.h"
#include "modes.h"
#include "led.h"

/* Prépare les capteurs (BME280, lumière, GPS). À appeler une fois au démarrage */
void Mesures_Init(void);

/*
 * À appeler à chaque tour de la boucle principale.
 * Fait une mesure quand LOG_INTERVAL est écoulé (x2 en mode économique,
 * toutes les 2 s en maintenance), sans jamais bloquer la boucle.
 * Rien en mode configuration. Vrai quand une nouvelle mesure vient d'être faite.
 */
bool Mesures_MettreAJour(Mode mode);

/* Dernière mesure en CSV (date, heure, capteurs, position), terminée par \r\n */
const char *Mesures_Ligne(void);

/* Dernière mesure affichée en clair sur la console (heure, valeurs avec leurs unités) */
void Mesures_Afficher(void);

/* Noms des colonnes, première ligne de chaque fichier de log */
const char *Mesures_Entete(void);

/* Date de la dernière mesure. *valide = faux si la RTC n'a pas répondu */
const RTC_DateTime *Mesures_Date(bool *valide);

/* Erreur à signaler d'après la dernière mesure : GPS muet, capteur en erreur, données hors seuils, ou aucune */
Erreur Mesures_Erreur(void);

#endif
