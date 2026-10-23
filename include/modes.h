#ifndef MODES_H
#define MODES_H

typedef enum
{
    MODE_STANDARD = 0,
    MODE_CONFIGURATION,
    MODE_ECONOMIQUE,
    MODE_MAINTENANCE
} Mode;

void Modes_Demarrer(void);

void Modes_MettreAJour(void);

Mode Modes_Courant(void);

void Modes_SignalerActivite(void);

#endif
