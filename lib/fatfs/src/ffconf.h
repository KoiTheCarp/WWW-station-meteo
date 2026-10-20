/*
 * Configuration de FatFs R0.12c (bibliothèque de ChaN fournie avec STM32Cube)
 * pour la station météo : une seule carte SD en FAT32, écriture de fichiers texte.
 */
#ifndef FFCONF_H
#define FFCONF_H

#define _FFCONF 68300           /* version de la configuration attendue par ff.c */

/* Fonctions utiles : lecture, écriture, renommage, état d'un fichier */
#define _FS_READONLY    0
#define _FS_MINIMIZE    0
#define _USE_STRFUNC    0
#define _USE_FIND       0
#define _USE_MKFS       0       /* la carte est formatée sur un PC */
#define _USE_FASTSEEK   0
#define _USE_EXPAND     0
#define _USE_CHMOD      0
#define _USE_LABEL      0
#define _USE_FORWARD    0

/*
 * Noms longs activés (tampon statique) : au-delà de la révision 9,
 * « AAMMJJ_10.LOG » ne tient plus dans le format court 8.3.
 */
#define _CODE_PAGE      850
#define _USE_LFN        1
#define _MAX_LFN        32
#define _LFN_UNICODE    0
#define _STRF_ENCODE    0
#define _FS_RPATH       0

/* Un seul volume : la carte SD */
#define _VOLUMES        1
#define _STR_VOLUME_ID  0
#define _VOLUME_STRS    "SD"
#define _MULTI_PARTITION 0
#define _MIN_SS         512
#define _MAX_SS         512
#define _USE_TRIM       0
#define _FS_NOFSINFO    0

/* _FS_TINY : un seul tampon de secteur partagé, pour économiser la RAM */
#define _FS_TINY        1
#define _FS_EXFAT       0

/* Date des fichiers donnée par la RTC (get_fattime dans horloge.c) */
#define _FS_NORTC       0
#define _NORTC_MON      1
#define _NORTC_MDAY     1
#define _NORTC_YEAR     2026

#define _FS_LOCK        0
#define _FS_REENTRANT   0
#define _FS_TIMEOUT     1000
#define _SYNC_t         void *

#endif
