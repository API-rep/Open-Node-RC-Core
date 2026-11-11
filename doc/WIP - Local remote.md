# WIP - Local remote

Statut : **à ouvrir** (document `WIP`), créé le 2026-11-11 à la clôture du ComBus link rework (idée retenue le 2026-10-25, question ouverte du rework). Rien n'est implémenté de ce document.
Public : le propriétaire et les futures sessions de travail sur les sources locales de commandes.
Plan : §1 constat, §2 idée retenue, §3 règles, §4 questions ouvertes.

Documents liés : [WIP - Remote firmware](<WIP - Remote firmware.md>) (la télécommande, un nœud à part entière) ; glossaire de [doc/README.md](README.md#glossary).

---

## 1. Constat

Une machine reçoit ses commandes par deux chemins différents :

- d'une **télécommande** (un nœud) : un lien ComBus, avec son contrat (MD5), sa table, son filtrage par sens et sa perte de lien (`NODE_LINK_FAIL`) ;
- d'une **source locale** branchée sur la machine (aujourd'hui la manette PS4 en Bluetooth, demain un récepteur RC PPM / SBUS) : un backend d'entrée, dont les boutons et sticks sont mappés vers le ComBus par `inputs_map`, et dont la santé passe par un contributeur à part (`LOCAL_REMOTE_FAIL`).

Le backend PS4 reste dans `input` : c'est un dispositif d'entrée, il n'expose pas de flux d'octets comme un `NodeCom`. Mais vu de la machine, une manette PS4 **est** une télécommande : deux chemins pour la même chose.

## 2. Idée retenue : un nœud virtuel

Le backend d'une source locale devient un **lien inter-nœud virtuel** : il remplit directement la trame reçue (snapshot) d'un lien, sans octets ni encodage, appliquée par le même chemin qu'une trame reçue (filtre par sens, couche d'écriture du lien). Sa santé devient celle du lien (`linkTimeoutMs`, perte vers `NODE_LINK_FAIL`), à la place de son contributeur et de son câblage à part.

## 3. Règles

- **Une source = un lien** : la règle « un seul émetteur par channel » s'applique telle quelle.
- **Deux sources de commandes actives en même temps : blocage complet** (pas de choix, pas de priorité), comme pour deux émetteurs entre nœuds.
- Pas pour les capteurs ordinaires : ce sont des channels `SYSTEM` locaux, sans contrat ni santé de lien.

## 4. Questions ouvertes

- Où vit le lien virtuel (cœur, `input`, `com`) et comment il se déclare dans la table des liens de la carte (`comLinks[]`).
- Le contrat d'un lien virtuel : un MD5 a-t-il un sens quand les deux bouts sont dans le même firmware ?
- Ce que devient `inputs_map` : la table du lien virtuel, ou un étage avant elle.
