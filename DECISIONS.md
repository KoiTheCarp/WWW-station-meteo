# Décisions techniques

Ce fichier liste les choix techniques faits pendant le développement qui ne figurent pas dans le livrable 1 (Analyse du système, rendu le 01/10/2026). Pour chaque choix : ce qu'on a décidé, pourquoi, et où le trouver dans le code.

Les choix déjà expliqués dans le livrable 1 n'y sont pas répétés : format CSV, maintenance accessible seulement depuis le standard, détection des appuis par interruption, noms de fichiers `AAMMJJ_N.LOG`, boucle sur les capteurs activés, dernière position GPS réutilisée en mode économique, carte SD relue à la sortie de la maintenance.

## 1. Écarts avec le livrable 1

Ces points s'éloignent de ce que disent les diagrammes. Il faut les justifier dans la documentation (L4) et à l'oral.

- **Pas de Timer pour LOG_INTERVAL.** Le livrable 1 prévoit un Timer qui envoie une interruption à chaque mesure. Le code compare à chaque tour de boucle le temps écoulé (`HAL_GetTick()`) avec l'intervalle. Pourquoi : même résultat sans périphérique en plus, et la boucle tourne de toute façon en continu pour surveiller les boutons. Le prosit 6 ne l'impose pas (il porte sur le bouton par interruption). Évolution possible : un timer pour mettre la carte en veille entre deux mesures. Où : `Mesures_MettreAJour()` dans `src/mesures.c`.
- **Paramètres en mémoire Flash, pas en EEPROM.** Le STM32L476 n'a pas d'EEPROM. Les paramètres sont sauvegardés dans la dernière page de la Flash (page 255 de la banque 2, adresse 0x080FF800, loin du programme), avec un mot magique et une somme de contrôle. Une page vide ou abîmée redonne les valeurs par défaut. Où : `src/parametres.c`.

## 2. Matériel et montage

