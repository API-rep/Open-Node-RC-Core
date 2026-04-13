# Sound Module — Notes de travail et TODOs

Source de départ : RC Engine Sound ESP32 (TheDIYGuy999)  
Répertoire : `src/sound_module/`  
Backup skeleton architecture cible : `src/sound_module_ref/` (**supprimé** — parité atteinte)  
Backup sources tiers (privé) : repo GitHub séparé à créer

---

## Plan de migration

### Étape 1 — Premier build vert [ DONE ]
- [x] Renommer `src.ino` → `main.cpp` (PlatformIO ne compile pas les `.ino`)
- [x] Ajouter les forward declarations manquantes (gérées implicitement par l'IDE Arduino)
- [x] Vérifier que tous les includes Arduino implicites sont présents
- [x] Build vert `sound_node_volvo` sans erreur de linker
- Notes :
  - Dashboard TFT supprimé → stub no-op `src/dashboard.h` (constantes + classe vide)
  - FastLED épinglé à `3.3.3` (les versions récentes 3.9+ utilisent un STL interne `fl::` incompatible avec gcc 8.4.0)
  - `SERVO_DEBUG` commenté : bug de portée (`steeringServoMicrosDelayed` déclaré dans un bloc `if` mais référencé hors de celui-ci — TODO Étape 4)
  - RAM : 10 % (32 KB / 320 KB) — Flash : 26,6 % (836 KB / 3 MB)

### Étape 2 — Abstraction config carte [ DONE ]
- [x] Identifier tous les `#define` hardcodés de pins GPIO dans `main.cpp` (section "PIN ASSIGNMENTS")
- [x] Créer `config/boards/sound_board_esp32.h` — pins regroupées par section (serial, analog, PWM input, motor/servo, lights, audio)
- [x] Remplacer le bloc GPIO inline dans `main.cpp` par `#include "config/boards/sound_board_esp32.h"`
- Notes :
  - `#define` conservés (pas de `constexpr`) pour compatibilité avec les nombreuses références dans le code tiers
  - Variante Wemos D1 Mini ESP32 et options `NEOPIXEL_ON_CH4` / `THIRD_BRAKELIGHT` conservées dans le header
  - Build confirmé vert — RAM 10 % / Flash 26,6 % (identique Étape 1)

### Étape 3 — Remplacement input par ComBus RX [ DONE ]
- [x] Intégrer `combus_rx` en remplacement de SBUS/PWM  
      → `combus_sound_interpreter.cpp` + architecture `SoundInterpreter` → `SoundCore` → `MixerState` → ISR
- [x] Mapper ComBus → sound via `sound_apply.cpp` + `soundMapper[]` (profiles/dumper_truck/)
- [x] Désactiver `2_Remote.h` — build guard `-D COMBUS_UART_RX=1` dans `sound_node_base`
- [x] Test hardware validé : machine COM3 → UART → nœud son COM8 (session 2026-03-22/23)

### Étape 4 — Nettoyage résidus [ DONE ]
- [x] Supprimer `src/sbus.h/.cpp`, `src/SUMD.h/.cpp` — ComBus UART remplace tous les protocoles RC physiques
- [x] Supprimer `src/serialInterface.h`, `src/webInterface.h` — non utilisés en mode ComBus
- [x] Garder déclarations SBUS/SUMD dans `sound_state.h/.cpp` sous guard `#if !defined(COMBUS_UART_RX)` (code rc_engine_sound intact)
- [x] Retirer `-D EMBEDDED_SBUS` de `platformio.ini`
- [ ] Conditionner WiFi/ESP-NOW (`ENABLE_WIRELESS`) proprement — déféré

### Étape 5 — Fusion avec skeleton `sound_module_ref` [ DONE ]
- [x] Migrer `main.cpp` vers l'architecture cible
- [x] Supprimer `sound_module_ref/` (parité validée — commit chore)

---

## TODOs futurs (hors scope migration)

### Dashboard TFT cabine (réintégration optionnelle)
- Écran LCD 80×160 px ST7735 simulant le tableau de bord intérieur du véhicule
- Gauges : vitesse (0–120 km/h), RPM (0–500), carburant %, AdBlue %
- Indicateurs : clignotants G/D, feux de croisement/route, antibrouillard, rapport, tension
- Deux skins : Gamadril (original) et Frevic (alternative)
- Données sources disponibles directement dans le moteur son une fois intégré
- Lib : TFT_eSPI (Bodmer) — à ajouter en `lib_deps` de `sound_node_volvo`
- Pins : SCL=18, DC=19, RES=21, SDA=23, CS=GND — conflit shaker+balises si activé
- Référence supprimée de `src/sound_module/` — à recréer depuis backup privé si nécessaire

### Dashboard mobile (monitoring technique)
- Option A : USB OTG + app *Serial USB Terminal* (Android) — zéro modif code, utilisable maintenant
- Option B : WebSerial/WebSocket via hotspot WiFi ESP32 — sans câble, ANSI propre, feature future

### Remorque ESP-NOW
- Nœud son → remorque via ESP-NOW (déjà prévu dans `0_generalSettings.h`, `ENABLE_WIRELESS`)
- Un seul émetteur radio sur le nœud son → pas de conflit avec le lien machine→son (UART filaire)
- `ENABLE_WIRELESS` (hotspot config WiFi) doit rester désactivé en mode opérationnel

---

## Notes diverses

- `src/sound_module/` est gitignored dans Open Node RC Core (code tiers, copyright)
- Le linker Arduino gère les forward declarations dans `.ino` ; en `.cpp` elles doivent être explicites
- `ESP32AnalogRead` : warning `ADC_ATTEN_DB_11 deprecated` — bénin, lib tierce non maintenue
- `PS4_Controller_Host` : warning `spp_deinit` présent dans ce build aussi — déjà connu, ignoré
