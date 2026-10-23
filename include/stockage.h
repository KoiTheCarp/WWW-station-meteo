#ifndef STOCKAGE_H
#define STOCKAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "cesi_types.h"
#include "led.h"

bool Stockage_Demarrer(void);

void Stockage_Arreter(void);

void Stockage_EcrireLigne(const RTC_DateTime *date, bool date_valide,
                          const char *entete, const char *ligne, uint32_t taille_max);

Erreur Stockage_Erreur(void);

#endif
