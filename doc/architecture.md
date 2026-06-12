# Project Architecture Reference


## 1. Vue d'ensemble du projet

firmware multi-nœuds pour véhicules RC de chantier (modélisme).


```
[ Télécommande PS4 ]
        │  Bluetooth
        ▼
[ Machine node — ESP32 ]  ─── UART ───► [ Sound node — ESP32 ]
   src/machines/               src/sound_module/
   - Input processing          - ComBus RX
   - Motion simulation         - Sound engine
   - Driver output             - Servo/DC output
        │
        ▼
[ Moteurs, servos, lumières ]

---
