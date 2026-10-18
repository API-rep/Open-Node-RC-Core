### Objectifs de fin d'hiver 2027 ###

1. **Un design de communication au point**, que le lien soit « remote » ou local (le même mécanisme dans les deux cas). Chantier : « Rework du module de communication ComBus » ci-dessous.
2. **Une télécommande fonctionnelle sur plusieurs machines** et, idéalement, **un réseau multi-nœuds** (plusieurs cartes et machines ensemble) lui aussi fonctionnel. S'appuie sur le point 1, sur le mode nœud du ComBus Builder (déjà compilé et vérifié) et sur la convergence Machine / Sound.
3. **Côté matériel (propriétaire)** : concevoir les cartes des machines. Camions : fait. Pelleteuse et chargeuse : à faire. Bulldozer : en construction. Ces cartes alimenteront les configurations de machines (script de génération du `platformio.ini`, « Autres chantiers »).
4. **Si le temps le permet** : continuer le reste de cette roadmap.


### Convergence des environnements Machine / Sound ###

Le projet audio actuel trouve son origine dans un projet autonome « son et lumière ». Cette séparation était pertinente tant que les deux environnements évoluaient indépendamment.

L'intégration progressive du code sur le matériel cible a toutefois mis en évidence une forte rigidité : configurations figées, hypothèses propres aux télécommandes RC et adaptations répétitives à chaque nouveau projet.

Après plusieurs itérations, un nouveau projet `sound_module` a été initié (retiré depuis du dépôt).

Les développements récents montrent cependant que les environnements *machine* et *sound* diffèrent beaucoup moins qu'initialement supposé. Les cartes son disposent elles aussi de GPIO, peuvent piloter des servos ou des moteurs DC et exécutent finalement les mêmes mécanismes fondamentaux.

**Orientation retenue :** faire converger les deux environnements vers une architecture commune, où le son et la lumière deviennent des modules spécialisés plutôt que des projets indépendants.

Il ne s'agit pas de reprendre le code son et lumière d'origine pour l'adapter. Le projet de base manquait de modularité et accumulait les exceptions. La cible est de réécrire ces fonctions selon la mécanique du projet actuel :
- une partie **core** pour le matériel et la gestion du son ;
- une **configuration** par machine pour s'adapter aux différences, éventuellement avec des ComBus Processors dédiés.

Une partie du travail est déjà faite ou en cours. Le chantier complet est prévu à long terme (environ un an), une fois le projet de base mûr et bien ancré.


### Chaînes de processors déclaratives (`.cbch`) ###

Aujourd'hui, le câblage des chaînes de ComBus Processors (qui est branché sur qui, dans quel ordre) est écrit à la main en C++. Exemple : `src/core/system/failsafe/failsafe_chain.cpp`, avec `cb_reset_fn` en tête puis les `cb_or_fn`.

**Idée :** décrire ce câblage dans des fichiers `.cbch`, lus par le ComBus Builder. Chaque fichier donnerait une liste ordonnée de processors et leurs entrées/sorties. Le builder générerait la table de câblage et refuserait le build si un processor cité n'existe pas en C++. Les processors et leur logique restent en C++.

**État :** rien n'est implémenté. Le builder ne lit que la section `channels:` des fichiers ; aucun fichier `.cbch` n'existe dans `src/`.

**À faire si le sujet est repris :**
- définir le format du fichier ;
- choisir comment le builder repère les processors C++ existants (table explicite, recherche textuelle... non choisi) ;
- générer le câblage et tester les deux cas (processor existant accepté, inexistant refusé) ;
- migrer d'abord une chaîne simple, par exemple celle du failsafe.


### Rework du module de communication ComBus ###

**Objectif de fin d'hiver :** voir la section « Objectifs de fin d'hiver 2027 » en tête de ce document.

Étude préliminaire (liens multi-instances, multi-protocoles) : `doc/WIP - ComBus link rework.md`. Modèle retenu au §3, points à trancher avant de coder au §8.

Aujourd'hui, seul l'UART est implémenté, de façon assez statique (flags `COMBUS_UART*`, `src/machines/init/com/combus_uart_init.cpp`). Objectif : un module de communication plus modulaire, ouvert à d'autres transports. L'interface de transport `NodeCom` existe déjà.

**Early stage — filtrage selon `direction`.** Le champ `direction` est généré dans chaque canal mais n'est lu nulle part dans le runtime : l'émission envoie tous les canaux de la vue du lien. À traiter avec le rework de fond du mécanisme de communication, pas avant.
- Volet « timeout » de ce chantier : relance côté télécommande pour un lien sans retour (uplink seul) ; accusé de réception et délai d'attente pour un lien avec retour (downlink). Aucune trace dans le code aujourd'hui.

**Secondaire — qualité de service à deux niveaux (basse / haute priorité).** À envisager seulement si le besoin apparaît (encombrement du réseau, etc.). Aucune trace dans le code.

### Autres chantiers ###

- Rapatrier les définitions de structures du dossier `include/struct` vers leurs fichiers respectifs.
- Vérifier la documentation actuelle (contenu, liens vers les fichiers) et sa segmentation, pour une prise de contexte optimale de l'agent IA lors de la (re)prise d'un job.
- Écrire le script de parsing des configurations YAML de cartes et de machines (base du script de génération ci-dessous).
- Créer le script de génération du `platformio.ini` d'une machine (env racine + sous-envs par carte) depuis un YAML (+ éventuellement une interface graphique de configuration).
