/*
 * Pilote de la carte SD en mode SPI, pour FatFs.
 * FatFs ne sait que lire et écrire des secteurs de 512 octets : ce fichier
 * fournit ces fonctions (disk_initialize, disk_read, disk_write...) en parlant
 * à la carte avec les commandes SD (CMD0, CMD8, ACMD41, CMD17, CMD24...).
 */
#include <stdbool.h>
#include <stddef.h>
#include "stm32l4xx_hal.h"
#include "board_pins.h"
#include "diskio.h"
#include "carte_sd.h"

extern SPI_HandleTypeDef SD_SPI_HANDLE;     /* SPI1, créé dans setup.c */

/* Commandes SD utilisées (ACMD = précédée de CMD55) */
#define CMD0    0U      /* remise à zéro, passage en mode SPI */
#define CMD8    8U      /* tension d'alimentation (cartes SDHC) */
#define CMD12   12U     /* fin d'une lecture de plusieurs secteurs */
#define CMD16   16U     /* taille d'un bloc (cartes anciennes) */
#define CMD17   17U     /* lecture d'un secteur */
#define CMD18   18U     /* lecture de plusieurs secteurs */
#define CMD24   24U     /* écriture d'un secteur */
#define CMD25   25U     /* écriture de plusieurs secteurs */
#define CMD55   55U     /* la commande suivante est une ACMD */
#define CMD58   58U     /* lecture du registre OCR (type de carte) */
#define ACMD41  (0x80U | 41U)   /* démarrage de la carte */

#define JETON_DEBUT_DONNEES     0xFEU   /* avant les 512 octets d'un secteur */
#define JETON_ECRITURE_MULTIPLE 0xFCU
#define JETON_FIN_ECRITURE      0xFDU

#define TIMEOUT_INIT_MS     1000U
#define TIMEOUT_LECTURE_MS  200U
#define TIMEOUT_ECRITURE_MS 500U

static volatile DSTATUS etat = STA_NOINIT;
static bool adressage_par_bloc = false;    /* SDHC : adresse en secteurs, SDSC : en octets */

static uint8_t Sd_Echanger(uint8_t octet)
{
    uint8_t recu = 0xFFU;
    (void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &octet, &recu, 1U, 50U);
    return recu;
}

/*
 * Un secteur entier en un seul appel HAL : octet par octet, le processeur à 4 MHz
 * passait plus de temps dans HAL_SPI_TransmitReceive que dans le transfert
 * (essai du 20/10 : 8 s de blocage pendant un archivage).
 */
static uint8_t octets_ff[512];
static uint8_t octets_ignores[512];

static void Sd_Recevoir(uint8_t *tampon, uint32_t taille)
{
    (void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, octets_ff, tampon, (uint16_t)taille, 100U);
}

static void Sd_Envoyer(const uint8_t *tampon, uint32_t taille)
{
    (void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, (uint8_t *)tampon, octets_ignores, (uint16_t)taille, 100U);
}

/* Vitesse du SPI : lente (< 400 kHz) pour l'initialisation, rapide ensuite */
static void Sd_VitesseSpi(uint32_t prescaler)
{
    __HAL_SPI_DISABLE(&SD_SPI_HANDLE);
    MODIFY_REG(SD_SPI_HANDLE.Instance->CR1, SPI_CR1_BR, prescaler);
    __HAL_SPI_ENABLE(&SD_SPI_HANDLE);
}

/* La carte répond 0xFF quand elle a fini son travail interne */
static bool Sd_AttendrePrete(uint32_t timeout_ms)
{
    uint32_t debut = HAL_GetTick();

    do
    {
        if (Sd_Echanger(0xFFU) == 0xFFU)
        {
            return true;
        }
    }
    while ((HAL_GetTick() - debut) < timeout_ms);

    return false;
}

static void Sd_Deselectionner(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    (void)Sd_Echanger(0xFFU);     /* un octet de plus pour que la carte relâche MISO */
}

static bool Sd_Selectionner(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
    (void)Sd_Echanger(0xFFU);

    if (Sd_AttendrePrete(TIMEOUT_ECRITURE_MS))
    {
        return true;
    }
    Sd_Deselectionner();
    return false;
}

