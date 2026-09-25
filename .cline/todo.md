# Chantier 12.6 - Nettoyage isNotDrived (FS1 legacy)

## Phase 1 : Test file
- [ ] 1.1 : Retirer Group F (4 tests test_failsafe_combus_link_*) + RUN_TEST calls
- [ ] 1.2 : Retirer `txComBus.isNotDrived = false;` dans fillRandom

## Phase 2 : Production code
- [ ] 2.1 : combus_defs.h - retirer `bool isNotDrived` du struct ComBus
- [ ] 2.2 : sys_manager.cpp - retirer les 2 `bus.isNotDrived = true`
- [ ] 2.3 : sys_manager.h - nettoyer la doc
- [ ] 2.4 : input_update.cpp - retirer `bus.isNotDrived = false;`
- [ ] 2.5 : combus_frame.cpp - retirer `combus->isNotDrived = false;`
- [ ] 2.6 : init.cpp - adapter la pause loop `!comBus.isNotDrived && KEY`
- [ ] 2.7 : main.cpp - retirer le commentaire
- [ ] 2.8 : dashboard_sig.cpp - adapter la lecture
- [ ] 2.9 : dashboard_machine.cpp - adapter les lectures
- [ ] 2.10 : dashboard_input.cpp - adapter les lectures

## Phase 3 : Doc cleanup
- [ ] 3.1 : failsafe_module.md - mettre à jour §12.5
- [ ] 3.2 : remote_link_lost.cb - mettre à jour référence isDrived
- [ ] 3.3 : remote_link_fallback_chain.cpp - nettoyer le commentaire
- [ ] 3.4 : combus_manager.h - retirer commentaire isDrived
- [ ] 3.5 : proc_chain.cpp - retirer commentaire isDrived

## Phase 4 : Build + Commit
- [ ] 4.1 : Build test SUCCESS
- [ ] 4.2 : Commit "chantier 12.5 final + 12.6 cleanup isNotDrived"

## Side-effects (NOT TOUCHED)
- src/sound_module/ (deprecated per user)
- out/combus_generated/* (auto-regenerated)
- *.diff files (historical)
- scripts/combus_builder/canon/channels.py (a déjà la bonne ref, c'est juste un commentaire)
- doc/WIP - combus_v2 - Versioning and cache.md (historical audit, on laisse)
- doc/combus_v2 - UART workflow.md (historical, on laisse)