#include "stm32l4xx_hal.h"
#include "led.h"
#include "grove_rgb.h"

#define PERIODE_CLIGNOTEMENT_MS  1000U  /* fréquence de 1 Hz imposée par la spécification */

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Couleur;

#define ROUGE   { 255,   0,   0 }
#define VERT    {   0, 255,   0 }
#define BLEU    {   0,   0, 255 }
#define JAUNE   { 255, 255,   0 }
#define ORANGE  { 255,  80,   0 }
#define BLANC   { 255, 255, 255 }
#define ETEINTE {   0,   0,   0 }

/* Une erreur = une deuxième couleur après le rouge, et la part de la seconde laissée au rouge */
typedef struct
{
    Couleur seconde;
    uint32_t duree_rouge_ms;
} Clignotement;

/* Indexé par Erreur. Durées égales : 500 ms chacune. 2 fois plus long : 333 ms de rouge, 667 ms de l'autre */
static const Clignotement clignotements[] =
{
    [ERREUR_RTC]                  = { BLEU,  500U },
    [ERREUR_GPS]                  = { JAUNE, 500U },
    [ERREUR_CAPTEUR]              = { VERT,  500U },
    [ERREUR_DONNEES_INCOHERENTES] = { VERT,  333U },
    [ERREUR_SD_PLEINE]            = { BLANC, 500U },
    [ERREUR_SD_ECRITURE]          = { BLANC, 333U },
};

/* Mêmes couleurs que dans modes.c (tableau du livrable 1) */
static const Couleur couleurs_modes[] =
{
    [MODE_STANDARD]      = VERT,
    [MODE_CONFIGURATION] = JAUNE,
    [MODE_ECONOMIQUE]    = BLEU,
    [MODE_MAINTENANCE]   = ORANGE,
};

static Couleur couleur_affichee = ETEINTE;
static Erreur erreur_precedente = ERREUR_AUCUNE;
static Mode mode_precedent = MODE_STANDARD;

/* Envoie la couleur seulement si elle change (l'envoi à la LED prend du temps) */
static void Led_Afficher(Couleur c)
{
    if (c.r != couleur_affichee.r || c.g != couleur_affichee.g || c.b != couleur_affichee.b)
    {
        GroveRGB_SetColor(c.r, c.g, c.b);
        couleur_affichee = c;
    }
}

void Led_MettreAJour(Mode mode, Erreur erreur)
{
    /*
     * modes.c allume lui-même la LED quand le mode change : on ne sait plus
     * ce qui est affiché, donc on oublie la couleur pour forcer le prochain envoi.
     */
    if (mode != mode_precedent)
    {
        couleur_affichee = (Couleur)ETEINTE;
        mode_precedent = mode;
    }

    if (erreur == ERREUR_AUCUNE)
    {
        /* Pas d'erreur : couleur fixe du mode (rien n'est renvoyé si elle est déjà affichée) */
        if (erreur_precedente != ERREUR_AUCUNE)
        {
            Led_Afficher(couleurs_modes[mode]);
        }
        erreur_precedente = ERREUR_AUCUNE;
        return;
    }

    /* Où en est-on dans la seconde ? Début : rouge, fin : l'autre couleur */
    uint32_t instant_ms = HAL_GetTick() % PERIODE_CLIGNOTEMENT_MS;
    const Clignotement *c = &clignotements[erreur];

    if (instant_ms < c->duree_rouge_ms)
    {
        Led_Afficher((Couleur)ROUGE);
    }
    else
    {
        Led_Afficher(c->seconde);
    }

    erreur_precedente = erreur;
}
