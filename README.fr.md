# Open RC Node

Open RC Node est un projet open-source visant à créer un environnement RC plus flexible, plus modulaire et mieux adapté aux projets DIY modernes.

L'idée est née simplement lors de séances de jeu en famille. Comme beaucoup de passionnés de modélisme, nous nous sommes rapidement retrouvés avec plusieurs véhicules, plusieurs télécommandes et une multitude de systèmes indépendants à gérer.

Les systèmes RC traditionnels fonctionnent généralement selon une logique simple : une télécommande est associée à un ou plusieurs véhicules, avec des possibilités limitées d'interaction entre les différents équipements.

Cette approche fonctionne parfaitement pour de nombreux usages, mais elle montre rapidement ses limites lorsqu'il devient nécessaire de partager un parc de machines entre plusieurs utilisateurs.

Open RC Node explore une approche différente en considérant l'ensemble des équipements comme les composants d'un même écosystème RC.

Télécommandes, véhicules, cartes électroniques, modules d'extension ou services logiciels peuvent communiquer entre eux au sein d'une architecture commune, tout en conservant la souplesse propre au monde du DIY.

Open RC Node ne cherche pas à réinventer le modélisme RC, mais à fournir une architecture commune permettant à plusieurs équipements, utilisateurs et services de collaborer au sein d'un même environnement, en s'affranchissant des limitations inhérentes aux architectures RC traditionnelles.

---

## Pensé pour les makers

Open RC Node est avant tout un projet de passionné destiné aux passionnés.

Le projet s'adresse aux personnes qui aiment comprendre, modifier et construire leurs propres systèmes.

L'objectif n'est pas de proposer une solution clé en main, mais une boîte à outils permettant de concevoir des équipements RC adaptés à ses propres besoins.

Un minimum de connaissances en électronique, en systèmes embarqués et en configuration logicielle reste nécessaire, mais l'architecture cherche à limiter autant que possible le développement spécifique au profit d'une approche basée sur des configurations entièrement éditables, naturellement compatibles avec les outils d'assistance modernes, y compris les IA génératives.

---

## ComBus : la colonne vertébrale du système

Au cœur du projet se trouve **ComBus**, un protocole de communication conçu spécialement pour Open RC Node et autour duquel s'articule l'ensemble du système.

Il est utilisé aussi bien pour les communications entre plusieurs équipements que pour les échanges internes au sein d'un même nœud.

Un module de surveillance batterie, un contrôleur moteur, un gestionnaire de modes de fonctionnement, une télécommande ou un véhicule utilisent tous les mêmes mécanismes de communication : ComBus.

Il constitue la colonne vertébrale du projet, assurant le transport des informations, des commandes et des états entre l'ensemble des éléments qui composent le système. Entièrement configurable, il adapte sa structure aux besoins de chaque équipement et de chaque architecture réseau, sans être limité par les modèles de communication ou les contraintes de canaux propres aux systèmes RC traditionnels.

---

## Une architecture définie par la configuration

L'un des objectifs majeurs du projet est de privilégier la configuration plutôt que le développement spécifique.

Les équipements sont décrits à l'aide de fichiers de configuration permettant de définir :

* la configuration du ComBus ;
* les configurations des machines ;
* le matériel utilisé ;
* les modules logiciels actifs ;
* les modes de fonctionnement ;
* les comportements généraux du système.

L'idée est qu'un nouveau véhicule, une nouvelle télécommande ou une nouvelle carte d'extension puisse être créé principalement par assemblage et configuration de composants existants.

---

## Une architecture ouverte et évolutive

Open RC Node repose actuellement sur l'ESP32 et son écosystème Arduino, mais son architecture n'est pas liée à un microcontrôleur particulier. Toute plateforme capable d'exécuter les composants fondamentaux du système peut théoriquement accueillir une implémentation d'Open RC Node.

De ce fait, ESP-NOW constitue aujourd'hui un candidat naturel pour les communications radio entre nœuds, mais l'architecture est conçue pour rester aussi indépendante que possible du moyen de transport utilisé.

De la même manière, les sources de contrôle peuvent être très diverses :

* télécommandes personnalisées ;
* manettes de jeu ;
* interfaces web ;
* ou tout autre équipement capable de produire ou consommer des messages ComBus.

Cette ouverture permet d'imaginer des systèmes allant du simple véhicule RC jusqu'à des projets plus ambitieux intégrant plusieurs machines, plusieurs opérateurs et différents niveaux d'automatisation.

---

## État du projet

Open RC Node est actuellement en développement actif. Les bases de l'architecture sont désormais posées et les principaux concepts du projet sont clairement établis.

Le projet est développé sur le temps libre de son auteur, dans une démarche de passionné et d'expérimentation continue. Les outils d'IA modernes participent activement à cette aventure, aussi bien pour la conception, la documentation que pour l'exploration de nouvelles idées.

Il faudra cependant encore du temps, des essais et de nombreuses itérations avant d'aboutir à un écosystème pleinement fonctionnel et mature.

Le projet évolue progressivement au travers d'expérimentations concrètes, avec pour objectif de construire un environnement RC cohérent, extensible et agréable à utiliser pour les passionnés de DIY.
