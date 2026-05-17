# DynamicQuests+ — Design Bible

Last updated: 2026-05-17

---

## Core Philosophy

The engine makes the mechanics happen. The author writes the story.

Anyone with MySQL access should be able to sit down, write a quest in plain language,
tag it with context and mood, define the beat structure and reward, and have the engine
bring it to life — correct models, emotes, behavior, text, spawns, and all.

The author never writes:
- Display IDs
- Emote numbers
- Creature entries
- Spawn coordinates
- Combat AI

The engine resolves all of that from the author's tags and text.

The engine runs at 5 RPM and 5000 RPM. A single-beat child asking for bread and a
9-beat demon bargainer with escalating costs run through the exact same pipeline.

---

## What the Author Provides

```
who:      spirit bear
where:    forest
beats:    1
story:    "I am the heart of this forest. Show me your healing powers
           or you will feed the soil forever."
emotion:  weary
hostile:  if quest fails
mechanic: cast (heal)
reward:   nature_rare
```

That is the complete authoring contract. Everything else is the engine's job.

---

## Mechanical Primitives

The six things a player can DO in a beat. Everything else is a combination.

| Primitive | What happens                                              |
|-----------|-----------------------------------------------------------|
| Witness   | Player is present, event plays out, no action required    |
| Kill      | Player kills N creatures of a type                        |
| Activate  | Player clicks a game object or NPC                        |
| Courier   | Player carries an item, delivers it somewhere             |
| Fight     | NPC turns hostile, resolves at HP threshold — not death   |
| Cast      | Player uses an ability on the NPC (heal, buff, purge)     |

---

## The Nine Systems

### System 1 — Trigger & Context
Decides when and where a quest fires. Reads zone, level, class, history, area tags.
Builds the context profile that all downstream systems consume.
**Status: exists, mostly solid.**

### System 2 — Selection
Takes the context profile and picks the right story from the DB.
Scores candidates against context tags. Filters by player history so nothing repeats.
**Status: partially exists inside TryTrigger, needs extraction and cleanup.**

### System 3 — Catalogue
Pure lookup. Tags in, game data IDs out. No world interaction, no side effects.
- NPC display IDs — `dq_world_npc_catalogue` (exists, needs polish)
- Game object entry IDs — `dq_object_catalogue` (missing)
- Dummy spell IDs — `dq_dummy_spell` (missing)
**Status: NPC catalogue exists. Object and spell catalogues missing.**

### System 4 — NPC Builder
Takes a creature entry from Catalogue and constructs a complete NPC profile for this
specific quest instance. Also operates on living NPCs mid-quest — faction flips,
HP state changes, behavior transitions.

Responsibilities:
- Level scaling (player level + `npc_level_offset` beat column)
- Faction assignment (friendly / hostile / neutral, changeable mid-quest)
- HP scaling and fight threshold setting
- Inherit base creature spells and combat AI
- Emit a built profile Spawn can place and Animation can dress

**Status: does not exist as a system. Logic is scattered.**

### System 5 — Spawn
Takes fully built NPC and GO profiles from NPC Builder and places them in the world.
Owns phase bit assignment, GUID tracking, and cleanup.
Does not know what the NPC does — that is NPC Builder's job.

Responsibilities:
- Creature spawning (exists)
- Game object spawning (missing)
- Phase bit management (partial)
- Cleanup on completion, failure, abort, and logout

**Status: creature spawning exists. GO spawning missing. Cleanup needs consolidation.**

### System 6 — Animation
Owns everything visual that is not a model or a stat.
Called by Spawn (entry), Resolution (exit), and Action (objective moments).

Responsibilities:
- Entry sequences: from_portal, fade_in, materialize, approaches
- Exit sequences: fade_out, portal_exit, dissolve
- Persistent auras: glow effects applied at spawn, removed at despawn
- Beat transition visuals: objective complete feedback, emotion change cue
- Dummy spell catalogue lookup by context tags

**Status: spawn_style partially exists. Animation system as a whole does not exist.**

### System 7 — Dialogue
Owns the text window completely. Data-driven, no hardcoded branches.

Responsibilities:
- Read beat text fields, build gossip page dynamically
- Generate buttons based on mechanic type and passive flag
- Item prereq check — [Give Item] button only appears if player has the item
- Text variant pool fallback — if author left chase/ambient text empty,
  pull from `dq_text_variant_pool` by emotion + context tags
