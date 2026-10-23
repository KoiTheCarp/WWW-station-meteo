#include <stdio.h>
#include "stm32l4xx_hal.h"
#include "mesures.h"
#include "bme280.h"
#include "grove_light.h"
#include "gps.h"
#include "horloge.h"
#include "parametres.h"
#include "board_pins.h"

extern I2C_HandleTypeDef hi2c1;

#if MESURES_TEST
#define UNITE_INTERVALLE_MS  1000U
#define NB_ECHECS_ERREUR     1U
#else
#define UNITE_INTERVALLE_MS  (60U * 1000U)
#define NB_ECHECS_ERREUR     2U
#endif

#define ESSAI_PERIODE_MS  100U

#define GPS_SILENCE_MS  3000U

#define MAINTENANCE_PERIODE_MS  2000U

#define LUMIERE_ERREUR  0xFFFFU

#define LUMIERE_ECART_DEBRANCHE  400U

typedef struct
{
    const char *nom;
    IdParametre parametre_actif;
    bool actif;
    float valeur;
    bool valide;
    uint8_t nb_echecs;
    bool a_seuils;
    float min;
    float max;
} Capteur;

enum
{
    CAPTEUR_TEMPERATURE = 0,
    CAPTEUR_HUMIDITE,
    CAPTEUR_PRESSION,
    CAPTEUR_LUMIERE,
    NB_CAPTEURS
};

static Capteur capteurs[NB_CAPTEURS] =
{
    { "Temperature", P_TEMP_AIR, true, 0.0f, false, 0U, true,  0.0f, 0.0f },
    { "Humidite",    P_HYGR,     true, 0.0f, false, 0U, false, 0.0f, 0.0f },
    { "Pression",    P_PRESSURE, true, 0.0f, false, 0U, true,  0.0f, 0.0f },
    { "Lumiere",     P_LUMIN,    true, 0.0f, false, 0U, false, 0.0f, 0.0f },
};

static BME280_Handle bme280;

typedef struct
{
    EtatGps etat;
    bool connue;
    double latitude;
    double longitude;
    uint8_t nb_echecs;
} PositionGps;

static PositionGps position = { GPS_MUET, false, 0.0, 0.0, 0U };
static uint32_t nb_mesures_eco = 0U;

static RTC_DateTime date_mesure;
static bool date_valide = false;
static bool rtc_en_erreur = false;

typedef struct
{
    bool en_cours;
    uint32_t debut_ms;
    uint32_t dernier_essai_ms;
    bool rtc_ok;
    bool bme280_ok;
    bool lumiere_ok;
    bool gps_fini;
    BME280_Data env;
    uint16_t lumiere;
} MesureEnCours;

static MesureEnCours mesure = { 0 };

static uint32_t derniere_mesure_ms = 0U;
static bool premiere_mesure = true;
static Mode mode_precedent = MODE_CONFIGURATION;

static char ligne[160];

static const char ENTETE[] = "DATE, HEURE, TEMP_AIR_C, HYGR_PCT, PRESSION_HPA, LUMIERE, NIVEAU_LUMIERE, LATITUDE, LONGITUDE\r\n";

static bool Capteur_HorsSeuils(const Capteur *c)
{
    return c->actif && c->valide && c->a_seuils && (c->valeur < c->min || c->valeur > c->max);
}

static void Capteur_Noter(Capteur *c, bool reussie, float valeur)
{
    if (!c->actif)
    {
        c->valide = false;
        c->nb_echecs = 0U;
    }
    else if (reussie)
    {
        c->valeur = valeur;
        c->valide = true;
        c->nb_echecs = 0U;
    }
    else
    {
        c->valide = false;
        if (c->nb_echecs < 255U)
        {
            c->nb_echecs++;
        }
    }
}

static void Mesures_AppliquerParametres(void)
{
    for (int i = 0; i < NB_CAPTEURS; i++)
    {
        capteurs[i].actif = Parametre(capteurs[i].parametre_actif) != 0;
    }
    capteurs[CAPTEUR_TEMPERATURE].min = (float)Parametre(P_MIN_TEMP_AIR);
    capteurs[CAPTEUR_TEMPERATURE].max = (float)Parametre(P_MAX_TEMP_AIR);
    capteurs[CAPTEUR_PRESSION].min = (float)Parametre(P_PRESSURE_MIN);
    capteurs[CAPTEUR_PRESSION].max = (float)Parametre(P_PRESSURE_MAX);
}

