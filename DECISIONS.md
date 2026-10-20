# Décisions techniques

Ce fichier liste les choix techniques faits pendant le développement qui ne figurent pas dans le livrable 1 (Analyse du système, rendu le 01/10/2026). Pour chaque choix : ce qu'on a décidé, pourquoi, et où le trouver dans le code.

Les choix déjà expliqués dans le livrable 1 n'y sont pas répétés : format CSV, maintenance accessible seulement depuis le standard, détection des appuis par interruption, noms de fichiers `AAMMJJ_N.LOG`, boucle sur les capteurs activés.

## 1. Écarts avec le livrable 1

Ces points s'éloignent de ce que disent les diagrammes. Il faut soit les justifier dans la documentation, soit corriger le code avant la soutenance.

- **Pas de Timer pour LOG_INTERVAL.** Le livrable 1 prévoit un Timer qui envoie une interruption à chaque mesure. Le code compare à chaque tour de boucle le temps écoulé (`HAL_GetTick()`) avec l'intervalle. Pourquoi : plus simple, et la boucle tourne de toute façon en continu pour surveiller les boutons. Le prosit 6 demande un timer pour LOG_INTERVAL : à décider. Où : `Mesures_MettreAJour()` dans `src/mesures.c`.
- **TIMEOUT non mesuré en secondes.** Le livrable 1 prévoit d'attendre jusqu'à 30 s la réponse d'un capteur. Dans le code, une lecture ratée compte tout de suite comme un échec, et le capteur passe en erreur après 2 échecs de suite. Pourquoi : les pilotes répondent ou échouent en quelques millisecondes. Attendre 30 s bloquerait la boucle et les boutons. Où : `NB_ECHECS_ERREUR` dans `src/mesures.c`.
- **Mode économique sans `derniere_pos`.** Le livrable 1 dit qu'aux cycles sans lecture GPS, on réutilise la dernière position connue. Le code affiche « Position non lue (eco) ». À aligner sur le livrable 1 quand on écrira la ligne CSV. Où : `Mesures_LireGps()` et `Mesures_Afficher()` dans `src/mesures.c`.
- **Paramètres en RAM, pas en EEPROM.** Le STM32L476 n'a pas d'EEPROM. Les paramètres sont pour l'instant des constantes dans le code (`LOG_INTERVAL_DEFAUT`, seuils). Un stockage en mémoire Flash est prévu en bonus, si le temps le permet.

## 2. Matériel et montage

- **Capteur d'environnement BME280, pas BME680.** Le capteur du montage répond avec l'identifiant 0x60 à l'adresse 0x76 : c'est un BME280. Le pilote BME680 du dépôt CESI attend l'identifiant 0x61 et renvoie des valeurs fixes (20 °C, 50 %, 1013,25 hPa) sans rien lire. On utilise le pilote BME280 fourni par le professeur, en mode forcé (environ 50 ms d'attente par mesure). Où : `lib/cesi_grove/src/bme280.c`. Vérifié sur la carte le 15/10.
- **Boutons.** Rouge = BTN1 (PA10), vert = BTN2 (PB3). Les boutons Grove sont actifs à l'état bas (0 = enfoncé) : résistance de tirage interne vers le haut. Vérifié sur la carte le 14/10. Où : `src/boutons.c`.
- **Shield Grove réglé sur 3V3.** En 5 V, la LED RGB reçoit des données brouillées. LED branchée sur le port D6.
- **Console série à 115200 bauds**, sur l'USART2 (le port du ST-LINK). Option `-Wl,-u,_printf_float` dans `platformio.ini` pour que `printf` affiche les nombres à virgule.

## 3. Organisation du code

- **Base de code : le dépôt CESI.** Les codes générés par IA écrits avant le projet n'ont pas été repris. Pourquoi : le dépôt CESI contient déjà des pilotes testés pour le matériel fourni.
- **Un fichier par rôle** (prosit 3) : `main.c` (boucle principale), `modes.c` (machine à états des 4 modes), `boutons.c` (interruptions et appui long), `mesures.c` (capteurs), `gps.c` (réception GPS), `led.c` (couleurs et clignotements).
- **La boucle `while (1)` ne bloque jamais.** Pas de `HAL_Delay` long ni de lecture qui attend. Pourquoi : sinon un appui de 5 s arrive en retard ou se perd.
- **Une structure par capteur** (prosit 4) : nom, unité, dernière valeur, validité, nombre d'échecs, seuils. Les capteurs sont rangés dans un tableau de structures, pas dans des tableaux séparés. Où : `Capteur` dans `src/mesures.c`.
- **Durées comparées par soustraction** (`HAL_GetTick() - debut >= duree`). Pourquoi : le calcul reste juste quand le compteur de millisecondes repasse à 0, au bout de 49 jours environ.

