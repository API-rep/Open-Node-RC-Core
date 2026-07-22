Document de référence pour l'implémentation ComBus v2

Contexte et objectifs

Le ComBus actuel utilise une architecture plate qui atteint ses limites. ComBus v2 doit :

    - Supprimer l'ownership — mécanisme surfait qui complexifie sans bénéfice

    Organiser les canaux en groupes thématiques — activation par compile flag pour optimisation

    Permettre la superposition de couches — fusion (pas écrasement) des configurations Base + Machine + Accessoire

    Ajouter des métadonnées fonctionnelles — portée, priorité, RX/TX pour les règles de diffusion

    Adapter le transport — encodage/décodage avec notion de couche et priorité (QoS)

    Migrer les fonctions dans la structure — approche "mini-classe" en C

    Adapter les processeurs — même dynamique de fusion que les données

Architecture cible
text

┌─────────────────────────────────────────────────────────────┐
│                    ComBus v2 (structure unique)             │
├─────────────────────────────────────────────────────────────┤
│  Métadonnées globales :                                     │
│  - Version du ComBus Set                                   │
│  - Nombre de couches fusionnées                           │
│  - Taille totale                                          │
├─────────────────────────────────────────────────────────────┤
│  Groupes thématiques (activation par compile flag) :       │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐        │
│  │ Motion  │ │ Light   │ │ Core    │ │ Sound   │  ...    │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘        │
├─────────────────────────────────────────────────────────────┤
│  Métadonnées par canal :                                   │
│  - Portée (Inter-node/device/intra)                      │
│  - Priorité (QoS)                                        │
│  - Direction (RX/TX/RW)                                 │
│  - Couche d'origine (base/machine/accessoire)           │
│  - Normé (nom réservé ou non)                           │
└─────────────────────────────────────────────────────────────┘

Principe des couches — Fusion

Les couches se fusionnent (union), pas d'écrasement.
text

┌─────────────────────────────────────────────────────────────┐
│                    ComBus final (fusion)                    │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐   │
│  │  Couche Accessoire : ajoute des canaux spécifiques  │   │
│  │  (prise de force, pompe, faucheuse)                │   │
│  ├─────────────────────────────────────────────────────┤   │
│  │  Couche Machine : ajoute des canaux machine         │   │
│  │  (modèles de tracteur, configurations)             │   │
│  ├─────────────────────────────────────────────────────┤   │
│  │  Couche Base : définition générique du ComBus Set   │   │
│  │  (canaux standards de la famille)                  │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘

ComBus v2 — Plan de refonte
Ce qu'on refait et pourquoi
Problèmes actuels
Problème	Impact
Espace plat unique	Tous les canaux mélangés, pas de modularité
Ownership surfait	Complexité inutile, pas de vrai besoin
Couches mélangées	On ne distingue pas Inter-node / Inter-device / Intra-device
Configuration rigide	Une machine ne peut pas hériter ou spécialiser un Set
Processeurs liés au firmware	Pas de superposition possible
Objectifs v2

    Supprimer l'ownership

    Organiser les canaux en groupes (Motion, Light, Core, Sound, Telemetry, Battery)

    Activer/désactiver les groupes par compile flag

    Permettre la superposition de 3 couches : Base → Machine → Accessoire (fusion, pas écrasement)

    Ajouter des métadonnées : portée, priorité, RX/TX

    Les processeurs suivent la même logique de couches

Les concepts
Groupes thématiques

Les canaux sont rangés par domaine. Chaque groupe peut être activé ou non à la compilation.
Groupe	Contenu
Motion	throttle, steering, brake, direction, PTO...
Light	headlight, blinkers, hazard, work_light...
Core	runlevel, vbat, error_flags, mode...
Sound	horn, melody, volume...
Telemetry	latitude, longitude, speed, heading...
Battery	voltage, current, level, temperature...
Couches superposables