static bool Mesures_GpsALire(Mode mode)
{
    if (mode == MODE_ECONOMIQUE)
    {
        nb_mesures_eco++;
        return (nb_mesures_eco % 2U) != 0U;
    }
    nb_mesures_eco = 0U;
    return true;
}

static void Mesures_EssayerGps(void)
{
    double latitude, longitude;

    position.etat = Gps_Lire(&latitude, &longitude, GPS_SILENCE_MS);
    if (position.etat == GPS_POSITION)
    {
        position.latitude = latitude;
        position.longitude = longitude;
        position.connue = true;
    }
    else if (position.etat == GPS_SANS_POSITION)
    {
        position.connue = false;
    }
    mesure.gps_fini = (position.etat != GPS_MUET);
}

static uint16_t Mesures_LireLumiereApres(GPIO_PinState tension)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = LIGHT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(LIGHT_GPIO_PORT, LIGHT_GPIO_PIN, tension);
    HAL_GPIO_Init(LIGHT_GPIO_PORT, &gpio);
    for (volatile uint32_t i = 0U; i < 50U; i++)
    {
    }

    gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    HAL_GPIO_Init(LIGHT_GPIO_PORT, &gpio);
    return GroveLight_ReadRaw();
}

static uint16_t Mesures_LireLumiere(void)
{
    uint16_t apres_0v = Mesures_LireLumiereApres(GPIO_PIN_RESET);
    uint16_t apres_3v3 = Mesures_LireLumiereApres(GPIO_PIN_SET);

    if (apres_0v == LUMIERE_ERREUR || apres_3v3 == LUMIERE_ERREUR)
    {
        return LUMIERE_ERREUR;
    }

    uint16_t ecart = (apres_3v3 > apres_0v) ? (uint16_t)(apres_3v3 - apres_0v)
                                            : (uint16_t)(apres_0v - apres_3v3);
    if (ecart > LUMIERE_ECART_DEBRANCHE)
    {
        return LUMIERE_ERREUR;
    }
    return (uint16_t)((apres_0v + apres_3v3) / 2U);
}

static void Mesures_Commencer(Mode mode)
{
    Mesures_AppliquerParametres();

    mesure.en_cours = true;
    mesure.debut_ms = HAL_GetTick();
    mesure.rtc_ok = false;
    mesure.bme280_ok = !(capteurs[CAPTEUR_TEMPERATURE].actif || capteurs[CAPTEUR_HUMIDITE].actif
                         || capteurs[CAPTEUR_PRESSION].actif);
    mesure.lumiere_ok = !capteurs[CAPTEUR_LUMIERE].actif;
    mesure.gps_fini = !Mesures_GpsALire(mode);
}

static bool Mesures_Essayer(void)
{
    mesure.dernier_essai_ms = HAL_GetTick();

    if (!mesure.rtc_ok)
    {
        mesure.rtc_ok = Horloge_Lire(&date_mesure);
    }

    if (!mesure.bme280_ok)
    {
        mesure.bme280_ok = BME280_Init(&bme280, &hi2c1) && BME280_Read(&bme280, &mesure.env);
    }

    if (!mesure.lumiere_ok)
    {
        mesure.lumiere = Mesures_LireLumiere();
        mesure.lumiere_ok = (mesure.lumiere != LUMIERE_ERREUR);
    }

    if (!mesure.gps_fini)
    {
        Mesures_EssayerGps();
    }

    return mesure.rtc_ok && mesure.bme280_ok && mesure.lumiere_ok && mesure.gps_fini;
}

