#ifndef BOUTONS_H
#define BOUTONS_H

#include <stdbool.h>

/* Configure les deux boutons du Grove Dual Button en interruption */
void Boutons_Init(void);

/* Lecture directe : le bouton rouge est-il enfoncé maintenant ? (sert au démarrage) */
bool Boutons_RougeEnfonce(void);

/* Vrai une seule fois, quand le bouton est tenu depuis 5 s */
bool Boutons_RougeAppuiLong(void);
bool Boutons_VertAppuiLong(void);

#endif
