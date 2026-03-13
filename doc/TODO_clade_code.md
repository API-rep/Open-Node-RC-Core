# TODO — Clade code : Dashboard sur second core + UART guard

## Dashboard sur second core (ESP32)

### Contexte
- Le dashboard ANSI consomme du temps CPU et peut provoquer des saccades sur la machine lors de l'affichage.
- L'ESP32 dispose de deux cœurs (core 0 et core 1).
- La logique machine tourne sur core 0 par défaut.
- Le dashboard ne fait que lire des données (pas de partage critique), donc il peut être isolé sur core 1.

### Objectif
- Déplacer le dashboard dans une tâche FreeRTOS dédiée, épinglée sur core 1.
- Réduire l'overhead sur le core principal.

### Implémentation
- Créer une tâche avec `xTaskCreatePinnedToCore`.
- La tâche appelle périodiquement `dashboard_update()`.
- Protéger les accès partagés si besoin (mutex/atomic), mais ici non nécessaire.

#### Exemple minimal
```cpp
void dashboardTask(void* arg) {
    while (true) {
        dashboard_update();
        vTaskDelay(10 / portTICK_PERIOD_MS); // ajuster selon besoin
    }
}

void setup() {
    // ... setup machine ...
    xTaskCreatePinnedToCore(
        dashboardTask, "Dashboard", 4096, NULL, 1, NULL, 1 // core 1
    );
}
```

### Points à valider
- Dashboard isolé = pas de conflit.
- Machine fluide même avec dashboard actif.
- Si besoin, ajuster la priorité ou le délai de la tâche.

---

## UART guard system

- Implémenter un module d'init UART générique (`uart_init.h/.cpp`) qui centralise la configuration hardware.
- Ajouter un guard (table statique ou singleton) pour empêcher plusieurs modules "client" d'utiliser le même port UART simultanément.
- Log explicite ou retour d'erreur si un port est déjà claimé.
- Permettre l'extension à plusieurs ports sans collision.

Exemple:
- `uart_init(port, ...)` vérifie si le port est déjà occupé.
- Si oui: refuse l'init, log ou retourne une erreur.
- Si non: marque le port comme occupé.

Priorité: robustesse multi-modules, diagnostic clair.

// EOF TODO_clade_code.md
