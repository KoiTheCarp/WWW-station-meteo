#include <stdio.h>
#include "stm32l4xx_hal.h"
#include "mesures.h"
#include "bme280.h"
#include "grove_light.h"
#include "gps.h"
#include "horloge.h"
#include "parametres.h"
#include "board_pins.h"

extern I2C_HandleTypeDef hi2c1;     /* bus I2C créé dans setup.c */

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

#define LUMIERE_ERREUR  0xFFFFU     /* valeur renvoyée par GroveLight_ReadRaw si la lecture rate */

/*
 * Capteur de lumière débranché : l'entrée A2 « flotte » et suit la dernière tension
 * qu'on lui a imposée. On mesure donc deux fois : après l'avoir mise à 0 V, puis à 3,3 V.
 * Un capteur branché impose sa propre tension : les deux mesures sont proches.
 * Débranché, elles s'écartent (essai du 20/10 : une seule mesure donnait ~900, pas 0).
 */
#define LUMIERE_ECART_DEBRANCHE  400U

/* Tout ce qu'il faut savoir sur un capteur, regroupé dans une structure */
typedef struct
{
    const char *nom;
    IdParametre parametre_actif;    /* LUMIN, TEMP_AIR, HYGR ou PRESSURE : 0 = capteur désactivé */
    bool actif;
    float valeur;           /* dernière valeur lue */
    bool valide;            /* la dernière lecture a-t-elle réussi ? sinon on écrit NA */
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

/* Les seuils sont recopiés depuis les paramètres à chaque mesure (ils changent en configuration) */
static Capteur capteurs[NB_CAPTEURS] =
{
    { "Temperature", P_TEMP_AIR, true, 0.0f, false, 0U, true,  0.0f, 0.0f },
    { "Humidite",    P_HYGR,     true, 0.0f, false, 0U, false, 0.0f, 0.0f },
    { "Pression",    P_PRESSURE, true, 0.0f, false, 0U, true,  0.0f, 0.0f },
    { "Lumiere",     P_LUMIN,    true, 0.0f, false, 0U, false, 0.0f, 0.0f },
};

static BME280_Handle bme280;

/* Le GPS donne deux valeurs (latitude, longitude) : il a sa propre structure */
typedef struct
{
    EtatGps etat;           /* résultat de la dernière lecture */
    bool connue;            /* a-t-on déjà eu une position ? (réutilisée en mode économique) */
    double latitude;
    double longitude;
    uint8_t nb_echecs;      /* lectures d'affilée où le module était muet */
} PositionGps;

static PositionGps position = { GPS_MUET, false, 0.0, 0.0, 0U };
static uint32_t nb_mesures_eco = 0U;   /* pour ne lire le GPS qu'une mesure sur deux en mode économique */

static RTC_DateTime date_mesure;
static bool date_valide = false;

static uint32_t derniere_mesure_ms = 0U;
static bool premiere_mesure = true;     /* la toute première mesure se fait tout de suite */
static Mode mode_precedent = MODE_CONFIGURATION;

static char ligne[160];

static const char ENTETE[] = "DATE,HEURE,TEMP_AIR_C,HYGR_PCT,PRESSION_HPA,LUMIERE,NIVEAU_LUMIERE,LATITUDE,LONGITUDE\r\n";

/* Valeur lue mais en dehors des seuils : données incohérentes */
static bool Capteur_HorsSeuils(const Capteur *c)
{
    return c->actif && c->valide && c->a_seuils && (c->valeur < c->min || c->valeur > c->max);
}

/* Met à jour un capteur après une lecture, réussie ou non */
static void Capteur_Noter(Capteur *c, bool reussie, float valeur)
{
    if (!c->actif)
    {
        c->valide = false;      /* désactivé en configuration : jamais en erreur */
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

/* Recopie les paramètres réglés en configuration dans les capteurs */
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

/* Lit la position, sauf une mesure sur deux en mode économique (spécification) */
static void Mesures_LireGps(Mode mode)
{
    if (mode == MODE_ECONOMIQUE)
    {
        nb_mesures_eco++;
        if (nb_mesures_eco % 2U == 0U)
        {
            return;     /* livrable 1 : on garde la dernière position connue */
        }
    }
    else
    {
        nb_mesures_eco = 0U;   /* en entrant en mode éco, la première mesure lit le GPS */
    }

    double latitude, longitude;
    position.etat = Gps_Lire(&latitude, &longitude, (uint32_t)Parametre(P_TIMEOUT) * 1000U);

    if (position.etat == GPS_POSITION)
    {
        position.latitude = latitude;
        position.longitude = longitude;
        position.connue = true;
    }

    /*
     * Choix du groupe : seul un GPS muet compte comme une erreur.
     * Sans position (à l'intérieur), on écrit NA sans faire clignoter la LED.
     */
    if (position.etat == GPS_MUET)
    {
        if (position.nb_echecs < 255U)
        {
            position.nb_echecs++;
        }
    }
    else
    {
        position.nb_echecs = 0U;
    }
}

/* Mesure la lumière juste après avoir imposé une tension à la broche A2 */
static uint16_t Mesures_LireLumiereApres(GPIO_PinState tension)
{
    GPIO_InitTypeDef gpio = {0};

    /* Broche en sortie pendant quelques microsecondes : on lui impose 0 V ou 3,3 V */
    gpio.Pin = LIGHT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(LIGHT_GPIO_PORT, LIGHT_GPIO_PIN, tension);
    HAL_GPIO_Init(LIGHT_GPIO_PORT, &gpio);
    for (volatile uint32_t i = 0U; i < 50U; i++)
    {
    }

    /* Retour en entrée analogique, comme dans GroveLight_Init, puis mesure */
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
        return LUMIERE_ERREUR;      /* la broche suit la tension imposée : rien n'y est branché */
    }
    return (uint16_t)((apres_0v + apres_3v3) / 2U);
}

/* Lit tous les capteurs une fois */
static void Mesures_Lire(Mode mode)
{
    BME280_Data env = {0};
    bool env_ok = false;
    bool bme280_utile = capteurs[CAPTEUR_TEMPERATURE].actif || capteurs[CAPTEUR_HUMIDITE].actif
                     || capteurs[CAPTEUR_PRESSION].actif;

    date_valide = Horloge_Lire(&date_mesure);

    if (bme280_utile)
    {
        /*
         * BME280_Init avant chaque mesure (environ 15 ms) : si le capteur a été
         * débranché ou a subi une baisse de tension (carte SD retirée), il repart
         * avec ses réglages d'usine et l'humidité reste bloquée (essai du 20/10 :
         * 92,6 % figé). Le réinitialiser à chaque fois évite ce cas.
         */
        if (BME280_Init(&bme280, &hi2c1))
        {
            env_ok = BME280_Read(&bme280, &env);
        }
    }

    Capteur_Noter(&capteurs[CAPTEUR_TEMPERATURE], env_ok, env.temperature_c);
    Capteur_Noter(&capteurs[CAPTEUR_HUMIDITE],    env_ok, env.humidity_percent);
    Capteur_Noter(&capteurs[CAPTEUR_PRESSION],    env_ok, env.pressure_hpa);

    uint16_t lumiere = capteurs[CAPTEUR_LUMIERE].actif ? Mesures_LireLumiere() : LUMIERE_ERREUR;
    Capteur_Noter(&capteurs[CAPTEUR_LUMIERE], lumiere != LUMIERE_ERREUR, (float)lumiere);

    Mesures_LireGps(mode);
}

/* Ajoute une colonne : OFF si le capteur est désactivé, NA s'il n'a pas répondu */
static int Mesures_Colonne(char *p, size_t reste, const Capteur *c, const char *format, bool pris_en_compte)
{
    if (!c->actif)
    {
        return snprintf(p, reste, ",OFF");
    }
    if (!c->valide || !pris_en_compte)
    {
        return snprintf(p, reste, ",NA");
    }
    return snprintf(p, reste, format, c->valeur);
}

/* Lumière brute du convertisseur (0 à 4095) ramenée sur 0 à 1023 comme LUMIN_LOW et LUMIN_HIGH */
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

/* Met la mesure en forme : une ligne CSV horodatée, une valeur par colonne (livrable 1) */
static void Mesures_FormaterLigne(void)
{
    char *p = ligne;
    size_t reste = sizeof(ligne);
    int n;

    if (date_valide)
    {
        n = snprintf(p, reste, "%04u-%02u-%02u,%02u:%02u:%02u",
                     (unsigned)date_mesure.year, (unsigned)date_mesure.month, (unsigned)date_mesure.day,
                     (unsigned)date_mesure.hours, (unsigned)date_mesure.minutes, (unsigned)date_mesure.seconds);
    }
    else
    {
        n = snprintf(p, reste, "NA,NA");
    }
    p += n;
    reste -= (size_t)n;

    /* Spécification : hors de HYGR_MINT..HYGR_MAXT, l'humidité n'est pas prise en compte */
    const Capteur *t = &capteurs[CAPTEUR_TEMPERATURE];
    bool humidite_ok = !t->valide
                    || (t->valeur >= (float)Parametre(P_HYGR_MINT) && t->valeur <= (float)Parametre(P_HYGR_MAXT));

    n = Mesures_Colonne(p, reste, &capteurs[CAPTEUR_TEMPERATURE], ",%.1f", true);
    p += n; reste -= (size_t)n;
    n = Mesures_Colonne(p, reste, &capteurs[CAPTEUR_HUMIDITE], ",%.1f", humidite_ok);
    p += n; reste -= (size_t)n;
    n = Mesures_Colonne(p, reste, &capteurs[CAPTEUR_PRESSION], ",%.1f", true);
    p += n; reste -= (size_t)n;
    n = Mesures_Colonne(p, reste, &capteurs[CAPTEUR_LUMIERE], ",%.0f", true);
    p += n; reste -= (size_t)n;
    n = snprintf(p, reste, ",%s", Mesures_NiveauLumiere(&capteurs[CAPTEUR_LUMIERE]));
    p += n; reste -= (size_t)n;

    /* En mode économique, une mesure sur deux réutilise la dernière position connue */
    if (position.connue && position.etat != GPS_MUET)
    {
        snprintf(p, reste, ",%.5f,%.5f\r\n", position.latitude, position.longitude);
    }
    else
    {
        snprintf(p, reste, ",NA,NA\r\n");
    }
}

Erreur Mesures_Erreur(void)
{
    /* Ordre du tableau Erreur : GPS, puis capteur qui ne répond plus, puis données incohérentes */
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
            /* Spécification : LOG_INTERVAL multiplié par 2 en mode économique */
            intervalle_ms = 2U * log_interval * UNITE_INTERVALLE_MS;
            break;

        case MODE_MAINTENANCE:
            /* Spécification : plus d'écriture sur la SD, données en direct sur le port série */
            intervalle_ms = MAINTENANCE_PERIODE_MS;
            break;

        default:
            /* Configuration : acquisition coupée */
            mode_precedent = mode;
            return false;
    }

    /* En sortant de la configuration, on mesure tout de suite avec les nouveaux réglages */
    if (mode_precedent == MODE_CONFIGURATION)
    {
        premiere_mesure = true;
    }
    mode_precedent = mode;

    /* On compare des durées (et pas des heures) : ça reste juste quand HAL_GetTick repasse à 0 */
    if (!premiere_mesure && (HAL_GetTick() - derniere_mesure_ms) < intervalle_ms)
    {
        return false;
    }

    premiere_mesure = false;
    derniere_mesure_ms = HAL_GetTick();

    Mesures_AppliquerParametres();
    Mesures_Lire(mode);
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