- Know which mechanics are passive (cast/heal register at beat start, not button click)
  vs active (give item, fight, next register on button click)

**Status: exists but hardcoded. Needs full rewrite.**

### System 8 — Action & Mechanic
Receives button clicks from Dialogue and routes to handlers.
Passive mechanics (cast, heal) register at beat start and listen continuously.
Active mechanics (give item, fight, next) activate on button click.

Handlers:
- Kill handler: spawn kill targets if needed, count deaths to objective
- Fight handler: calls NPC Builder to flip faction mid-quest, threshold listener, concede
- Cast/Heal handler: passive OnHeal / OnSpellHit listener on quest NPC GUID
- Give Item handler: inventory check, item removal, mark complete
- Activate handler: spawn GO if needed, OnActivate listener
- Next handler: advance beat
- Finish handler: close interaction

**Status: scattered across HandleGossipSelect and MechanicArchetype. Needs extraction.**

### System 9 — Resolution
Owns what happens when a beat ends — success or failure. Choice-aware.

Responsibilities:
- Receive completion signal with path identifier (which button was pressed)
- Route to next beat or close arc using `choice_success_transition` /
  `choice_fail_transition` beat columns
- Apply reward on success path
- Trigger fight handler on failure if `hostile_on_fail` is set
- Fire exit animation before despawn
- Consolidated cleanup of all spawns and phase bits

**Status: partially exists scattered across MechanicArchetype and DynamicQuestMgr.**

---

## Build Order

### Phase 1 — Schema & Reference Data
*Foundation. Everything else reads from this.*

- [ ] Schema migration: add new beat columns —
      `mechanic_passive`, `choice_success_transition`, `choice_fail_transition`,
      `entry_animation`, `exit_animation`, `entry_spell`, `exit_spell`,
      `aura_spell`, `fight_threshold`, `cast_school`, `npc_level_offset`
- [ ] `dq_dummy_spell` table: tags → spell_id
      Pre-populate with AC dummy spells (portals, nature burst, holy glow, shadow pulse, fade)
- [ ] `dq_object_catalogue` table: tags → GO entry ID
      (shrines, saplings, portals, crates, ritual objects)
- [ ] `dq_text_variant_pool` table: emotion + context_tags + text_type → text variants
      Engine draws from this when author left chase/ambient fields empty

### Phase 2 — NPC Builder
*Before Spawn because Spawn consumes it.*

- [ ] NPC Builder initial profile: level scaling, faction, HP scaling,
      inherit base creature spells and AI, fight threshold setting
- [ ] NPC Builder mid-quest update: faction flip, HP state change, behavior change
      on an already-living creature (required by fight path and hostile-on-fail)

### Phase 3 — Spawn System Revamp
- [ ] Extend Spawn to handle game objects: GO spawning, GUID tracking, cleanup
- [ ] Clean entry animation handoff: after placing NPC or GO, pass to Animation system

### Phase 4 — Animation System
- [ ] Entry sequences: from_portal, fade_in, materialize, approaches
- [ ] Exit sequences: fade_out, portal_exit, dissolve
- [ ] Persistent auras: apply dummy aura spell at spawn, remove at despawn
- [ ] Beat transition effects: objective complete visual, emotion change cue
- [ ] Dummy spell catalogue lookup: context tags → correct dummy spell

### Phase 5 — Passive Mechanic Detection
*Must exist before Dialogue — Dialogue needs to know which mechanics are passive.*

- [ ] OnHeal hook: detect player healing a specific quest NPC GUID
- [ ] OnSpellHit / OnSpellCast hook: detect player casting on quest NPC,
      filter by school from `cast_school` beat column
- [ ] OnTakeDamage threshold hook: detect NPC HP dropping below `fight_threshold` %

### Phase 6 — Dialogue System Rewrite
- [ ] Data-driven gossip builder: beat text → gossip page, mechanic type → buttons,
      passive flag awareness, item prereq check, no hardcoded branches
- [ ] Text variant pool integration: pull from `dq_text_variant_pool` when fields empty

### Phase 7 — Action Router
- [ ] Button click routing table: button ID → handler, passive vs active registration
- [ ] Full mechanic handler set: Kill, Fight, Cast/Heal, Give Item, Activate, Next, Finish

