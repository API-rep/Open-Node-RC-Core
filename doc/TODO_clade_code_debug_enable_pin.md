# TODO — Debug enable pin + dashboard driver pin state

## Debugger la pin enable
- Vérifier l'état logique de la pin enable (DRV8801, volvo A60H) au boot et en runtime.
- S'assurer que la polarité est bien ActiveHigh (HIGH = pont activé).
- Tracer les transitions (LOW au boot, HIGH en mode actif).
- Ajouter logs ou instrumentation pour observer l'état réel (oscilloscope, voltmètre, ou logs runtime).

## Dashboard driver : affichage des pins d'état
- Ajouter une section au dashboard driver (Layer 3) pour afficher :
  - Les pins d'état (enable, sleep, brake, etc.)
  - Leur tension logique (HIGH/LOW)
  - Option : afficher la valeur mesurée (si ADC ou sense pin disponible)
- Permettre un diagnostic rapide des états hardware depuis le terminal.

---

À transmettre à Claude code pour intégration et debug.

// EOF TODO_clade_code_debug_enable_pin.md
