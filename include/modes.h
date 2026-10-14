#ifndef MODES_H
#define MODES_H

/* Les quatre modes de la spécification (livrable 1) */
typedef enum
{
    MODE_STANDARD = 0,
    MODE_CONFIGURATION,
    MODE_ECONOMIQUE,
    MODE_MAINTENANCE
} Mode;

/* Au démarrage : configuration si le bouton rouge est enfoncé, sinon standard */
void Modes_Demarrer(void);

/* À appeler à chaque tour de la boucle principale : traite les appuis longs */
void Modes_MettreAJour(void);

Mode Modes_Courant(void);

#endif
