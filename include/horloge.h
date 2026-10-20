#ifndef HORLOGE_H
#define HORLOGE_H

#include <stdbool.h>
#include "cesi_types.h"

/* Vérifie que la RTC DS1307 répond. À appeler une fois au démarrage */
void Horloge_Init(void);

/* Lit la date et l'heure dans la RTC. Faux si la RTC ne répond pas */
bool Horloge_Lire(RTC_DateTime *dt);

/* Réglages (commandes CLOCK, DATE et DAY du mode configuration). Faux si la RTC ne répond pas */
bool Horloge_RegleHeure(uint8_t heures, uint8_t minutes, uint8_t secondes);
bool Horloge_RegleDate(uint8_t mois, uint8_t jour, uint16_t annee);
bool Horloge_RegleJour(uint8_t jour_semaine);   /* 1 = lundi ... 7 = dimanche */

#endif
