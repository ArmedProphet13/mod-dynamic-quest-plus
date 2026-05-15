# DynamicQuests+ — TODO

Last updated: 2026-05-13

---

## DONE

### Foundation
- [x] CMakeLists.txt, conf/mod_dynamicquests.conf.dist, module skeleton
- [x] `PlayerContext` struct + `DQ_PlayerScript::OnUpdate` populates it
- [x] `EligibilityEngine` — all hard gates + soft scoring (VMAP outdoor check, combat, AFK, dungeon/raid/BG, cooldown, active-interaction guard)
- [x] `WorldCatalogue` — tagged NPC index, `LoadFromDB`, `GetNPCCandidates`, `GetCourierEntryForTheme`
- [x] `InteractionTemplateLibrary` — loads `dq_interaction_template` + `dq_interaction_choice` rows into memory structs
- [x] `NPCMatchingEngine` — `FindBestCourier` uses `dq_courier_template` theme→entry lookup
- [x] `DynamicQuestMgr` singleton — owns all subsystems, per-player state map, `Initialize`/`Shutdown`
- [x] `IMechanicModule` interface — `OnStart`, `OnChoice`, `OnComplete`, `OnFail`, `OnCleanup`, `OnUpdate`, `OnKill`, `OnLoot`, `OnDelivery`
- [x] `MechanicUtils` — `DeliverReward`, `ParseUint32`, `ParseFloat`, `ParseString`

### Mechanics
- [x] `MechanicGossipExchange` — immediate reward/deny/fetch, `DeliverReward`, `OnDelivery` for fetch return to courier
- [x] `MechanicKill` — kill N of optional entry, auto-complete at target count
- [x] `MechanicCollect` — loot N of item, auto-complete at target count
- [x] `MechanicFetchItem` — spawn destination NPC, 10-min timeout, deliver item to dest or courier
- [x] `MechanicCombatOptional` — make courier hostile on "attack" choice, reward on courier death
- [x] `MechanicRitual` — Phase 1: kill N warlocks before timer; Phase 2: elite demon spawns on expiry, kill for reward

### Scripts
- [x] `DQ_CourierAI` — intercept, gossip menu from template choices, fetch-return branch, 60s timeout, `JustDied` dispatch
- [x] `DQ_DestinationAI` — kneel emote, gossip check for item, `OnDestinationInteracted` dispatch, `JustDied` → fail
- [x] `DQ_PlayerScript` — `OnUpdate` (PCO + IEE tick), `OnZoneChange`, `OnLogin` (load history), `OnLogout` (save history), `OnKill`, `OnLootItem`
- [x] `DQ_WorldScript` — `OnStartup` loads WC + ITL
- [x] `DQ_CommandScript` — `.dq trigger`, `.dq stop`, `.dq status`, `.dq history`, `.dq debug on/off/gate`, `.dq catalogue zone/query`, `.dq reload`, `.dq bootstrap`

### Mechanic dispatch plumbing in DQMgr
- [x] `_mechanics` registry (`unordered_map<uint8, unique_ptr<IMechanicModule>>`)
- [x] `GetMechanic(templateId)` lookup
- [x] Dispatch in `OnCourierArrived`, `OnInteractionAccepted`, `OnPlayerKill`, `OnPlayerLoot`, `AbortInteraction`, `TickOnQuest`
- [x] `OnCourierKilled` → `mechanic->OnKill` (for CombatOptional)
- [x] `OnDestinationInteracted` → `mechanic->OnDelivery`

### Persistence
- [x] `character_dq_history` schema (`data/sql/base/db_characters/character_dq_history.sql`)
- [x] `OnPlayerLogin` — load history deque + cooldown from `next_trigger_time`
- [x] `OnPlayerLogout` — UPSERT save to `character_dq_history`

### SQL / Data
- [x] `dq_interaction_tables.sql` — `dq_interaction_template`, `dq_interaction_choice`, `dq_reward_pool`
- [x] `dq_courier_template.sql` — courier entries 900001-900005, destination entries 900011-900013 with correct `ScriptName` and faction=35
- [x] `dq_world_npc_catalogue.sql` — schema + index structure
- [x] `dq_npc_blacklist.sql` — schema + seed blacklist entries
- [x] `data/sql/interactions/deal_with_devil.sql` — 3-choice demon deal template
- [x] `data/sql/interactions/hungry_child.sql` — bread fetch/give template

### loader.cpp
- [x] `AddSC_DQ_DestinationAI()` added

---

## TODO

### P0 — Broken / Placeholder (must fix before first test run)