static void Mesures_Terminer(void)
{
    mesure.en_cours = false;

    date_valide = mesure.rtc_ok;
    rtc_en_erreur = !mesure.rtc_ok;

    Capteur_Noter(&capteurs[CAPTEUR_TEMPERATURE], mesure.bme280_ok, mesure.env.temperature_c);
    Capteur_Noter(&capteurs[CAPTEUR_HUMIDITE],    mesure.bme280_ok, mesure.env.humidity_percent);
    Capteur_Noter(&capteurs[CAPTEUR_PRESSION],    mesure.bme280_ok, mesure.env.pressure_hpa);
    Capteur_Noter(&capteurs[CAPTEUR_LUMIERE],     mesure.lumiere_ok, (float)mesure.lumiere);

    if (!mesure.gps_fini && position.etat == GPS_MUET)
    {
        if (position.nb_echecs < 255U)
        {
            position.nb_echecs++;
        }
    }
    else if (mesure.gps_fini && position.etat != GPS_MUET)
    {
        position.nb_echecs = 0U;
    }
}

static int Mesures_Colonne(char *p, size_t reste, const Capteur *c, const char *format, bool pris_en_compte)
{
    if (!c->actif)
    {
        return snprintf(p, reste, ", OFF");
    }
    if (!c->valide || !pris_en_compte)
    {
        return snprintf(p, reste, ", NA");
    }
    return snprintf(p, reste, format, c->valeur);
}

static const char *Mesures_NiveauLumiere(const Capteur *c)
{
    if (!c->actif)
    {
        return "OFF";
    }
    if (!c->valide)
    {
        return "NA";
    }

    int32_t niveau = (int32_t)(c->valeur * 1023.0f / 4095.0f);
    if (niveau < Parametre(P_LUMIN_LOW))
    {
        return "FAIBLE";
    }
    if (niveau > Parametre(P_LUMIN_HIGH))
    {
        return "FORTE";
    }
    return "MOYENNE";
}

static bool Mesures_HumiditeOk(void)
{
    const Capteur *t = &capteurs[CAPTEUR_TEMPERATURE];
    return !t->valide
        || (t->valeur >= (float)Parametre(P_HYGR_MINT) && t->valeur <= (float)Parametre(P_HYGR_MAXT));
}

static void Mesures_FormaterLigne(void)
{
    char *p = ligne;
    size_t reste = sizeof(ligne);
    int n;

    if (date_valide)
    {
        n = snprintf(p, reste, "%04u-%02u-%02u, %02u:%02u:%02u",
                     (unsigned)date_mesure.year, (unsigned)date_mesure.month, (unsigned)date_mesure.day,
                     (unsigned)date_mesure.hours, (unsigned)date_mesure.minutes, (unsigned)date_mesure.seconds);
    }
    else
    {
        n = snprintf(p, reste, "NA, NA");
    }
    p += n;
    reste -= (size_t)n;

    bool humidite_ok = Mesures_HumiditeOk();

    n = Mesures_Colonne(p, reste, &capteurs[CAPTEUR_TEMPERATURE], ", %.1f", true);
    p += n; reste -= (size_t)n;
    n = Mesures_Colonne(p, reste, &capteurs[CAPTEUR_HUMIDITE], ", %.1f", humidite_ok);
    p += n; reste -= (size_t)n;
    n = Mesures_Colonne(p, reste, &capteurs[CAPTEUR_PRESSION], ", %.1f", true);
    p += n; reste -= (size_t)n;
    n = Mesures_Colonne(p, reste, &capteurs[CAPTEUR_LUMIERE], ", %.0f", true);
    p += n; reste -= (size_t)n;
    n = snprintf(p, reste, ", %s", Mesures_NiveauLumiere(&capteurs[CAPTEUR_LUMIERE]));
    p += n; reste -= (size_t)n;

    if (position.connue && position.etat != GPS_MUET)
    {
        snprintf(p, reste, ", %.5f, %.5f\r\n", position.latitude, position.longitude);
    }
    else
    {
        snprintf(p, reste, ", NA, NA\r\n");
    }
}

static void Mesures_AfficherValeur(const Capteur *c, const char *format, bool pris_en_compte)
{
    if (!c->actif)
    {
        printf("OFF");
    }
    else if (!c->valide || !pris_en_compte)
    {
        printf("NA");
    }
    else
    {
        printf(format, c->valeur);
    }
}

