# CLAUDE.md — Open Node RC Core

## Projet
- Open Node RC Core (OpenRcNode) : cœur logiciel de machines RC maison (ESP32). PlatformIO, firmware C++, outils Python.
- Propriétaire amateur, francophone : répondre en français.
- ComBus Builder (scripts/combus_builder/) : génère depuis des YAML .cb les en-têtes/sources ComBus par carte.
- Il tourne via un hook SCons : scripts/combus_scons_hook.py (`extra_scripts = pre:scripts/combus_scons_hook.py`, section [env] de platformio.ini).
- Architecture et historique : doc/combus_builder_handoff.md.
- Convention : un fichier dont le nom commence par ~ est en cours de rédaction (ex. doc/~combus_v2.md).
- src/sound_module et les envs sound_node_* ont été retirés. Ne pas les réintroduire.

## Règles absolues
- Aucun fichier hors du dossier du projet. Temporaires uniquement dans .pio\, supprimés après usage.
- Aucun commit, merge ou push sans feu vert explicite. Écrire « NE COMMITE PAS » tant qu'il n'est pas donné.
- Jamais `git add -A` ni `git commit -a` : ajouter les fichiers un par un. Pas de --force.
- Ne pas toucher aux 6 stashes existants.
- Modifications chirurgicales : une intention par changement, pas de refactor non demandé.
- doc/configuration/cb_syntax.md reste non suivi tant que le propriétaire ne dit pas de le committer.

## Git
- Branche de travail par défaut : main (synchronisée avec origin).
- La branche combus-board-aliases a été supprimée (2026-10-15, demande du propriétaire).
- Merges avec --no-ff. `git branch -d` pour une branche mergée ; -D seulement sur demande explicite.
- Avertissement LF/CRLF : normal, ne rien modifier pour ça.

## Tests
- pytest : le dossier scripts/combus_builder/tests se lance d'un coup ou fichier par fichier (724 tests verts, aucun xfail au 2026-10-15).
- Toujours PYTHONIOENCODING=utf-8 (console Windows cp1252).
- Un test qui retourne un booléen sans assert passe à vide. Vérifier qu'un test « mord » en le faisant échouer sur l'ancien code.
- `scripts` n'a pas de __init__.py (namespace package) : les imports peuvent mélanger dépôts/worktrees. Vérifier `module.__file__`.
- Un test doit échouer par assert ou exception, jamais par `return False` (passe à vide sous pytest).

## Pièges Windows et outils
- Shell Windows (cmd/PowerShell). Commande longue : arrière-plan + log + polling (20 cycles max), jamais en direct. Pas de Get-Process en boucle.
- PowerShell : '@{u}..HEAD' entre apostrophes. cmd : `set "X=Y"` avec guillemets.

## Méthode de travail
- Le propriétaire délègue le Python mais vérifie chaque étape : rapports bruts (sorties collées), pas de résumé flatteur, pas d'analyse non demandée.
- Audits en lecture seule : faits avec fichier:ligne. « INTROUVABLE » plutôt que deviner.
- Avant tout test/commit : montrer le diff. Un commit par intention.
