### Convergence des environnements Machine / Sound ###

Le projet audio actuel trouve son origine dans un projet autonome « son et lumière ». Cette séparation était pertinente tant que les deux environnements évoluaient indépendamment.

L'intégration progressive du code sur le matériel cible a toutefois mis en évidence une forte rigidité : configurations figées, hypothèses propres aux télécommandes RC et adaptations répétitives à chaque nouveau projet.

Après plusieurs itérations, un nouveau projet `sound_module` a été initié.

Les développements récents montrent cependant que les environnements *machine* et *sound* diffèrent beaucoup moins qu'initialement supposé. Les cartes son disposent elles aussi de GPIO, peuvent piloter des servos ou des moteurs DC et exécutent finalement les mêmes mécanismes fondamentaux.

**Orientation retenue :** faire converger progressivement les deux environnements vers une architecture commune, où le son et la lumière deviennent des modules spécialisés plutôt que des projets indépendants.

Jusqu'à cette convergence, les deux projets évolueront en miroir afin de limiter le coût d'une fusion ultérieure.


### System improvement roadmap ###
## DynConfig structure implementation ##
Following structure readme file :
- Split existing config into config/dynconfig/state sub structure
- "norm" dynconfig usage for a minimal RAM footprint (ex : skip title, etc)
- Adapt existing dynconfig consumer (combus processor, ...S)