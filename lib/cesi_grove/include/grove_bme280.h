#ifndef GROVE_BME280_H
#define GROVE_BME280_H

#include <stdbool.h>
#include "cesi_types.h"

/* Vérifie l'identité de la puce (0x60), lit sa calibration et lance les mesures. */
bool GroveBME280_Init(void);

/* Remplit température (°C), humidité (%) et pression (hPa). Renvoie false si la lecture échoue. */
bool GroveBME280_ReadEnvironment(Env_Data *data);

#endif
