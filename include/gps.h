#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <stdint.h>

/* Ce que le GPS a donné à la dernière lecture */
typedef enum
{
    GPS_MUET = 0,           /* plus rien reçu du module depuis TIMEOUT : débranché ou en panne */
    GPS_SANS_POSITION,      /* le module parle, mais pas encore de position (pas assez de satellites) */
    GPS_POSITION            /* position valide */
} EtatGps;

/*
 * Démarre la réception du GPS par interruption : chaque octet reçu est rangé
 * sans bloquer la boucle principale, et la dernière trame GGA est gardée.
 */
void Gps_Init(void);

/*
 * Analyse la dernière trame GGA reçue. Latitude et longitude en degrés (négatif = sud / ouest).
 * silence_max_ms : sans aucun octet reçu depuis cette durée (paramètre TIMEOUT), le GPS est muet.
 */
EtatGps Gps_Lire(double *latitude, double *longitude, uint32_t silence_max_ms);

#endif
