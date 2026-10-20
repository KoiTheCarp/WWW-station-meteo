#ifndef PARAMETRES_H
#define PARAMETRES_H

#include <stdbool.h>
#include <stdint.h>

/* Les paramètres réglables en mode configuration (tableau de la spécification) */
typedef enum
{
    P_LOG_INTERVAL = 0,     /* minutes entre 2 mesures */
    P_FILE_MAX_SIZE,        /* taille max d'un fichier de log, en octets */
    P_TIMEOUT,              /* secondes avant d'abandonner un capteur */
    P_LUMIN,
    P_LUMIN_LOW,
    P_LUMIN_HIGH,
    P_TEMP_AIR,
    P_MIN_TEMP_AIR,
    P_MAX_TEMP_AIR,
    P_HYGR,
    P_HYGR_MINT,
    P_HYGR_MAXT,
    P_PRESSURE,
    P_PRESSURE_MIN,
    P_PRESSURE_MAX,
    NB_PARAMETRES
} IdParametre;

/* Charge les paramètres sauvegardés en mémoire Flash, ou les valeurs par défaut */
void Parametres_Init(void);

int32_t Parametre(IdParametre id);

/* Cherche un paramètre par son nom (« LOG_INTERVAL »...). Faux si le nom est inconnu */
bool Parametres_Chercher(const char *nom, IdParametre *id);

const char *Parametres_Nom(IdParametre id);

/* Change une valeur et la sauvegarde. Faux si elle sort du domaine de définition */
bool Parametres_Modifier(IdParametre id, int32_t valeur);

/* Domaine de définition, pour les messages d'erreur */
void Parametres_Domaine(IdParametre id, int32_t *min, int32_t *max);

/* Commande RESET : toutes les valeurs par défaut, puis sauvegarde */
void Parametres_Reinitialiser(void);

#endif
