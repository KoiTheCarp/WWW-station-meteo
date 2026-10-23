#ifndef PARAMETRES_H
#define PARAMETRES_H

#include <stdbool.h>
#include <stdint.h>

#ifndef MESURES_TEST
#define MESURES_TEST  1
#endif

typedef enum
{
    P_LOG_INTERVAL = 0,
    P_FILE_MAX_SIZE,
    P_TIMEOUT,
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

void Parametres_Init(void);

int32_t Parametre(IdParametre id);

bool Parametres_Chercher(const char *nom, IdParametre *id);

const char *Parametres_Nom(IdParametre id);

bool Parametres_Modifier(IdParametre id, int32_t valeur);

void Parametres_Domaine(IdParametre id, int32_t *min, int32_t *max);

void Parametres_Reinitialiser(void);

#endif
