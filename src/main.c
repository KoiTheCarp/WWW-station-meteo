#include <stdio.h>
#include "setup.h"
#include "cesi_types.h"
#include "grove_rgb.h"
#include "grove_button.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "bme280.h"
#include "sd_logger.h"
#include "boutons.h"
#include "modes.h"
#include "stm32l4xx_hal.h"


extern I2C_HandleTypeDef hi2c1;

int main(void)
{
    Global_Init();
    GroveRGB_Init();
    Boutons_Init();

    printf("WWW : demarrage\r\n");

    /* Diagramme d'activité : bouton rouge enfoncé au démarrage -> configuration, sinon standard */
    Modes_Demarrer();

    GroveLight_Init();
    uint32_t derniere_lecture_ms = 0;

    bool rtc_ok = GroveRTC_Init();
    printf("RTC : ok=%d\r\n", rtc_ok);
        /* Mise à l'heure : une seule fois, puis mettre ces 2 lignes en commentaire */
    RTC_DateTime reglage = { .seconds = 0, .minutes = 24, .hours = 11, .dayOfWeek = 5, .day = 16, .month = 10, .year = 2026 };
    GroveRTC_SetDateTime(&reglage);
    RTC_DateTime maintenant;

    BME280_Handle bme;
    BME280_Data env;
    bool env_ok = BME280_Init(&bme, &hi2c1);
    // printf("BME280 init : ok=%d\r\n", env_ok);

    GroveGPS_Init();
    GPS_Data gps = {0};

    while (1)
    {
        /* Ne jamais bloquer ici : la boucle doit tourner vite pour voir les appuis de 5 s */
        Modes_MettreAJour();

        /* Test palier 1 : une lecture par seconde, sans bloquer */
        if ((HAL_GetTick() - derniere_lecture_ms) >= 1000U)
        {
            derniere_lecture_ms = HAL_GetTick();

            uint16_t lumiere = GroveLight_ReadRaw();
            // printf("Lumiere : %u\r\n", lumiere);

            GroveRTC_GetDateTime(&maintenant);
            printf("Heure : %02d:%02d:%02d\r\n", maintenant.hours, maintenant.minutes, maintenant.seconds);
            printf("Date : %02d/%02d/%04d\r\n", maintenant.day, maintenant.month, maintenant.year);
            BME280_Read(&bme, &env);
            // printf("Temp : %.2f C  Hum : %.2f %%  Pression : %.2f hPa\r\n", env.temperature_c, env.humidity_percent, env.pressure_hpa);

            bool gps_ok = GroveGPS_ReadGGA(&gps, 1100);
            // printf("GPS : ok=%d fix=%d satellites=%d\r\n", gps_ok, gps.fix_quality, gps.satellites);
            // printf("Position : lat=%.6f lon=%.6f\r\n", gps.latitude, gps.longitude);
        }

        /* Palier 1 : la mesure toutes les LOG_INTERVAL viendra ici */
    }
}