void Mesures_Afficher(void)
{
    if (date_valide)
    {
        printf("%02u:%02u:%02u  ", (unsigned)date_mesure.hours, (unsigned)date_mesure.minutes,
               (unsigned)date_mesure.seconds);
    }
    else
    {
        printf("Heure NA  ");
    }

    printf("Temp ");
    Mesures_AfficherValeur(&capteurs[CAPTEUR_TEMPERATURE], "%.1f °C", true);
    printf(" | Humidite ");
    Mesures_AfficherValeur(&capteurs[CAPTEUR_HUMIDITE], "%.1f %%", Mesures_HumiditeOk());
    printf(" | Pression ");
    Mesures_AfficherValeur(&capteurs[CAPTEUR_PRESSION], "%.1f hPa", true);
    printf(" | Lumiere ");
    Mesures_AfficherValeur(&capteurs[CAPTEUR_LUMIERE], "%.0f", true);
    if (capteurs[CAPTEUR_LUMIERE].actif && capteurs[CAPTEUR_LUMIERE].valide)
    {
        printf(" (%s)", Mesures_NiveauLumiere(&capteurs[CAPTEUR_LUMIERE]));
    }

    if (position.connue && position.etat != GPS_MUET)
    {
        printf(" | GPS %.5f, %.5f\r\n", position.latitude, position.longitude);
    }
    else
    {
        printf(" | GPS NA\r\n");
    }
}

Erreur Mesures_Erreur(void)
{
    if (rtc_en_erreur)
    {
        return ERREUR_RTC;
    }
    if (position.nb_echecs >= NB_ECHECS_ERREUR)
    {
        return ERREUR_GPS;
    }
    for (int i = 0; i < NB_CAPTEURS; i++)
    {
        if (capteurs[i].nb_echecs >= NB_ECHECS_ERREUR)
        {
            return ERREUR_CAPTEUR;
        }
    }
    for (int i = 0; i < NB_CAPTEURS; i++)
    {
        if (Capteur_HorsSeuils(&capteurs[i]))
        {
            return ERREUR_DONNEES_INCOHERENTES;
        }
    }
    return ERREUR_AUCUNE;
}

void Mesures_Init(void)
{
    GroveLight_Init();
    Gps_Init();

    printf("BME280 : %s\r\n", BME280_Init(&bme280, &hi2c1) ? "pret" : "pas de reponse");
}

bool Mesures_MettreAJour(Mode mode)
{
    uint32_t intervalle_ms;
    uint32_t log_interval = (uint32_t)Parametre(P_LOG_INTERVAL);

    switch (mode)
    {
        case MODE_STANDARD:
            intervalle_ms = log_interval * UNITE_INTERVALLE_MS;
            break;

        case MODE_ECONOMIQUE:
            intervalle_ms = 2U * log_interval * UNITE_INTERVALLE_MS;
            break;

        case MODE_MAINTENANCE:
            intervalle_ms = MAINTENANCE_PERIODE_MS;
            break;

        default:
            mode_precedent = mode;
            mesure.en_cours = false;
            return false;
    }

    if (mode_precedent == MODE_CONFIGURATION)
    {
        premiere_mesure = true;
    }
    mode_precedent = mode;

    if (!mesure.en_cours)
    {
        if (!premiere_mesure && (HAL_GetTick() - derniere_mesure_ms) < intervalle_ms)
        {
            return false;
        }

        premiere_mesure = false;
        derniere_mesure_ms = HAL_GetTick();
        Mesures_Commencer(mode);
    }
    else if ((HAL_GetTick() - mesure.dernier_essai_ms) < ESSAI_PERIODE_MS)
    {
        return false;
    }

    bool tout_repondu = Mesures_Essayer();
    uint32_t timeout_ms = (uint32_t)Parametre(P_TIMEOUT) * 1000U;

    if (!tout_repondu && (HAL_GetTick() - mesure.debut_ms) < timeout_ms)
    {
        return false;
    }

    Mesures_Terminer();
    Mesures_FormaterLigne();
    return true;
}

const char *Mesures_Entete(void)
{
    return ENTETE;
}

const char *Mesures_Ligne(void)
{
    return ligne;
}

const RTC_DateTime *Mesures_Date(bool *valide)
{
    *valide = date_valide;
    return &date_mesure;
}
