# Fichiers de configuration

Cette section décrit les formats de fichiers qui configurent un système Open RC Node.
Chaque format a sa page de référence de syntaxe.

| Format | Extension | Rôle | Référence |
|---|---|---|---|
| Déclaration de channels ComBus | `.cb` (YAML) | Déclare les channels ComBus (identifiant, type, portée, direction, activation). Lu par le ComBus Builder au build. | [cb_syntax.md](cb_syntax.md) |

## Règles communes

- Les fichiers sont du YAML. Le fichier doit être un mapping à la racine ; un
  fichier vide ou une liste en racine est une erreur de build.
- La validation est stricte : une clé inconnue dans un channel est refusée avec
  un message qui nomme le fichier et la position du channel.
- Un fichier `.cb` peut être placé n'importe où sous `src/`, à côté du module
  qu'il décrit. Le détail de la recherche est dans [cb_syntax.md](cb_syntax.md).

Fonctionnement interne du ComBus Builder (pour modifier le builder lui-même) :
`scripts/combus_builder/README.md`.
