# TODO — Dashboard sur second core (ESP32)

## Contexte
- Le dashboard ANSI consomme du temps CPU et peut provoquer des saccades sur la machine lors de l'affichage.
- L'ESP32 dispose de deux cœurs (core 0 et core 1).
- La logique machine tourne sur core 0 par défaut.
- Le dashboard ne fait que lire des données (pas de partage critique), donc il peut être isolé sur core 1.

## Objectif
- Déplacer le dashboard dans une tâche FreeRTOS dédiée, épinglée sur core 1.
- Réduire l'overhead sur le core principal.

## Implémentation
- Créer une tâche avec `xTaskCreatePinnedToCore`.
- La tâche appelle périodiquement `dashboard_update()`.
- Protéger les accès partagés si besoin (mutex/atomic), mais ici non nécessaire.

### Exemple minimal
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

## Note UART
- Voir `TODO_uart_guard.md` pour la proposition d'init centralisée et de guard UART.
- L'objectif est d'éviter les conflits d'accès et de garantir l'exclusivité par port.

## Points à valider
- Dashboard isolé = pas de conflit.
- Machine fluide même avec dashboard actif.
- Si besoin, ajuster la priorité ou le délai de la tâche.

---

À tester dès que possible avec crédits premium.

// EOF TODO_dashboard_core.md