- [x] **`.dq history clear` is a placeholder** — wired to `DynamicQuestMgr::ClearHistory`
- [x] **`dq_bootstrap_tier1_quests()` stored procedure** — stub added to `dq_bootstrap.sql`
- [x] **`courier_template` display IDs** — fixed. `creature_template_model` INSERT added with verified display IDs (2575=succubus, 221=boy, 4041=Forsaken, 87=peasant, 179=kodo, 3167=SW guard). `modelid1` column no longer exists in this AzerothCore version.

### P1 — Missing Content / Features

- [x] **`data/sql/interactions/warlock_ritual.sql`** — created (entry:0 for any kill, phase2_entry:12922, verify in DB)
- [x] **`.dq episode` subcommand group** — `list`, `skip`, `fail`, `spawn` all implemented

- [x] **More interaction content** — `collect_herbs.sql` (MechanicCollect), `patrol_kill.sql` (MechanicKill), `lost_caravan.sql` (MechanicFetchItem)

### P2 — Quality / Robustness

- [x] **`MechanicFetchItem` destination spawn** — now uses `GetNearPoint` (VMAP-aware Z)
- [x] **`PlayerContext::inventoryHas[]` population** — bag scan implemented in `PlayerContextObserver.cpp`
- [x] **VMAP outdoor check** — uses `player->IsOutdoors()` which is the correct AzerothCore wrapper
- [x] **Cooldown UI feedback** — `FormatDuration()` helper added; `.dq status` now shows "14m 32s"
- [x] **Flying/swimming gates** — `isFlying` and `isSwimming` added to context + eligibility engine

- [x] **Concurrent multi-player safety** — all DB calls are synchronous `CharacterDatabase.Query/Execute` on the world thread. No `AsyncQuery` used anywhere. `_states` is only touched from the world thread. No mutex needed.

- [ ] **`consecutive_tier1` escalation to Tier 2** — logic is in place (`TryTrigger` boosts tier2Pct when consecutiveTier1 >= 3). Verify in a test run that the counter actually increments and persists across sessions.

### P3 — Polish / Production

- [x] **Full world scan bootstrap** — `dq_bootstrap_catalogue()` scans ALL of `creature_template` with no zone restriction. Already a full world scan.

- [x] **Log channel wiring audit** — all channels correct. Two minor fixes applied: `DeliverReward` now logs to `module.dynamicquests.mechanic`; "no courier found" now logs to `module.dynamicquests.nme`.

- [x] **conf dist completeness** — `conf/mod_dynamicquests.conf.dist` created with all config keys and defaults.
  ```ini
  DynamicQuests.Enable
  DynamicQuests.Tier1MinTimerSeconds / Tier1MaxTimerSeconds
  DynamicQuests.Tier2MinTimerSeconds / Tier2MaxTimerSeconds
  DynamicQuests.ActivityWindowSeconds
  DynamicQuests.HistorySize
  DynamicQuests.CourierSpeedBonus
  DynamicQuests.CourierTimeoutSeconds
  DynamicQuests.Tier2Chance
  DynamicQuests.QuestIdRangeStart / QuestIdRangeEnd
  DynamicQuests.VerboseLogging
  ```
  Verify all keys are present and have sensible defaults.

- [ ] **`.dq status` diagnostic panel** — current implementation should match the plan output format exactly:
  ```
  === DynamicQuests+ | Player: Arthas ===
  State      : IDLE (cooldown: 14m 32s)
  Context    : TRAVELING (340y/5min, 0 kills)
  ...
  Gate checks:
    [PASS] Not AFK
    [FAIL] Cooldown (ready in 14m)
  ...
  ```
  Verify all gate check lines are printed, and NME last-query result is included.

- [ ] **In-game smoke test checklist** (before considering Phase 4 complete):
  - `.dq catalogue query demon,humanoid 85` → returns valid non-boss demon entries for Tirisfal
  - `.dq trigger` → NME picks contextually correct NPC, courier spawns ahead, intercepts
  - Demon deal gossip: all 3 choice branches execute (reward / despawn / combat)
  - Child bread: "here you go" consumes item; "fetch" creates destination NPC
  - Enter cave → VMAP blocks trigger; trigger resumes on exit
  - `.dq status` → all gate checks displayed, history shown
  - `.dq debug on` → verbose candidate list, weights, winner in log
  - Bootstrap re-run → cleans previous catalogue, regenerates cleanly
  - `.dq episode fail` → warlock ritual timer forces Branch B
  - Logout mid-interaction → spawns cleaned, state reset

---

## Build

Build is skipped unless explicitly requested (per CLAUDE.md). To build:
```bash
cd build && cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/azeroth-server -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSCRIPTS=static -DMODULES=static && make -j$(nproc) && make install
```