- **Capteur d'environnement BME280, pas BME680.** Le capteur du montage répond avec l'identifiant 0x60 à l'adresse 0x76 : c'est un BME280. Le pilote BME680 du dépôt CESI attend l'identifiant 0x61 et renvoie des valeurs fixes (20 °C, 50 %, 1013,25 hPa) sans rien lire. On utilise le pilote BME280 fourni par le professeur, en mode forcé (environ 50 ms d'attente par mesure). Où : `lib/cesi_grove/src/bme280.c`. Vérifié sur la carte le 15/10.
- **Boutons.** Rouge = BTN1 (PA10), vert = BTN2 (PB3). Les boutons Grove sont actifs à l'état bas (0 = enfoncé) : résistance de tirage interne vers le haut. Vérifié sur la carte le 14/10. Où : `src/boutons.c`.
- **Shield Grove réglé sur 3V3.** En 5 V, la LED RGB reçoit des données brouillées, et le capteur de lumière envoie plus de 3,3 V sur A2 (la mesure reste bloquée à 4095 et l'entrée PA4 ne supporte pas plus de 3,6 V). LED branchée sur le port D6, lumière sur A2.
- **RTC alimentée à part en 5 V.** La DS1307 ne fonctionne pas en 3,3 V. Elle n'est donc pas branchée sur une prise Grove du shield, mais par un câble Grove-Dupont : VCC sur la broche 5V, GND sur GND, SCL et SDA sur les broches SCL et SDA (D15, D14), qui supportent le 5 V. Vérifié le 20/10.
- **Carte SD par 3 fils Dupont.** Le SD Card Shield fait passer le SPI par le connecteur ISP, absent sur la Nucleo : MISO → D12, SCK → D13, MOSI → D11. CS sur D4. Images dans `docs/Photo carte nucléo/` du dossier projet.
- **Console série à 115200 bauds**, sur l'USART2 (le port du ST-LINK). Option `-Wl,-u,_printf_float` dans `platformio.ini` pour que `printf` affiche les nombres à virgule.

## 3. Organisation du code

- **Base de code : le dépôt CESI.** On part du dépôt fourni plutôt que de tout réécrire. Pourquoi : il contient déjà des pilotes testés pour le matériel fourni.
- **Un fichier par rôle** (prosit 3) : `main.c` (boucle principale), `modes.c` (machine à états des 4 modes), `boutons.c` (interruptions et appui long), `mesures.c` (capteurs et ligne CSV), `gps.c` (réception GPS), `led.c` (couleurs et clignotements), `horloge.c` (RTC), `stockage.c` (fichiers sur la SD), `carte_sd.c` (pilote SPI de la SD), `parametres.c` (paramètres et Flash), `console.c` (commandes du mode configuration).
- **La boucle `while (1)` ne bloque jamais.** Pas de `HAL_Delay` long ni de lecture qui attend. Pourquoi : sinon un appui de 5 s arrive en retard ou se perd.
- **Une structure par capteur** (prosit 4) : nom, paramètre d'activation, dernière valeur, validité, nombre d'échecs, seuils. Les capteurs sont rangés dans un tableau de structures. Où : `Capteur` dans `src/mesures.c`.
- **Durées comparées par soustraction** (`HAL_GetTick() - debut >= duree`). Pourquoi : le calcul reste juste quand le compteur de millisecondes repasse à 0, au bout de 49 jours environ.

## 4. Boutons et modes

- **Le mode change dès que les 5 s sont atteintes**, sans attendre que l'utilisateur lâche le bouton. L'interruption se déclenche sur les deux fronts (appui et relâchement) et note seulement l'heure. C'est la boucle principale qui décide si 5 s sont passées. Où : `Bouton_Front()` et `Bouton_AppuiLong()` dans `src/boutons.c`.
- **Anti-rebond logiciel de 30 ms.** Un front qui arrive moins de 30 ms après le précédent est ignoré.
- **Un seul changement de mode par appui.** Garder le bouton enfoncé 10 s ne fait pas changer de mode deux fois (`deja_signale`).
- **En configuration, les boutons ne font rien.** On en sort seulement après 30 min sans activité. Chaque caractère tapé dans la console remet ce compte à zéro (`Modes_SignalerActivite()`).

## 5. Mesures

- **TIMEOUT comme la spécification, sans bloquer.** Une mesure dure jusqu'à ce que tout ait répondu, ou au plus TIMEOUT secondes. Ce qui n'a pas répondu est réessayé toutes les 100 ms ; entre deux essais, la boucle principale continue (boutons, LED, console). Après TIMEOUT : « NA », et 2 mesures en timeout d'affilée = erreur LED. Vaut pour le BME280, la lumière, la RTC (erreur dès la première fois, livrable 1) et le GPS muet. Vérifié le 20/10 : lumière débranchée, NA après 30 s, passage en mode éco pendant l'attente. En démo, taper `TIMEOUT=5`. Où : `Mesures_Commencer()`, `Mesures_Essayer()`, `Mesures_Terminer()` dans `src/mesures.c`.
- **Debug / release de LOG_INTERVAL.** Avec `MESURES_TEST` à 1 (debug, par défaut), LOG_INTERVAL compte en secondes : tests et démo. À 0 (release, `-DMESURES_TEST=0` donné par le script de compilation), en minutes comme la spécification. Code rendu = release, démo = debug. Où : `src/mesures.c`.
- **Première mesure tout de suite au démarrage** et en sortant de la configuration, sans attendre un premier intervalle complet. L'intervalle compte depuis le début de chaque mesure.
- **Maintenance : une mesure toutes les 2 s.** Le livrable 1 dit « en direct » sans fixer de rythme.
- **BME280 réinitialisé avant chaque mesure** (environ 15 ms). Un capteur débranché, ou qui subit une baisse de tension (carte SD retirée), repart avec ses réglages d'usine sans mesurer l'humidité, qui reste alors bloquée (92,6 % figé lors de l'essai du 20/10).
- **Seuils** : MIN/MAX_TEMP_AIR et PRESSURE_MIN/MAX, réglables. Une valeur en dehors déclenche l'erreur « données incohérentes ».
- **Hygrométrie** : si la température est en dehors de HYGR_MINT..HYGR_MAXT, l'humidité est écrite « NA » (pas prise en compte), sans erreur.
- **Capteur désactivé** (LUMIN, TEMP_AIR, HYGR ou PRESSURE à 0) : colonne « OFF », jamais en erreur.
- **Lumière en valeur brute** du convertisseur (0 à 4095). Le niveau FAIBLE / MOYENNE / FORTE se calcule en ramenant la valeur sur 0 à 1023, l'échelle de LUMIN_LOW et LUMIN_HIGH.
- **Lumière débranchée détectée.** Le capteur est analogique : débranché, l'entrée A2 flotte et donne une valeur au hasard. On mesure deux fois, après avoir imposé 0 V puis 3,3 V à la broche. Un capteur branché donne deux valeurs proches, une entrée vide suit la tension imposée (écart de plus de 400 = débranché). Vérifié le 20/10.
- **Ligne CSV** : `DATE,HEURE,TEMP_AIR_C,HYGR_PCT,PRESSION_HPA,LUMIERE,NIVEAU_LUMIERE,LATITUDE,LONGITUDE`, séparateur virgule, point décimal, date `AAAA-MM-JJ`. « NA » si le capteur n'a pas répondu (et date « NA,NA » si la RTC ne répond pas).

## 6. GPS

- **Réception par interruption.** Chaque octet reçu sur l'UART4 déclenche une interruption qui assemble les trames et garde seulement la dernière trame GGA. La lecture se fait ensuite sans attendre. Pourquoi : la fonction du dépôt CESI (`GroveGPS_ReadGGA`) attend une trame jusqu'à 1,1 s et bloquait la boucle. Où : `src/gps.c`.
- **GPS « muet » à un essai = aucun octet reçu depuis 3 s** (il envoie une trame par seconde). Muet jusqu'à TIMEOUT = échec.
- **Seul un GPS muet est une erreur.** Sans position, par exemple à l'intérieur, on écrit « NA » sans faire clignoter la LED. Pourquoi : un GPS sans satellites n'est pas en panne.
- **Somme de contrôle NMEA vérifiée.** Une trame abîmée n'est pas utilisée.
- **Mode économique : la première mesure lit le GPS**, puis une mesure sur deux.

## 7. Carte SD

- **FatFs R0.12c** (bibliothèque de ChaN fournie avec STM32Cube), copiée dans `lib/fatfs/`. Configuration dans `ffconf.h` : un volume, noms longs activés (au-delà de `_9`, « AAMMJJ_10.LOG » ne tient plus dans le format court 8.3), pas de formatage depuis la carte.
- **Pilote SPI écrit pour FatFs** (`src/carte_sd.c`) : `SDLogger_WriteLine()` du dépôt CESI était vide. Initialisation à 15,6 kHz (moins de 400 kHz, imposé par la norme SD), puis 2 MHz pour lire et écrire (environ 2 ms par secteur). CMD0 répété jusqu'à 10 fois et envoyé sans attendre que la carte soit prête, sinon une carte remise après la maintenance n'était jamais reconnue (essai du 20/10). Un secteur est transféré en un seul appel HAL : octet par octet, le processeur à 4 MHz était trop lent.
- **Ouverture, écriture et fermeture à chaque ligne.** `f_close` vide le tampon sur la carte : entre deux mesures, rien n'est en cours, et une coupure de courant ne perd au plus que la ligne en cours.
- **Archivage par renommage.** Quand une ligne ferait dépasser FILE_MAX_SIZE, `AAMMJJ_0.LOG` est renommé en première révision libre (`_1`, `_2`...), puis un nouveau `_0` vide est créé avec la ligne d'en-tête. Renommer donne le même résultat que la copie de la spécification, sans recopier le fichier. Le prochain numéro libre est trouvé en lisant le répertoire une seule fois (au premier archivage du jour), puis gardé en mémoire : essayer `_1`, `_2`... un par un bloquait la boucle 8 s avec 38 archives.
- **FILE_MAX_SIZE = 2048 par défaut** (« 2 Ko par défaut » dans le descriptif ; l'exemple de commande donne 4096).
- **Erreurs** : plus de place (`FR_DENIED` ou écriture incomplète) = SD pleine ; tout autre problème = erreur d'accès, et la carte est remontée à la mesure suivante (carte retirée puis remise).
- **Date des fichiers** donnée par la RTC (`get_fattime()` dans `src/horloge.c`).

## 8. Mode configuration

- **Commandes** : `NOM=VALEUR` pour régler, `NOM` seul pour lire la valeur, `CLOCK=HH:MM:SS`, `DATE=MM,JJ,AAAA`, `DAY=MON..SUN`, `RESET`, `VERSION`, `HELP`. Minuscules acceptées, `LOG_INTERVALL` (orthographe de la spécification) aussi.
- **Valeur hors domaine ou pas un entier** : message d'erreur, rien n'est modifié (livrable 1).
- **Domaines ajoutés** quand la spécification n'en donne pas : LOG_INTERVAL de 1 à 1440 min, FILE_MAX_SIZE de 512 à 1 048 576 octets, TIMEOUT de 1 à 300 s.
- **Réception par interruption** sur l'USART2, dans une file circulaire de 64 caractères. Écho des caractères tapés (la console ne les affiche pas d'elle-même). Hors configuration, ce qui est tapé est ignoré.
- **VERSION** : version, numéro de build (`-DNUMERO_BUILD=...`, donné par le script de compilation) et numéro de lot.

## 9. LED

- **« Deux fois plus long » sur une période de 1 s** : 333 ms de rouge, puis 667 ms de l'autre couleur. Durées égales : 500 ms chacune.
- **Une seule erreur affichée à la fois** : la première dans l'ordre RTC, GPS, capteur, données incohérentes, SD pleine, écriture SD. Les erreurs SD ne s'affichent pas en maintenance (la carte n'y est pas utilisée).
- **Pas de clignotement d'erreur en configuration**, puisqu'il n'y a pas de mesure.
- **Couleurs RGB** : vert (0, 255, 0), jaune (255, 255, 0), bleu (0, 0, 255), orange (255, 80, 0).
- **La couleur n'est envoyée à la LED que si elle change.** Pourquoi : chaque envoi prend du temps.

## 10. Ce qui n'est pas codé

- **Modules complémentaires** (température de l'eau, courant, vent, particules) : non codés. La structure `Capteur` et la boucle sur les capteurs permettent de les ajouter.

## 11. Outils et travail en équipe

- **Compiler depuis VS Code** (extension PlatformIO). Dans la VM du CESI, la commande `pio` du terminal (version 4.3.4 d'apt) est cassée. Pour un script, utiliser `~/.platformio/penv/bin/pio`.
- **Paramètres propres à chaque carte** : ils sont dans la Flash de la carte. Une carte neuve démarre avec les valeurs par défaut, et l'heure se règle avec `CLOCK`, `DATE` et `DAY`.
- **Une branche par chantier**, fusionnée dans `main` quand elle marche sur la carte. La branche `mesures-periodiques` a été fusionnée dans `main` le 20/10. La branche `sd-rtc-config` (SD, RTC, configuration) a été testée sur la carte le 20/10.
