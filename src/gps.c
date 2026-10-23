#include <stdlib.h>
#include <string.h>
#include "stm32l4xx_hal.h"
#include "gps.h"
#include "grove_gps_air530z.h"

extern UART_HandleTypeDef GPS_UART_HANDLE;

#define TAILLE_LIGNE        100U
#define NB_CHAMPS_GGA       15

static char ligne_en_cours[TAILLE_LIGNE];
static uint32_t longueur_en_cours = 0U;
static bool dans_une_trame = false;

static volatile char derniere_gga[TAILLE_LIGNE];
static volatile bool gga_recue = false;
static volatile uint32_t dernier_octet_ms = 0U;

static void Gps_RecevoirOctet(char c)
{
    dernier_octet_ms = HAL_GetTick();

    if (c == '$')
    {
        dans_une_trame = true;
        longueur_en_cours = 0U;
    }
    if (!dans_une_trame)
    {
        return;
    }

    if (c == '\r' || c == '\n')
    {
        ligne_en_cours[longueur_en_cours] = '\0';
        dans_une_trame = false;

        if (longueur_en_cours >= 6U && strncmp(&ligne_en_cours[3], "GGA", 3) == 0)
        {
            for (uint32_t i = 0U; i <= longueur_en_cours; i++)
            {
                derniere_gga[i] = ligne_en_cours[i];
            }
            gga_recue = true;
        }
        return;
    }

    if (longueur_en_cours < TAILLE_LIGNE - 1U)
    {
        ligne_en_cours[longueur_en_cours++] = c;
    }
    else
    {
        dans_une_trame = false;
    }
}

void UART4_IRQHandler(void)
{
    USART_TypeDef *uart = GPS_UART_HANDLE.Instance;
    uint32_t etat = uart->ISR;

    if (etat & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE))
    {
        uart->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    }

    if (etat & USART_ISR_RXNE)
    {
        Gps_RecevoirOctet((char)(uart->RDR & 0xFFU));
    }
}

void Gps_Init(void)
{
    GroveGPS_Init();

    __HAL_UART_ENABLE_IT(&GPS_UART_HANDLE, UART_IT_RXNE);
    HAL_NVIC_SetPriority(UART4_IRQn, 3U, 0U);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
}

static bool Gps_SommeControleOk(const char *ligne)
{
    const char *etoile = strchr(ligne, '*');
    if (etoile == NULL)
    {
        return false;
    }

    uint8_t somme = 0U;
    for (const char *p = ligne + 1; p < etoile; p++)
    {
        somme ^= (uint8_t)*p;
    }
    return somme == (uint8_t)strtol(etoile + 1, NULL, 16);
}

static double Gps_EnDegres(const char *valeur, char hemisphere)
{
    double brut = atof(valeur);
    int degres = (int)(brut / 100.0);
    double resultat = degres + (brut - degres * 100.0) / 60.0;

    return (hemisphere == 'S' || hemisphere == 'W') ? -resultat : resultat;
}

EtatGps Gps_Lire(double *latitude, double *longitude, uint32_t silence_max_ms)
{
    char ligne[TAILLE_LIGNE];
    bool recue;

    __disable_irq();
    recue = gga_recue;
    uint32_t silence_ms = HAL_GetTick() - dernier_octet_ms;
    for (uint32_t i = 0U; i < TAILLE_LIGNE; i++)
    {
        ligne[i] = derniere_gga[i];
    }
    __enable_irq();

    if (!recue || silence_ms > silence_max_ms)
    {
        return GPS_MUET;
    }
    if (!Gps_SommeControleOk(ligne))
    {
        return GPS_SANS_POSITION;
    }

    char *champs[NB_CHAMPS_GGA] = { 0 };
    int nb = 0;
    char *p = ligne;

    while (nb < NB_CHAMPS_GGA)
    {
        champs[nb++] = p;
        p = strchr(p, ',');
        if (p == NULL)
        {
            break;
        }
        *p++ = '\0';
    }

    if (nb < 7 || atoi(champs[6]) == 0 || champs[2][0] == '\0' || champs[4][0] == '\0')
    {
        return GPS_SANS_POSITION;
    }

    *latitude = Gps_EnDegres(champs[2], champs[3][0]);
    *longitude = Gps_EnDegres(champs[4], champs[5][0]);
    return GPS_POSITION;
}
