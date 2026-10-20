#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "stm32l4xx_hal.h"
#include "console.h"
#include "parametres.h"
#include "horloge.h"

extern UART_HandleTypeDef huart2;   /* port série du ST-LINK, créé dans setup.c */

#define VERSION_PROGRAMME  "1.0"
#define NUMERO_LOT         "WWW-2026-01"

/* Numéro de build : donné par le script de compilation (-DNUMERO_BUILD=...), sinon 0 */
#ifndef NUMERO_BUILD
#define NUMERO_BUILD 0
#endif

#define TAILLE_FILE     64U     /* caractères reçus en attente de traitement */
#define TAILLE_COMMANDE 40U

/* File circulaire : l'interruption écrit au début, la boucle principale lit à la fin */
static volatile char file[TAILLE_FILE];
static volatile uint32_t tete = 0U;
static volatile uint32_t queue = 0U;

static char commande[TAILLE_COMMANDE];
static uint32_t longueur = 0U;
static Mode mode_precedent = MODE_STANDARD;

/* Interruption de l'USART2 : un caractère a été tapé dans la console */
void USART2_IRQHandler(void)
{
    USART_TypeDef *uart = huart2.Instance;
    uint32_t etat = uart->ISR;

    if (etat & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE))
    {
        uart->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    }

    if (etat & USART_ISR_RXNE)
    {
        char c = (char)(uart->RDR & 0xFFU);
        uint32_t suivant = (tete + 1U) % TAILLE_FILE;

        if (suivant != queue)       /* file pleine : le caractère est perdu */
        {
            file[tete] = c;
            tete = suivant;
        }
    }
}