## 4. Boutons et modes

- **Le mode change dès que les 5 s sont atteintes**, sans attendre que l'utilisateur lâche le bouton. L'interruption se déclenche sur les deux fronts (appui et relâchement) et note seulement l'heure. C'est la boucle principale qui décide si 5 s sont passées. Où : `Bouton_Front()` et `Bouton_AppuiLong()` dans `src/boutons.c`.
- **Anti-rebond logiciel de 30 ms.** Un front qui arrive moins de 30 ms après le précédent est ignoré.
- **Un seul changement de mode par appui.** Garder le bouton enfoncé 10 s ne fait pas changer de mode deux fois (`deja_signale`).
- **En configuration, les boutons ne font rien.** On en sort seulement après 30 min sans activité.

## 5. Mesures

- **Mode test de LOG_INTERVAL.** Avec `MESURES_TEST` à 1, LOG_INTERVAL compte en secondes au lieu de minutes (une mesure toutes les 10 s). Pourquoi : tester sans attendre 10 min. **À remettre à 0 pour le rendu.** Où : `src/mesures.c`.
- **Première mesure tout de suite au démarrage**, sans attendre un premier intervalle complet.
- **Maintenance : une mesure toutes les 2 s.** Le livrable 1 dit « en direct » sans fixer de rythme.
- **Pilote BME280 relancé après une lecture ratée.** Un capteur débranché puis rebranché repart avec ses réglages d'usine, sans mesurer l'humidité. Sans nouvelle initialisation, l'humidité restait bloquée.
- **Seuils par défaut** : température de -10 à 60 °C, pression de 850 à 1080 hPa. Une valeur en dehors déclenche l'erreur « données incohérentes ». L'humidité et la lumière n'ont pas de seuil d'incohérence.
- **Lumière en valeur brute** du convertisseur analogique (0 à 4095), sans unité. La valeur 0xFFFF signale une lecture ratée.

## 6. GPS

- **Réception par interruption.** Chaque octet reçu sur l'UART4 déclenche une interruption qui assemble les trames et garde seulement la dernière trame GGA. La lecture se fait ensuite sans attendre. Pourquoi : la fonction du dépôt CESI (`GroveGPS_ReadGGA`) attend une trame jusqu'à 1,1 s et bloquait la boucle. Où : `src/gps.c`.
- **GPS « muet » = aucun octet reçu depuis 3 s.** Le module envoie une trame par seconde.
- **Seul un GPS muet est une erreur.** Sans position, par exemple à l'intérieur, on affiche « NA » sans faire clignoter la LED. Pourquoi : un GPS sans satellites n'est pas en panne.
- **Somme de contrôle NMEA vérifiée.** Une trame abîmée n'est pas utilisée.
- **Mode économique : la première mesure lit le GPS**, puis une mesure sur deux.

## 7. LED

- **« Deux fois plus long » sur une période de 1 s** : 333 ms de rouge, puis 667 ms de l'autre couleur. Durées égales : 500 ms chacune.
- **Une seule erreur affichée à la fois** : la première dans l'ordre du tableau (RTC, GPS, capteur, données incohérentes, SD pleine, écriture SD).
- **Pas de clignotement d'erreur en configuration**, puisqu'il n'y a pas de mesure.
- **Couleurs RGB** : vert (0, 255, 0), jaune (255, 255, 0), bleu (0, 0, 255), orange (255, 80, 0).
- **La couleur n'est envoyée à la LED que si elle change.** Pourquoi : chaque envoi prend du temps.

## 8. Ce qui n'est pas codé

- **Modules complémentaires** (température de l'eau, courant, vent, particules) : non codés. La structure `Capteur` et la boucle sur les capteurs permettent de les ajouter.
- **RTC** : pas encore intégrée aux mesures. La mise à l'heure se fera avec les commandes `CLOCK` et `DATE` du mode configuration, pas dans le code de démarrage.
- **Carte SD** : la fonction `SDLogger_WriteLine()` du dépôt CESI est vide. L'écriture des fichiers (FatFs, rotation) reste à faire.

## 9. Outils et travail en équipe

- **Compiler depuis VS Code** (extension PlatformIO). Dans la VM du CESI, la commande `pio` du terminal (version 4.3.4 d'apt) est cassée. Pour un script, utiliser `~/.platformio/penv/bin/pio`.
- **Une branche par chantier**, fusionnée dans `main` quand elle marche sur la carte. La branche `mesures-periodiques` a été fusionnée dans `main` le 20/10.
