#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stockage.h"
#include "carte_sd.h"
#include "ff.h"

#define REVISION_MAX  9999U     /* au-delà, la carte est considérée comme pleine pour ce jour */

static FATFS systeme_fichiers;
static bool montee = false;             /* système de fichiers monté et utilisable */
static Erreur erreur = ERREUR_AUCUNE;

/* Préfixe AAMMJJ du dernier fichier utilisé (si la RTC ne répond plus, on continue dedans) */
static char jour_courant[7] = "000000";

/* Prochaine révision libre (_1, _2...), connue pour le jour jour_revision */
static char jour_revision[7] = "";
static uint32_t revision_suivante = 0U;

bool Stockage_Demarrer(void)
{
    CarteSd_Oublier();      /* force la réinitialisation : la carte a peut-être été changée */
    jour_revision[0] = '\0';  /* et ses fichiers aussi : on relira le répertoire avant d'archiver */

    montee = (f_mount(&systeme_fichiers, "", 1) == FR_OK);
    erreur = montee ? ERREUR_AUCUNE : ERREUR_SD_ECRITURE;

    printf("Carte SD : %s\r\n", montee ? "prete (FAT32)" : "absente ou illisible");
    return montee;
}

void Stockage_Arreter(void)
{
    /*
     * Chaque écriture ouvre puis ferme le fichier : rien n'est en cours entre deux mesures.
     * On démonte quand même pour ne plus jamais toucher à la carte en maintenance.
     */
    if (montee)
    {
        (void)f_mount(NULL, "", 0);
        montee = false;
    }
    CarteSd_Oublier();
    erreur = ERREUR_AUCUNE;
    printf("Carte SD : ecritures arretees, retrait possible\r\n");
}

/* Traduit un code FatFs en erreur LED : plus de place, ou problème d'accès */
static void Stockage_NoterErreur(FRESULT resultat)
{
    if (resultat == FR_DENIED)
    {
        erreur = ERREUR_SD_PLEINE;      /* plus de cluster libre ou répertoire plein */
    }
    else
    {
        erreur = ERREUR_SD_ECRITURE;
        montee = false;                 /* carte retirée ou abîmée : on la remontera à la prochaine mesure */
    }
}

/*
 * Le fichier _0 est plein : il devient la première révision libre (_1, _2...),
 * puis un nouveau _0 vide est créé. Renommer évite de recopier tout le fichier.
 */
/*
 * Cherche la plus grande révision déjà sur la carte pour ce jour, en lisant le répertoire
 * une seule fois. Essayer _1, _2, _3... un par un relisait tout le répertoire à chaque
 * essai : avec 38 archives, la boucle principale restait bloquée 8 s (essai du 20/10).
 */
static FRESULT Stockage_ChercherRevision(void)
{
    DIR repertoire;
    FILINFO info;
    size_t longueur_jour = strlen(jour_courant);
    uint32_t plus_grande = 0U;
    FRESULT resultat = f_opendir(&repertoire, "");

    while (resultat == FR_OK)
    {
        resultat = f_readdir(&repertoire, &info);
        if (resultat != FR_OK || info.fname[0] == '\0')
        {
            break;      /* fin du répertoire */
        }
        /* Nom de la forme AAMMJJ_N.LOG pour le jour courant ? */
        if (strncmp(info.fname, jour_courant, longueur_jour) == 0 && info.fname[longueur_jour] == '_')
        {
            uint32_t revision = (uint32_t)strtoul(&info.fname[longueur_jour + 1U], NULL, 10);
            if (revision > plus_grande)
            {
                plus_grande = revision;
            }
        }
    }
    (void)f_closedir(&repertoire);

    if (resultat == FR_OK)
    {
        strcpy(jour_revision, jour_courant);
        revision_suivante = plus_grande + 1U;
    }
    return resultat;
}

static FRESULT Stockage_Archiver(const char *nom_courant)
{
    char nom_archive[20];

    /* Nouveau jour, ou première archive depuis le démarrage : on regarde ce qui existe déjà */
    if (strcmp(jour_revision, jour_courant) != 0)
    {
        FRESULT resultat = Stockage_ChercherRevision();
        if (resultat != FR_OK)
        {
            return resultat;
        }
    }
    if (revision_suivante > REVISION_MAX)
    {
        return FR_DENIED;
    }

    snprintf(nom_archive, sizeof(nom_archive), "%s_%lu.LOG", jour_courant, (unsigned long)revision_suivante);
    printf("Carte SD : %s plein, archive en %s\r\n", nom_courant, nom_archive);

    FRESULT resultat = f_rename(nom_courant, nom_archive);
    if (resultat == FR_OK)
    {
        revision_suivante++;
    }
    return resultat;
}

/* Écrit tout le texte, ou signale la carte pleine si elle n'a pas pu tout prendre */
static FRESULT Stockage_Ecrire(FIL *fichier, const char *texte)
{
    UINT ecrits = 0U;
    UINT longueur = (UINT)strlen(texte);
    FRESULT resultat = f_write(fichier, texte, longueur, &ecrits);

    if (resultat == FR_OK && ecrits < longueur)
    {
        resultat = FR_DENIED;
    }
    return resultat;
}

void Stockage_EcrireLigne(const RTC_DateTime *date, bool date_valide,
                          const char *entete, const char *ligne, uint32_t taille_max)
{
    char nom[20];
    FIL fichier;
    FRESULT resultat;

    /* Carte retirée puis remise (ou erreur précédente) : on retente de la monter */
    if (!montee && !Stockage_Demarrer())
    {
        return;
    }

    if (date_valide)
    {
        snprintf(jour_courant, sizeof(jour_courant), "%02u%02u%02u",
                 (unsigned)(date->year % 100U), (unsigned)(date->month % 100U), (unsigned)(date->day % 100U));
    }
    snprintf(nom, sizeof(nom), "%s_0.LOG", jour_courant);

    resultat = f_open(&fichier, nom, FA_OPEN_APPEND | FA_WRITE);

    /* Fichier plein : on l'archive et on repart d'un fichier vide */
    if (resultat == FR_OK && f_size(&fichier) > 0U
        && f_size(&fichier) + strlen(ligne) > taille_max)
    {
        (void)f_close(&fichier);
        resultat = Stockage_Archiver(nom);
        if (resultat == FR_OK)
        {
            resultat = f_open(&fichier, nom, FA_CREATE_NEW | FA_WRITE);
        }
    }

    if (resultat == FR_OK && f_size(&fichier) == 0U)
    {
        resultat = Stockage_Ecrire(&fichier, entete);
    }
    if (resultat == FR_OK)
    {
        resultat = Stockage_Ecrire(&fichier, ligne);
    }

    /* f_close vide le tampon sur la carte : après, la ligne est vraiment enregistrée */
    FRESULT fermeture = f_close(&fichier);
    if (resultat == FR_OK)
    {
        resultat = fermeture;
    }

    if (resultat == FR_OK)
    {
        erreur = ERREUR_AUCUNE;
    }
    else
    {
        printf("Carte SD : erreur %d sur %s\r\n", (int)resultat, nom);
        Stockage_NoterErreur(resultat);
    }
}

Erreur Stockage_Erreur(void)
{
    return erreur;
}
