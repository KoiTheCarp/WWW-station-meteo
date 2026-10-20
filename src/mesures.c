#include <stdio.h>
#include "stm32l4xx_hal.h"
#include "mesures.h"
#include "bme280.h"
#include "grove_light.h"

extern I2C_HandleTypeDef hi2c1;     /* bus I2C créé dans setup.c */

/* LOG_INTERVAL de la spécification : 10 minutes par défaut */
#define LOG_INTERVAL_DEFAUT  10U

/*
 * Pour tester sans attendre 10 min : à 1, LOG_INTERVAL compte en secondes
 * au lieu de minutes (une mesure toutes les 10 s). Remettre à 0 pour le rendu.
 */
#define MESURES_TEST  1

#if MESURES_TEST
#define UNITE_INTERVALLE_MS  1000U
#else
#define UNITE_INTERVALLE_MS  (60U * 1000U)
#endif

/* 2 lectures ratées d'affilée = capteur en erreur (commande TIMEOUT de la spécification) */
#define NB_ECHECS_ERREUR  2U

/* Mode maintenance : données affichées en direct, une mesure toutes les 2 s */
#define MAINTENANCE_PERIODE_MS  2000U

/*
 * Seuils par défaut de la spécification : en dehors, les données sont incohérentes.
 * Humidité et lumière n'en ont pas (HYGR_MINT/MAXT et LUMIN_LOW/HIGH servent à autre chose).
 */
#define MIN_TEMP_AIR_DEFAUT    (-10.0f)
#define MAX_TEMP_AIR_DEFAUT    60.0f
#define PRESSURE_MIN_DEFAUT    850.0f
#define PRESSURE_MAX_DEFAUT    1080.0f

#define LUMIERE_ERREUR  0xFFFFU     /* valeur renvoyée par GroveLight_ReadRaw si la lecture rate */

/* Tout ce qu'il faut savoir sur un capteur, regroupé dans une structure */
typedef struct
{
    const char *nom;
    const char *unite;
    float valeur;           /* dernière valeur lue */
    bool valide;            /* la dernière lecture a-t-elle réussi ? sinon on affiche NA */
    uint8_t nb_echecs;      /* lectures ratées d'affilée */
    bool a_seuils;          /* le capteur a-t-il un minimum et un maximum ? */
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
    { "Temperature", "C",   0.0f, false, 0U, true,  MIN_TEMP_AIR_DEFAUT, MAX_TEMP_AIR_DEFAUT },
    { "Humidite",    "%",   0.0f, false, 0U, false, 0.0f, 0.0f },
    { "Pression",    "hPa", 0.0f, false, 0U, true,  PRESSURE_MIN_DEFAUT, PRESSURE_MAX_DEFAUT },
    { "Lumiere",     "",    0.0f, false, 0U, false, 0.0f, 0.0f },
};

static BME280_Handle bme280;
static bool bme280_pret = false;

static uint32_t derniere_mesure_ms = 0U;
static bool premiere_mesure = true;     /* la toute première mesure se fait tout de suite */

/* Valeur lue mais en dehors des seuils : données incohérentes */
static bool Capteur_HorsSeuils(const Capteur *c)
{
    return c->valide && c->a_seuils && (c->valeur < c->min || c->valeur > c->max);
}

/* Met à jour un capteur après une lecture, réussie ou non */
static void Capteur_Noter(Capteur *c, bool reussie, float valeur)
{
    if (reussie)
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

/* Lit tous les capteurs une fois */
static void Mesures_Lire(void)
{
    BME280_Data env = {0};
    bool env_ok = false;

    /* Si le BME280 n'a pas répondu au démarrage, on retente à chaque mesure */
    if (!bme280_pret)
    {
        bme280_pret = BME280_Init(&bme280, &hi2c1);
    }
    if (bme280_pret)
    {
        env_ok = BME280_Read(&bme280, &env);
    }

    /*
     * Lecture ratée : le capteur a peut-être été débranché, donc éteint.
     * Rebranché, il repart avec ses réglages d'usine (humidité non mesurée) :
     * il faut refaire BME280_Init, ce qui se fera à la prochaine mesure.
     */
    if (!env_ok)
    {
        bme280_pret = false;
    }

    Capteur_Noter(&capteurs[CAPTEUR_TEMPERATURE], env_ok, env.temperature_c);
    Capteur_Noter(&capteurs[CAPTEUR_HUMIDITE],    env_ok, env.humidity_percent);
    Capteur_Noter(&capteurs[CAPTEUR_PRESSION],    env_ok, env.pressure_hpa);

    uint16_t lumiere = GroveLight_ReadRaw();
    Capteur_Noter(&capteurs[CAPTEUR_LUMIERE], lumiere != LUMIERE_ERREUR, (float)lumiere);
}

/* Affiche la mesure sur le port série (plus tard : écriture de la ligne sur la SD) */
static void Mesures_Afficher(Mode mode)
{
    const char *nom_mode = "STANDARD";

    if (mode == MODE_ECONOMIQUE)
    {
        nom_mode = "ECONOMIQUE";
    }
    else if (mode == MODE_MAINTENANCE)
    {
        nom_mode = "MAINTENANCE";
    }

    printf("Mesure (%s) :", nom_mode);

    for (int i = 0; i < NB_CAPTEURS; i++)
    {
        const Capteur *c = &capteurs[i];

        if (c->valide && c->unite[0] == '\0')
        {
            printf("  %s %.0f", c->nom, c->valeur);     /* lumière : valeur brute, sans unité */
        }
        else if (c->valide)
        {
            printf("  %s %.1f %s", c->nom, c->valeur, c->unite);
            if (Capteur_HorsSeuils(c))
            {
                printf(" (hors seuils)");
            }
        }
        else
        {
            printf("  %s NA", c->nom);
            if (c->nb_echecs >= NB_ECHECS_ERREUR)
            {
                printf(" (erreur)");
            }
        }
    }
    printf("\r\n");
}

Erreur Mesures_Erreur(void)
{
    /* Un capteur qui ne répond plus passe avant des données incohérentes */
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

    bme280_pret = BME280_Init(&bme280, &hi2c1);
    printf("BME280 : %s\r\n", bme280_pret ? "pret" : "pas de reponse");
}

void Mesures_MettreAJour(Mode mode)
{
    uint32_t intervalle_ms;

    switch (mode)
    {
        case MODE_STANDARD:
            intervalle_ms = LOG_INTERVAL_DEFAUT * UNITE_INTERVALLE_MS;
            break;

        case MODE_ECONOMIQUE:
            /* Spécification : LOG_INTERVAL multiplié par 2 en mode économique */
            intervalle_ms = 2U * LOG_INTERVAL_DEFAUT * UNITE_INTERVALLE_MS;
            break;

        case MODE_MAINTENANCE:
            /* Spécification : plus d'écriture sur la SD, données en direct sur le port série */
            intervalle_ms = MAINTENANCE_PERIODE_MS;
            break;

        default:
            /* Configuration : acquisition coupée */
            return;
    }

    /* On compare des durées (et pas des heures) : ça reste juste quand HAL_GetTick repasse à 0 */
    if (!premiere_mesure && (HAL_GetTick() - derniere_mesure_ms) < intervalle_ms)
    {
        return;
    }

    premiere_mesure = false;
    derniere_mesure_ms = HAL_GetTick();

    Mesures_Lire();
    Mesures_Afficher(mode);
}