3 couches qui se fusionnent (union). Pas d'écrasement.
text

Accessoire : ajoute ses propres canaux (ex: pompe, faucheuse)
     ↓
Machine : spécialise le Set de base
     ↓
Base : définition générique du ComBus Set

Règle : un même identifiant ne peut apparaître qu'une seule fois. Si un canal est défini dans plusieurs couches, les métadonnées sont fusionnées (priorité max, portée max...).

Exemple PTO :

    Base définit pto_enable (ID standard)

    Machine ajoute une priorité élevée (sécurité)

    Accessoire utilise le même canal

    Résultat : le bouton PTO active toujours le même canal, seul le traitement change

Métadonnées

Chaque canal a des métadonnées pour guider la diffusion :
Champ	Rôle
Portée	Jusqu'où le canal peut être transmis (Intra / Inter-device / Inter-node)
Priorité	Importance pour l'envoi (0 = bas, 7 = critique)
RX/TX	Peut être reçu/transmis

Remplace l'ancien ownership par un mécanisme fonctionnel.
Noms réservés

Certains identifiants sont fixes pour éviter les conflits entre couches.
Domaine	Plage
Motion	0x0100 - 0x01FF
Light	0x0200 - 0x02FF
Core	0x0300 - 0x03FF
Sound	0x0400 - 0x04FF
Telemetry	0x0500 - 0x05FF
Battery	0x0600 - 0x06FF
Utilisateur	0x8000 - 0xFFFF
Processeurs

Même logique de couches que les données.

    Un processeur a un nom normé (ex: "pto.control")

    Une couche supérieure peut désactiver un processeur de même nom

    Ordre d'exécution : Base → Machine → Accessoire

Exemple :

    Base : processeur "pto.control" (commande standard)

    Accessoire : processeur "pto.control" désactivé

    Accessoire : processeur "pto.pump" (commande pompe)

Ce qu'on doit implémenter
Architecture
text

ComBus (structure unique)
  ├── Groupes activables (Motion, Light, Core...)
  ├── Métadonnées par canal
  ├── Fusion des 3 couches (Base + Machine + Accessoire)
  └── Processeurs avec désactivation

Organisation des fichiers
Fichier	Rôle
combus_types.h	Types, métadonnées, énumérations
combus_groups.h	Structures des groupes
combus_ids.h	Identifiants réservés
combus_config.h	Compile flags
combus_layers.c	Fusion des couches
combus_metadata.c	Gestion des métadonnées
combus_transport.c	Encodage/décodage
combus_processor.c	Gestion des processeurs
Activation

    Groupes : compile flags (CONFIG_COMBUS_MOTION, etc.). Si absent → groupe désactivé (pointeur NULL).

    Couches : chargement séparé Base / Machine / Accessoire. Fusion à la finalisation.

Règles de fusion
Règle	Description
Union	Tous les canaux de toutes les couches sont présents
Unicité	Un même ID ne peut apparaître qu'une seule fois
Types	Un canal ne peut pas changer de type
Métadonnées	Fusion : priorité max, portée max, RX/TX = OR
Phases d'implémentation

Phase 1 — Suppression de l'ownership

    Nettoyer le code existant

Phase 2 — Groupes thématiques

    Définir les groupes et leurs canaux

    Mettre en place les compile flags

Phase 3 — Fusion des couches

    Base + Machine + Accessoire → registre final

Phase 4 — Métadonnées

    Ajouter portée, priorité, RX/TX

Phase 5 — Transport adapté

    Encodage/décodage avec les nouvelles infos

Phase 6 — Processeurs

    Superposition et désactivation par couche

Points à trancher

    Ordre des couches : Base → Machine → Accessoire ?

    Activation des groupes : uniquement par compile flag ?

    Règles de fusion des métadonnées : priorité max, portée max ?

    Désactivation des processeurs : par nom identique ?

    Format de trame versionné ou non ?