# WIP - Master

Point d'entrée des chantiers en cours et à venir : **le premier fichier à lire en reprenant le travail**, avant le sous-WIP du chantier visé. Créé le 2026-11-11, à la clôture du ComBus link rework.
Public : le propriétaire et les sessions de travail (humaines ou IA).

Pour reprendre une session : lire ce fichier, puis `CLAUDE.md` et [doc/ia-guidelines.md](ia-guidelines.md) (règles de travail), puis le sous-WIP du chantier en cours. Vocabulaire : glossaire de [doc/README.md](README.md#glossary).

Convention : un sous-WIP (`doc/WIP - <sujet>.md`) est en français tant que son chantier est ouvert ; à la clôture, son contenu utile passe dans la doc du projet, en anglais, et le WIP est supprimé (l'historique reste dans git).

---

## 1. Chantier en cours : clôture du ComBus link rework (branche `combus-link-rework`)

Le code est relu (blocs 1 à 11) ; reste, dans l'ordre :

1. **Doc** : rédiger la doc du rework en anglais, dans le style existant (accroche, conceptuel, technique), en y répartissant le contenu utile de `WIP - ComBus link rework.md` et de `~combus_v2.md` ; puis supprimer ces deux fichiers.
2. **Test à deux cartes** (deux cartes reliées en UART).
3. **Revue indépendante** par un agent tiers : relecture et tests fonctionnels, à partir des règles et des résultats attendus, sans les conclusions de l'auteur.
4. **Relecture de la doc** par le propriétaire.
5. **Merge** dans `main`.

## 2. Chantiers suivants (ordre proposé, à valider)

| Ordre | Chantier | Sous-WIP | Pourquoi à cette place |
|---|---|---|---|
| 1 | Norme des channels communs, bibliothèques de processors et de coupleurs types | [ComBus processors](<WIP - ComBus processors.md>) §2.8 | les chantiers suivants reposent sur ces noms |
| 2 | Norme des processors (flags par thème, paquets, redéfinition) | [ComBus processors](<WIP - ComBus processors.md>) | préalable à la chaîne de simulation |
| 3 | Chaîne de simulation (entrées → simulation → sorties `BASE`) | [Simulation chain](<WIP - Simulation chain.md>) | s'appuie sur la norme des processors |
| 4 | Feux | [Light rework](<WIP - Light rework.md>) | consommateur des sorties de la simulation |
| 5 | Télécommande | [Remote firmware](<WIP - Remote firmware.md>) | grande utilisatrice des channels communs |
| 6 | Sources locales de commandes (nœud virtuel) | [Local remote](<WIP - Local remote.md>) | même chemin que la télécommande |
| 7 | Transports (ESP-NOW d'abord, I2C, CAN, contrat `SYSTEM`) | [Node transports](<WIP - Node transports.md>) | la télécommande et la remorque sans fil |
| 8 | Builder de configuration à partir de fichiers YAML | — (pas encore de WIP) | voir §3 |

## 3. Futur rework sans WIP : builder de configuration YAML

Générer la configuration (dont `platformio.ini`) à partir de fichiers YAML, comme le ComBus Builder le fait pour les `.cb`. Idées à y reprendre :

- un env racine (configuration commune) et un sous-env par carte, mono-carte compris, avec `HAS_RUNLEVEL_MANAGER` sur une seule carte par machine (README du ComBus Builder, §10) ;
- **configuration modulaire** : un fichier par module optionnel (son, lumière : ses ports, ses devices, ses `.cb`, ses mappages de commandes), inclus par un flag `HAS_*` ; la table entrées → channels (`inputs_map`) en fragments inclus par flag ; la liste des profils compatibles par type de machine (une pelleteuse ne charge pas une remorque de semi-remorque) ;
- la table des liens d'une carte (`comLinks[]`, aujourd'hui dans son `envCfg.h`) et le paramétrage des `ComDev` du cœur.

## 4. En attente, sans chantier

- **Un même type plusieurs fois** dans un ensemble de nœuds (deux remorques, double traction) et deux nœuds de même type en égal à égal : il faudra raisonner sur les instances, plus sur les types ; piste d'un mode « twin » (deux composants qui ne forment qu'une entité). À reprendre avec l'identité des pairs (Node transports §6).
- **Un module qui fait de sa carte la carte mère** (une carte son et lumière pour une petite machine) : ressources minimales à vérifier au démarrage.
- **Renommage de `FAILSAFE_HARD` / `FAILSAFE_SOFT`** (piste `FAILSAFE_HALT` / `FAILSAFE_IDLE`), mis de côté pendant le rework.
- **`include/struct/`** : dossier déprécié, à vider vers les fichiers maîtres des thèmes (avec la norme des processors).
- **Relecture de l'application** (`src/machines/main.cpp`, `envCfg`, processors simulés, `volvo_A60H_bruder.cpp`, `core/system/sound/`, profils de son des types de machine).

Le reste du backlog : [deferred_features.md](deferred_features.md) et [roadmap.md](roadmap.md).
