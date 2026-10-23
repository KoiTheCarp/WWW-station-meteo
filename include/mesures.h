#ifndef MESURES_H
#define MESURES_H

#include <stdbool.h>
#include "cesi_types.h"
#include "modes.h"
#include "led.h"

void Mesures_Init(void);

bool Mesures_MettreAJour(Mode mode);

const char *Mesures_Ligne(void);

void Mesures_Afficher(void);

const char *Mesures_Entete(void);

const RTC_DateTime *Mesures_Date(bool *valide);

Erreur Mesures_Erreur(void);

#endif
