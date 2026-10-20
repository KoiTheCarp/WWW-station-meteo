#ifndef CONSOLE_H
#define CONSOLE_H

#include "modes.h"

/* Démarre la réception des caractères tapés sur le port série (USART2, par interruption) */
void Console_Init(void);

/*
 * À appeler à chaque tour de la boucle principale.
 * En mode configuration : traite les commandes tapées (LOG_INTERVAL=5, CLOCK=14:30:00, RESET...).
 * Dans les autres modes, les caractères reçus sont ignorés.
 */
void Console_MettreAJour(Mode mode);

#endif
