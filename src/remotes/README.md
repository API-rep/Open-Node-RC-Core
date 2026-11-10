# La télécommande

La télécommande est un nœud comme un autre : elle a son propre ComBus et un lien vers la machine qu'elle pilote. Ce dossier contient son firmware (env `remotes`).

## Ce qu'elle envoie

Un type de machine (`-D MACHINE_TYPE_<TYPE>`, par exemple `dumper_truck`) a un **contrat d'entrées** : la vue BASE de son profil. La télécommande en a le miroir, construit par le ComBus Builder : ce que la machine reçoit, la télécommande l'émet. Elle n'envoie donc que des entrées (sticks, boutons), et la machine reste seule maîtresse de ce qu'elle en fait.

Le lien a sa propre table et son propre MD5 (`combus::peer::kRemoteLink`), les mêmes que la machine ait des coupleurs ou non. Comme pour tout lien avec table, le MD5 porte aussi le côté de chaque bout : la machine est l'embase, la télécommande la fiche. Il n'y a pas d'appairage : c'est un lien ordinaire entre nœuds.

## Comment le lien s'ouvre

```text
setup()  remote_links_init()     un lien par machine pilotée (driven_machines.h)
loop()   remote_links_update()   réception, surveillance, puis émission
```

La table des machines pilotées, `config/driven_machines.h`, donne pour chacune son nom, son ComBus et son lien (port, sens, protocole).

## Limites actuelles

- **Une seule machine pilotée.** En piloter plusieurs demande un ComBus par machine, donc un par lien : `combus_links_init()` et `combus_tx_update()` n'en prennent qu'un aujourd'hui.
- **Pas d'entrée physique** (`INPUT_NONE`) : les channels gardent leur valeur sûre.
- **Le port est écrit en dur** (UART2, GPIO 17/16), en attendant une configuration de carte pour la télécommande.

Ces trois points sont marqués `TODO(chantier 4)` dans le code.

## Aller plus loin

- [Les liens](../core/system/combus/protocol/link/README.md) : décrire, démarrer et surveiller un lien
- [Le protocole ComBus](../core/system/combus/protocol/README.md) : la vue d'ensemble
- [Le ComBus Builder](../../scripts/combus_builder/README.md) : la vue miroir de la télécommande
