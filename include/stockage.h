#ifndef STOCKAGE_H
#define STOCKAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "cesi_types.h"
#include "led.h"

/* Initialise la carte SD et monte le système de fichiers FAT32. Faux si la carte est absente ou illisible */
bool Stockage_Demarrer(void);

/* Mode maintenance : plus aucune écriture, la carte peut être retirée sans risque */
void Stockage_Arreter(void);

/*
 * Ajoute une ligne CSV au fichier du jour AAMMJJ_0.LOG (en-tête si le fichier est neuf).
 * Si la ligne ferait dépasser taille_max, le fichier est d'abord archivé en
 * AAMMJJ_1.LOG, AAMMJJ_2.LOG... et on repart sur un AAMMJJ_0.LOG vide.
 * date_valide = faux si la RTC n'a pas répondu : on écrit alors dans le dernier fichier utilisé.
 * entete = première ligne d'un fichier neuf (noms des colonnes).
 */
void Stockage_EcrireLigne(const RTC_DateTime *date, bool date_valide,
                          const char *entete, const char *ligne, uint32_t taille_max);

/* ERREUR_SD_PLEINE, ERREUR_SD_ECRITURE ou ERREUR_AUCUNE d'après la dernière écriture */
Erreur Stockage_Erreur(void);

#endif