/* Envoie une commande et renvoie la réponse R1 (0 = OK, 1 = en cours d'initialisation) */
static uint8_t Sd_Commande(uint8_t cmd, uint32_t argument)
{
    uint8_t reponse;

    if (cmd & 0x80U)
    {
        cmd &= 0x7FU;
        reponse = Sd_Commande(CMD55, 0U);
        if (reponse > 1U)
        {
            return reponse;
        }
    }

    if (cmd == CMD0)
    {
        /*
         * Carte qu'on vient de remettre : avant CMD0, elle n'est pas encore en mode SPI
         * et peut ne pas répondre 0xFF. On n'attend donc pas qu'elle soit prête
         * (essai du 20/10 : carte remise après la maintenance jamais reconnue).
         */
        Sd_Deselectionner();
        HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
        (void)Sd_Echanger(0xFFU);
    }
    else if (cmd != CMD12)
    {
        Sd_Deselectionner();
        if (!Sd_Selectionner())
        {
            return 0xFFU;
        }
    }

    /* Trame de 6 octets : commande, argument sur 4 octets, CRC (vérifié seulement pour CMD0 et CMD8) */
    uint8_t crc = 0x01U;
    if (cmd == CMD0)
    {
        crc = 0x95U;
    }
    else if (cmd == CMD8)
    {
        crc = 0x87U;
    }

    (void)Sd_Echanger((uint8_t)(0x40U | cmd));
    (void)Sd_Echanger((uint8_t)(argument >> 24));
    (void)Sd_Echanger((uint8_t)(argument >> 16));
    (void)Sd_Echanger((uint8_t)(argument >> 8));
    (void)Sd_Echanger((uint8_t)argument);
    (void)Sd_Echanger(crc);

    if (cmd == CMD12)
    {
        (void)Sd_Echanger(0xFFU);     /* octet de remplissage après CMD12 */
    }

    /* La réponse arrive dans les 10 octets suivants (bit 7 à 0) */
    for (uint8_t i = 0U; i < 10U; i++)
    {
        reponse = Sd_Echanger(0xFFU);
        if ((reponse & 0x80U) == 0U)
        {
            break;
        }
    }
    return reponse;
}

static bool Sd_LireBloc(uint8_t *tampon)
{
    uint32_t debut = HAL_GetTick();
    uint8_t jeton;

    do
    {
        jeton = Sd_Echanger(0xFFU);
    }
    while (jeton == 0xFFU && (HAL_GetTick() - debut) < TIMEOUT_LECTURE_MS);

    if (jeton != JETON_DEBUT_DONNEES)
    {
        return false;
    }

    Sd_Recevoir(tampon, 512U);
    (void)Sd_Echanger(0xFFU);     /* CRC ignoré */
    (void)Sd_Echanger(0xFFU);
    return true;
}

static bool Sd_EcrireBloc(const uint8_t *tampon, uint8_t jeton)
{
    if (!Sd_AttendrePrete(TIMEOUT_ECRITURE_MS))
    {
        return false;
    }

    (void)Sd_Echanger(jeton);
    if (jeton == JETON_FIN_ECRITURE)
    {
        return true;
    }

    Sd_Envoyer(tampon, 512U);
    (void)Sd_Echanger(0xFFU);     /* CRC ignoré */
    (void)Sd_Echanger(0xFFU);

    /* Réponse de la carte : xxx0 0101 = données acceptées */
    return (Sd_Echanger(0xFFU) & 0x1FU) == 0x05U;
}

