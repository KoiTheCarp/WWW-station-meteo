#ifndef CARTE_SD_H
#define CARTE_SD_H

/*
 * Pilote bas niveau de la carte SD (SPI). Les fonctions disk_* utilisées
 * par FatFs sont dans carte_sd.c ; ici seulement ce dont stockage.c a besoin.
 */

/* Oublie la carte : elle sera réinitialisée au prochain montage (carte retirée ou changée) */
void CarteSd_Oublier(void);

#endif