void Console_Init(void)
{
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART2_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void Console_Aide(void)
{
    printf("Commandes : NOM=VALEUR pour regler, NOM seul pour lire la valeur\r\n");
    for (int i = 0; i < NB_PARAMETRES; i++)
    {
        int32_t min, max;
        Parametres_Domaine((IdParametre)i, &min, &max);
        printf("  %-14s = %ld  (%ld a %ld)\r\n", Parametres_Nom((IdParametre)i),
               (long)Parametre((IdParametre)i), (long)min, (long)max);
    }
    printf("  CLOCK=HH:MM:SS   DATE=MM,JJ,AAAA   DAY=MON..SUN\r\n");
    printf("  RESET   VERSION   HELP\r\n");
}

/* Lit un entier qui doit occuper tout le texte (« 12a » est refusé) */
static bool Console_LireEntier(const char *texte, int32_t *valeur)
{
    char *fin;

    if (*texte == '\0')
    {
        return false;
    }
    long v = strtol(texte, &fin, 10);
    if (*fin != '\0')
    {
        return false;
    }
    *valeur = (int32_t)v;
    return true;
}

static void Console_Clock(const char *valeur)
{
    unsigned h, m, s;
    char reste;

    if (sscanf(valeur, "%u:%u:%u%c", &h, &m, &s, &reste) != 3 || h > 23U || m > 59U || s > 59U)
    {
        printf("Erreur : format CLOCK=HEURE{0-23}:MINUTE{0-59}:SECONDE{0-59}\r\n");
        return;
    }
    if (Horloge_RegleHeure((uint8_t)h, (uint8_t)m, (uint8_t)s))
    {
        printf("Heure reglee : %02u:%02u:%02u\r\n", h, m, s);
    }
    else
    {
        printf("Erreur : la RTC ne repond pas\r\n");
    }
}

static void Console_Date(const char *valeur)
{
    unsigned mois, jour, annee;
    char reste;

    if (sscanf(valeur, "%u,%u,%u%c", &mois, &jour, &annee, &reste) != 3
        || mois < 1U || mois > 12U || jour < 1U || jour > 31U || annee < 2000U || annee > 2099U)
    {
        printf("Erreur : format DATE=MOIS{1-12},JOUR{1-31},ANNEE{2000-2099}\r\n");
        return;
    }
    if (Horloge_RegleDate((uint8_t)mois, (uint8_t)jour, (uint16_t)annee))
    {
        printf("Date reglee : %02u/%02u/%04u\r\n", jour, mois, annee);
    }
    else
    {
        printf("Erreur : la RTC ne repond pas\r\n");
    }
}

static void Console_Day(const char *valeur)
{
    static const char *const jours[] = { "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };

    for (uint8_t i = 0U; i < 7U; i++)
    {
        if (strcmp(valeur, jours[i]) == 0)
        {
            if (Horloge_RegleJour((uint8_t)(i + 1U)))
            {
                printf("Jour regle : %s\r\n", jours[i]);
            }
            else
            {
                printf("Erreur : la RTC ne repond pas\r\n");
            }
            return;
        }
    }
    printf("Erreur : format DAY={MON,TUE,WED,THU,FRI,SAT,SUN}\r\n");
}

static void Console_Parametre(const char *nom, const char *valeur)
{
    IdParametre id;
    int32_t nombre;

    if (!Parametres_Chercher(nom, &id))
    {
        printf("Erreur : commande inconnue \"%s\" (HELP pour la liste)\r\n", nom);
        return;
    }

    /* Nom seul : on affiche la valeur actuelle */
    if (valeur == NULL)
    {
        printf("%s = %ld\r\n", Parametres_Nom(id), (long)Parametre(id));
        return;
    }

    /* Livrable 1 : valeur hors domaine = message d'erreur, rien n'est modifié */
    if (!Console_LireEntier(valeur, &nombre) || !Parametres_Modifier(id, nombre))
    {
        int32_t min, max;
        Parametres_Domaine(id, &min, &max);
        printf("Erreur : %s doit etre un entier de %ld a %ld\r\n", Parametres_Nom(id), (long)min, (long)max);
        return;
    }
    printf("%s = %ld (sauvegarde)\r\n", Parametres_Nom(id), (long)nombre);
}

static void Console_Executer(char *texte)
{
    /* Majuscules : « log_interval=5 » est accepté aussi */
    for (char *p = texte; *p != '\0'; p++)
    {
        *p = (char)toupper((unsigned char)*p);
    }

    char *egal = strchr(texte, '=');
    char *valeur = NULL;
    if (egal != NULL)
    {
        *egal = '\0';
        valeur = egal + 1;
    }

    if (strcmp(texte, "RESET") == 0 && valeur == NULL)
    {
        Parametres_Reinitialiser();
        printf("Parametres remis aux valeurs par defaut\r\n");
    }
    else if (strcmp(texte, "VERSION") == 0 && valeur == NULL)
    {
        printf("Station WWW version %s, build %d, lot %s\r\n", VERSION_PROGRAMME, NUMERO_BUILD, NUMERO_LOT);
    }
    else if ((strcmp(texte, "HELP") == 0 || strcmp(texte, "AIDE") == 0) && valeur == NULL)
    {
        Console_Aide();
    }
    else if (strcmp(texte, "CLOCK") == 0 && valeur != NULL)
    {
        Console_Clock(valeur);
    }
    else if (strcmp(texte, "DATE") == 0 && valeur != NULL)
    {
        Console_Date(valeur);
    }
    else if (strcmp(texte, "DAY") == 0 && valeur != NULL)
    {
        Console_Day(valeur);
    }
    else
    {
        Console_Parametre(texte, valeur);
    }
}

void Console_MettreAJour(Mode mode)
{
    bool affichage = false;

    if (mode == MODE_CONFIGURATION && mode_precedent != MODE_CONFIGURATION)
    {
        printf("Configuration : tapez HELP pour la liste des commandes\r\n> ");
        affichage = true;
    }
    mode_precedent = mode;

    while (queue != tete)
    {
        affichage = true;
        char c = file[queue];
        queue = (queue + 1U) % TAILLE_FILE;

        /* Hors configuration, on ne fait rien de ce qui est tapé */
        if (mode != MODE_CONFIGURATION)
        {
            continue;
        }

        /* Toute touche compte comme une activité : le compte des 30 min repart à zéro */
        Modes_SignalerActivite();

        if (c == '\r' || c == '\n')
        {
            if (longueur > 0U)
            {
                printf("\r\n");
                commande[longueur] = '\0';
                longueur = 0U;
                Console_Executer(commande);
                printf("> ");
            }
        }
        else if ((c == '\b' || c == 0x7F) && longueur > 0U)
        {
            longueur--;
            printf("\b \b");        /* efface le caractère à l'écran */
        }
        else if (isprint((unsigned char)c) && c != ' ' && longueur < TAILLE_COMMANDE - 1U)
        {
            commande[longueur++] = c;
            printf("%c", c);        /* écho : la console n'affiche pas ce qu'on tape */
        }
    }

    /* printf garde le texte en mémoire jusqu'à la fin de ligne : on force l'affichage de l'écho */
    if (affichage)
    {
        fflush(stdout);
    }
}
