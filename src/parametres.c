#include <stdio.h>
#include <string.h>
#include "stm32l4xx_hal.h"
#include "parametres.h"

/*
 * Le STM32L476 n'a pas d'EEPROM : les paramètres sont sauvegardés dans la
 * dernière page de la mémoire Flash (page 255 de la banque 2, 2 Ko), loin du programme.
 */
#define FLASH_ADRESSE_PARAMETRES  0x080FF800UL
#define FLASH_PAGE_PARAMETRES     255U
#define MAGIE                     0x57575731UL     /* « WWW1 » : la page contient bien nos paramètres */

typedef struct
{
    const char *nom;
    int32_t min;
    int32_t max;
    int32_t defaut;
} Definition;

/* Domaines et valeurs par défaut du tableau de la spécification */
static const Definition definitions[NB_PARAMETRES] =
{
    [P_LOG_INTERVAL]  = { "LOG_INTERVAL",  1,    1440,    10 },
    [P_FILE_MAX_SIZE] = { "FILE_MAX_SIZE", 512,  1048576, 2048 },
    [P_TIMEOUT]       = { "TIMEOUT",       1,    300,     30 },
    [P_LUMIN]         = { "LUMIN",         0,    1,       1 },
    [P_LUMIN_LOW]     = { "LUMIN_LOW",     0,    1023,    255 },
    [P_LUMIN_HIGH]    = { "LUMIN_HIGH",    0,    1023,    768 },
    [P_TEMP_AIR]      = { "TEMP_AIR",      0,    1,       1 },
    [P_MIN_TEMP_AIR]  = { "MIN_TEMP_AIR",  -40,  85,      -10 },
    [P_MAX_TEMP_AIR]  = { "MAX_TEMP_AIR",  -40,  85,      60 },
    [P_HYGR]          = { "HYGR",          0,    1,       1 },
    [P_HYGR_MINT]     = { "HYGR_MINT",     -40,  85,      0 },
    [P_HYGR_MAXT]     = { "HYGR_MAXT",     -40,  85,      50 },
    [P_PRESSURE]      = { "PRESSURE",      0,    1,       1 },
    [P_PRESSURE_MIN]  = { "PRESSURE_MIN",  300,  1100,    850 },
    [P_PRESSURE_MAX]  = { "PRESSURE_MAX",  300,  1100,    1080 },
};

/* Ce qui est écrit en Flash : un nombre pair de mots de 32 bits (la Flash s'écrit par 64 bits) */
typedef struct
{
    uint32_t magie;
    int32_t valeurs[NB_PARAMETRES];
    uint32_t somme;
} Sauvegarde;

#define NB_DOUBLES_MOTS  ((sizeof(Sauvegarde) + 7U) / 8U)

static int32_t valeurs[NB_PARAMETRES];

static uint32_t Parametres_Somme(const int32_t *v)
{
    uint32_t somme = MAGIE;
    for (int i = 0; i < NB_PARAMETRES; i++)
    {
        somme = (somme << 1 | somme >> 31) ^ (uint32_t)v[i];
    }
    return somme;
}

static bool Parametres_Sauvegarder(void)
{
    uint64_t tampon[NB_DOUBLES_MOTS] = { 0 };
    Sauvegarde *s = (Sauvegarde *)tampon;
    FLASH_EraseInitTypeDef effacement = { 0 };
    uint32_t page_en_erreur = 0U;
    bool ok;

    s->magie = MAGIE;
    memcpy(s->valeurs, valeurs, sizeof(valeurs));
    s->somme = Parametres_Somme(valeurs);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    /* La Flash doit être effacée avant d'être réécrite */
    effacement.TypeErase = FLASH_TYPEERASE_PAGES;
    effacement.Banks = FLASH_BANK_2;
    effacement.Page = FLASH_PAGE_PARAMETRES;
    effacement.NbPages = 1U;
    ok = HAL_FLASHEx_Erase(&effacement, &page_en_erreur) == HAL_OK;

    for (uint32_t i = 0U; ok && i < NB_DOUBLES_MOTS; i++)
    {
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_ADRESSE_PARAMETRES + 8U * i, tampon[i]) == HAL_OK;
    }

    HAL_FLASH_Lock();

    if (!ok)
    {
        printf("Erreur : sauvegarde des parametres impossible\r\n");
    }
    return ok;
}

static void Parametres_ValeursParDefaut(void)
{
    for (int i = 0; i < NB_PARAMETRES; i++)
    {
        valeurs[i] = definitions[i].defaut;
    }
}

void Parametres_Init(void)
{
    const Sauvegarde *s = (const Sauvegarde *)FLASH_ADRESSE_PARAMETRES;
    bool valide = (s->magie == MAGIE) && (s->somme == Parametres_Somme(s->valeurs));

    for (int i = 0; valide && i < NB_PARAMETRES; i++)
    {
        valide = s->valeurs[i] >= definitions[i].min && s->valeurs[i] <= definitions[i].max;
    }

    if (valide)
    {
        memcpy(valeurs, s->valeurs, sizeof(valeurs));
        printf("Parametres : charges depuis la Flash\r\n");
    }
    else
    {
        /* Premier démarrage, ou page abîmée : valeurs par défaut */
        Parametres_ValeursParDefaut();
        printf("Parametres : valeurs par defaut\r\n");
    }
}

int32_t Parametre(IdParametre id)
{
    return valeurs[id];
}

bool Parametres_Chercher(const char *nom, IdParametre *id)
{
    /* La spécification écrit aussi « LOG_INTERVALL » : on accepte les deux */
    if (strcmp(nom, "LOG_INTERVALL") == 0)
    {
        *id = P_LOG_INTERVAL;
        return true;
    }

    for (int i = 0; i < NB_PARAMETRES; i++)
    {
        if (strcmp(nom, definitions[i].nom) == 0)
        {
            *id = (IdParametre)i;
            return true;
        }
    }
    return false;
}

const char *Parametres_Nom(IdParametre id)
{
    return definitions[id].nom;
}

bool Parametres_Modifier(IdParametre id, int32_t valeur)
{
    if (valeur < definitions[id].min || valeur > definitions[id].max)
    {
        return false;
    }
    valeurs[id] = valeur;
    return Parametres_Sauvegarder();
}

void Parametres_Domaine(IdParametre id, int32_t *min, int32_t *max)
{
    *min = definitions[id].min;
    *max = definitions[id].max;
}

void Parametres_Reinitialiser(void)
{
    Parametres_ValeursParDefaut();
    (void)Parametres_Sauvegarder();
}
