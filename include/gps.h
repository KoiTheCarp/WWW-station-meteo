#ifndef GPS_H
#define GPS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    GPS_MUET = 0,
    GPS_SANS_POSITION,
    GPS_POSITION
} EtatGps;

void Gps_Init(void);

EtatGps Gps_Lire(double *latitude, double *longitude, uint32_t silence_max_ms);

#endif