### Phase 8 — Fight Handler
- [ ] NPC goes hostile: NPC Builder mid-quest faction flip, combat AI enabled,
      Animation transition effect
- [ ] Threshold detection and concede: at `fight_threshold` HP → neutral faction,
      concede animation, report to Resolution with fight path flag
- [ ] Hostile-on-fail: beat timer expires → fight handler activates with fail flag

### Phase 9 — Resolution Consolidation
- [ ] Choice-aware completion: path identifier routes via
      `choice_success_transition` / `choice_fail_transition` columns
- [ ] Cleanup consolidation: spawns, phase bit release, exit animation — one place
- [ ] Hostile fallback: `hostile_on_fail` set + beat fails → hand to fight handler

### Phase 10 — Selection Cleanup
- [ ] Extract Selection from TryTrigger into independent system
- [ ] Clean scoring: context match quality weighting, anti-repeat logic

### Phase 11 — Data & Integration
- [ ] Update existing stories (HungryChild, Ileana, HerbSeller) to new beat columns
- [ ] Author ghost warrior quest — first full pipeline test (3 beats, kill, resolution)
- [ ] Author spirit bear quest — first cast/heal and hostile-on-fail test
- [ ] Full smoke test: all story types through the complete pipeline
- [ ] In-game testing pass

---

## What Currently Exists

### Infrastructure

| System           | Status                                                       |
|------------------|--------------------------------------------------------------|
| DQLog            | Complete — category-aware macros over AC logger hierarchy    |
| DQClientSession  | Complete — owns all gossip send/receive/validate/expire      |

### Narrative Systems

| System            | Status                                       |
|-------------------|----------------------------------------------|
| Trigger & Context | Exists, solid                                |
| Selection         | Partial — embedded in TryTrigger             |
| Catalogue         | NPC only — object + spell catalogues missing |
| NPC Builder       | Does not exist as a system                   |
| Spawn             | Creatures only — no GOs, cleanup scattered   |
| Animation         | spawn_style partial — system does not exist  |
| Dialogue          | Exists but hardcoded — needs full rewrite    |
| Action & Mechanic | Scattered — needs extraction into handlers   |
| Resolution        | Partial — scattered across multiple files    |

---

## What Was Added in v8 (2026-05-17)

**DQLog** (`src/core/DQLog.h`) — header-only structured logging layer.
All module logging now goes through `DQ_LOG_DEBUG/INFO/WARN/ERROR(cat, player, fmt, ...)`.
`.dq debug on/off` wires to `sLog->SetLogLevel("module.dynamicquests", ...)` and propagates
to all six subcategories automatically. `cfg_verbose` removed.

**DQClientSession** (`src/core/DQClientSession.h/.cpp`) — centralised gossip communication.
Owns Open (build menu + stamp non-zero menuId + send), Validate (menuId + GUID check),
Tick (expiry countdown in DynamicQuestMgr::OnPlayerUpdate), and Close.
`DQPendingSession` struct lives in `PlayerDQState`; `menuId == 0` is the no-session sentinel.

**API changes:**
- `DQDialogueMgr::OpenBeatGossip(Player*, Creature*, ...)` → `BuildBeatMenu(Player*, ...)`
  Removed `Creature*` parameter and `SendGossipMenuFor` call — sending is `DQClientSession::Open`'s job.
- `DQ_CourierCreatureAI::_waitTimer` removed — gossip session expiry is now owned by
  `DQClientSession::Tick` called from `DynamicQuestMgr::OnPlayerUpdate`.

---

## What Was Deprecated in v7 (2026-05-17)

The following were deleted as part of the everything-as-archetype refactor.
Documented here so future contributors understand what was removed and why.

- `MechanicHungryChild` / `MechanicSuccubus` — hardcoded per-story C++ mechanics
- `DQ_HungryChildAI` / `DQ_SuccubusAI` — hardcoded per-story creature AI
- `InteractionTemplateLibrary` — legacy template system replaced by beat data
- `dq_interaction_template` / `dq_interaction_choice` / `dq_text_variant` — legacy tables

All content from the above now lives as SQL data in `dq_archetype` / `dq_archetype_beat`.
No new hardcoded per-story C++ will ever be written. New content = new SQL rows.

---

## The Law

> New content is always SQL data, never new C++.
> The engine brings the story to life. The author writes the story.
