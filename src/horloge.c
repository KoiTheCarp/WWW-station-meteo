#include <stdio.h>
#include "horloge.h"
#include "grove_rtc_ds1307.h"
#include "integer.h"

static RTC_DateTime derniere_date = { 0, 0, 0, 1, 1, 1, 2026 };

void Horloge_Init(void)
{
    printf("RTC : %s\r\n", GroveRTC_Init() ? "prete" : "pas de reponse");
}

bool Horloge_Lire(RTC_DateTime *dt)
{
    if (!GroveRTC_GetDateTime(dt))
    {
        return false;
    }
    derniere_date = *dt;
    return true;
}

static bool Horloge_Modifier(RTC_DateTime *dt)
{
    return GroveRTC_GetDateTime(dt);
}

bool Horloge_RegleHeure(uint8_t heures, uint8_t minutes, uint8_t secondes)
{
    RTC_DateTime dt;

    if (!Horloge_Modifier(&dt))
    {
        return false;
    }
    dt.hours = heures;
    dt.minutes = minutes;
    dt.seconds = secondes;
    return GroveRTC_SetDateTime(&dt);
}

bool Horloge_RegleDate(uint8_t mois, uint8_t jour, uint16_t annee)
{
    RTC_DateTime dt;

    if (!Horloge_Modifier(&dt))
    {
        return false;
    }
    dt.month = mois;
    dt.day = jour;
    dt.year = annee;
    return GroveRTC_SetDateTime(&dt);
}

bool Horloge_RegleJour(uint8_t jour_semaine)
{
    RTC_DateTime dt;

    if (!Horloge_Modifier(&dt))
    {
        return false;
    }
    dt.dayOfWeek = jour_semaine;
    return GroveRTC_SetDateTime(&dt);
}

DWORD get_fattime(void)
{
    const RTC_DateTime *d = &derniere_date;
    uint32_t annee = (d->year >= 1980U) ? (d->year - 1980U) : 0U;

    return (annee << 25) | ((uint32_t)d->month << 21) | ((uint32_t)d->day << 16)
         | ((uint32_t)d->hours << 11) | ((uint32_t)d->minutes << 5) | ((uint32_t)d->seconds >> 1);
}
