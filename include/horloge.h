#ifndef HORLOGE_H
#define HORLOGE_H

#include <stdbool.h>
#include "cesi_types.h"

void Horloge_Init(void);

bool Horloge_Lire(RTC_DateTime *dt);

bool Horloge_RegleHeure(uint8_t heures, uint8_t minutes, uint8_t secondes);
bool Horloge_RegleDate(uint8_t mois, uint8_t jour, uint16_t annee);
bool Horloge_RegleJour(uint8_t jour_semaine);

#endif