DSTATUS disk_initialize(BYTE pdrv)
{
    uint8_t ocr[4];

    if (pdrv != 0U)
    {
        return STA_NOINIT;
    }

    for (uint32_t i = 0U; i < sizeof(octets_ff); i++)
    {
        octets_ff[i] = 0xFFU;     /* en lecture, la carte attend des 0xFF en face de chaque octet */
    }

    Sd_VitesseSpi(SPI_BAUDRATEPRESCALER_256);   /* 4 MHz / 256 = 15,6 kHz */

    /* Au moins 74 coups d'horloge avec CS haut pour réveiller la carte */
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    for (uint8_t i = 0U; i < 10U; i++)
    {
        (void)Sd_Echanger(0xFFU);
    }

    etat = STA_NOINIT;
    adressage_par_bloc = false;

    /* Certaines cartes ne répondent pas au premier CMD0 : on réessaie (comme sd_logger.c du dépôt) */
    uint8_t reponse = 0xFFU;
    for (uint8_t essai = 0U; essai < 10U && reponse != 1U; essai++)
    {
        reponse = Sd_Commande(CMD0, 0U);
    }

    if (reponse == 1U)
    {
        uint32_t debut = HAL_GetTick();

        if (Sd_Commande(CMD8, 0x1AAU) == 1U)
        {
            /* Carte SD version 2 (SDHC ou SDSC récente) */
            Sd_Recevoir(ocr, 4U);
            if (ocr[2] == 0x01U && ocr[3] == 0xAAU)
            {
                while ((HAL_GetTick() - debut) < TIMEOUT_INIT_MS && Sd_Commande(ACMD41, 1UL << 30) != 0U)
                {
                }
                if ((HAL_GetTick() - debut) < TIMEOUT_INIT_MS && Sd_Commande(CMD58, 0U) == 0U)
                {
                    Sd_Recevoir(ocr, 4U);
                    adressage_par_bloc = (ocr[0] & 0x40U) != 0U;   /* bit CCS : carte SDHC */
                    etat = 0U;
                }
            }
        }
        else
        {
            /* Carte SD version 1 : blocs de 512 octets à imposer */
            while ((HAL_GetTick() - debut) < TIMEOUT_INIT_MS && Sd_Commande(ACMD41, 0U) != 0U)
            {
            }
            if ((HAL_GetTick() - debut) < TIMEOUT_INIT_MS && Sd_Commande(CMD16, 512U) == 0U)
            {
                etat = 0U;
            }
        }
    }

    Sd_Deselectionner();

    if (etat == 0U)
    {
        Sd_VitesseSpi(SPI_BAUDRATEPRESCALER_2);  /* 2 MHz : un secteur en environ 2 ms */
    }
    return etat;
}

DSTATUS disk_status(BYTE pdrv)
{
    return (pdrv == 0U) ? etat : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0U || count == 0U)
    {
        return RES_PARERR;
    }
    if (etat & STA_NOINIT)
    {
        return RES_NOTRDY;
    }
    if (!adressage_par_bloc)
    {
        sector *= 512U;
    }

    if (count == 1U)
    {
        if (Sd_Commande(CMD17, sector) == 0U && Sd_LireBloc(buff))
        {
            count = 0U;
        }
    }
    else if (Sd_Commande(CMD18, sector) == 0U)
    {
        do
        {
            if (!Sd_LireBloc(buff))
            {
                break;
            }
            buff += 512U;
        }
        while (--count > 0U);
        (void)Sd_Commande(CMD12, 0U);
    }

    Sd_Deselectionner();
    return (count == 0U) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv != 0U || count == 0U)
    {
        return RES_PARERR;
    }
    if (etat & STA_NOINIT)
    {
        return RES_NOTRDY;
    }
    if (!adressage_par_bloc)
    {
        sector *= 512U;
    }

    if (count == 1U)
    {
        if (Sd_Commande(CMD24, sector) == 0U && Sd_EcrireBloc(buff, JETON_DEBUT_DONNEES))
        {
            count = 0U;
        }
    }
    else if (Sd_Commande(CMD25, sector) == 0U)
    {
        do
        {
            if (!Sd_EcrireBloc(buff, JETON_ECRITURE_MULTIPLE))
            {
                break;
            }
            buff += 512U;
        }
        while (--count > 0U);

        if (!Sd_EcrireBloc(NULL, JETON_FIN_ECRITURE))
        {
            count = 1U;
        }
    }

    Sd_Deselectionner();
    return (count == 0U) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)buff;

    if (pdrv != 0U)
    {
        return RES_PARERR;
    }
    if (etat & STA_NOINIT)
    {
        return RES_NOTRDY;
    }

    if (cmd == CTRL_SYNC)
    {
        /* Attendre que la carte ait fini d'écrire avant de dire que c'est fait */
        bool prete = Sd_Selectionner();
        Sd_Deselectionner();
        return prete ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;
}

/* Oublie la carte : elle peut être retirée, et sera réinitialisée au prochain montage */
void CarteSd_Oublier(void)
{
    etat = STA_NOINIT;
}
