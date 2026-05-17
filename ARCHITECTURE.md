# DQ+ Architecture Documentation — Phases 2–9

Last updated: 2026-05-17

---

## AC Integration Reference — Exact Patterns

These are the exact AC APIs and conventions the implementation must follow.
All code in this module must be indistinguishable in style from upstream AC.

### Code Style (from upstream AC source)
- Class and function definitions: opening brace on **new line**
- `if / for / while / switch`: opening brace on **same line**
- Private member variables: `_camelCase` prefix
- Constants and enum values: `SCREAMING_SNAKE_CASE`
- No `std::` prefix on standard library types in `.cpp` files (pulled in via headers)
- 4-space indentation, no tabs

### Singleton Pattern
```cpp
// .h
class MyMgr
{
public:
    static MyMgr* instance();
    void LoadFromDB();
private:
    MyMgr() = default;
    bool _loaded = false;
};
#define sMyMgr MyMgr::instance()

// .cpp
MyMgr* MyMgr::instance()
{
    static MyMgr instance;
    return &instance;
}
```

### Script Registration
```cpp
// PlayerScript
class DQ_MyScript : public PlayerScript
{
public:
    DQ_MyScript() : PlayerScript("DQ_MyScript",
    {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_SPELL_CAST,
    }) { }

    void OnPlayerUpdate(Player* player, uint32 diff) override { ... }
};

// UnitScript — note: second parameter is bool addToScripts
class DQ_MyUnitScript : public UnitScript
{
public:
    DQ_MyUnitScript() : UnitScript("DQ_MyUnitScript", true,
    {
        UNITHOOK_ON_HEAL,
        UNITHOOK_ON_DAMAGE,
    }) { }
};

void AddSC_DQ_MyScript()
{
    new DQ_MyScript();
    new DQ_MyUnitScript();
}
```

### Faction Constants (SharedDefines.h)
```cpp
FACTION_MONSTER  = 14    // generic hostile, attacks players
FACTION_FRIENDLY = 35    // friendly to all
```
Always use named constants. Never use raw numbers for factions.

### Spell School Values (SharedDefines.h)
The `castSchool` beat column stores **SpellSchools enum** values (not masks):
```
SPELL_SCHOOL_NORMAL = 0   (physical — also means "any" in our system)
SPELL_SCHOOL_HOLY   = 1
SPELL_SCHOOL_FIRE   = 2
SPELL_SCHOOL_NATURE = 3
SPELL_SCHOOL_FROST  = 4
SPELL_SCHOOL_SHADOW = 5
SPELL_SCHOOL_ARCANE = 6
```
School **mask** bits (what SpellInfo->GetSchoolMask() returns):
```
SPELL_SCHOOL_MASK_HOLY   = 0x02  (1 << 1)
SPELL_SCHOOL_MASK_FIRE   = 0x04  (1 << 2)
SPELL_SCHOOL_MASK_NATURE = 0x08  (1 << 3)
...
```
Conversion: `if (beat->castSchool == 0 || (spellInfo->GetSchoolMask() & (1u << beat->castSchool)))`

### Key API Signatures (confirmed from source)
```cpp
// Spell cast — triggered=true skips cooldowns/costs/interrupts (for visual/dummy spells)
creature->CastSpell(creature, spellId, true);     // self-cast, triggered
creature->CastSpell(player,   spellId, true);     // cast on player, triggered

// Persistent aura
unit->AddAura(spellId, target);                   // apply
unit->RemoveAurasDueToSpell(spellId);             // remove

// Display model — second param is float scale (default 1.f), NOT a bool
creature->SetDisplayId(modelId);                  // keep native scale
creature->SetDisplayId(modelId, 1.5f);            // 150% scale

// Despawn — use duration literals (Milliseconds, Seconds, ms suffix)
creature->DespawnOrUnsummon();                    // immediate
creature->DespawnOrUnsummon(2500ms);              // 2.5s delay
creature->DespawnOrUnsummon(Milliseconds(2500));  // same

// Text
creature->Say(text,  LANG_UNIVERSAL);             // broadcast to nearby
creature->Say(text,  LANG_UNIVERSAL, player);     // directed at specific player
creature->Yell(text, LANG_UNIVERSAL);

// Unit flags — use named constants from UnitDefines.h
creature->AddUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
creature->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
creature->RemoveUnitFlag(UNIT_FLAG_PACIFIED);

// Combat
creature->SetFaction(FACTION_MONSTER);
creature->CombatStop(true);                       // true = clear threat list

// AI attack start
creature->AI()->AttackStart(player);
```

### Inventory Search Pattern (Player.cpp)
```cpp
for (uint8 bag = INVENTORY_SLOT_BAG_0; bag < INVENTORY_SLOT_BAG_END; ++bag)
{
    for (uint8 slot = 0; slot < MAX_BAG_SIZE; ++slot)
    {
        Item* item = player->GetItemByPos(bag, slot);
        if (!item)
            continue;
        ItemTemplate const* proto = item->GetTemplate();
        if (proto->Class == ITEM_CLASS_CONSUMABLE && proto->SubClass == ITEM_SUBCLASS_FOOD)
            return item;
    }
}
```

### DB Query Pattern
```cpp
void MyMgr::LoadFromDB()
{
    uint32 oldMSTime = getMSTime();
    _data.clear();

    QueryResult result = WorldDatabase.Query("SELECT col1, col2 FROM my_table ORDER BY id");
    if (!result)
    {
        LOG_WARN("module.dynamicquests", "MyMgr: table is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        MyEntry entry;
        entry.id    = fields[0].Get<uint32>();
        entry.name  = fields[1].Get<std::string>();
        _data.push_back(std::move(entry));
        ++count;
    } while (result->NextRow());

    LOG_INFO("module.dynamicquests", "MyMgr: Loaded {} entries in {} ms.",
        count, GetMSTimeDiffToNow(oldMSTime));
}
```

---

## System Dependency Map

```
ArchetypeMgr (data, read-only)
  └─ DQNPCBuilder         ← resolves spawn profile from beat data
       └─ DQSpawnSystem   ← physical spawn (exists, extended with levelOffset)
            └─ DQAnimationMgr  ← visual post-spawn / pre-despawn effects
                 └─ MechanicArchetype  ← orchestrates the full beat lifecycle
                      ├─ DQDialogueMgr      ← builds gossip menus from beat data
                      ├─ DQPassiveHooks     ← UnitScript/PlayerScript passive detection
                      └─ DynamicQuestMgr   ← state machine, resolution, cleanup
```

**New files:** `DQNPCBuilder`, `DQAnimationMgr`, `DQDialogueMgr`, `DQ_PassiveHooks`
**Modified:** `DQSpawnSystem`, `MechanicArchetype`, `DQ_CourierAI`, `DynamicQuestMgr`, `loader.cpp`
**Deleted (legacy):** `MechanicHungryChild.*`, `MechanicSuccubus.*`, `DQ_HungryChildAI.cpp`,
                       `DQ_SuccubusAI.*`, `InteractionTemplateLibrary.*`

---

## DQLog — Structured Logging

**File:** `src/core/DQLog.h` (header-only)

### Purpose
Thin wrapper over AC's native `LOG_*` macros. Adds two things the raw macros lack:
subcategory filtering per system, and automatic `[PlayerName]` prefixing so every line
in the server log is traceable to a specific player without any extra call-site work.

### Category hierarchy
```
module.dynamicquests              ← parent (set by .dq debug on/off)
  ├─ module.dynamicquests.state   ← state machine transitions, login/logout
  ├─ module.dynamicquests.courier ← spawn, movement, arrival, timeout
  ├─ module.dynamicquests.session ← gossip open/validate/close/expire
  ├─ module.dynamicquests.beat    ← beat start/complete/fail
  ├─ module.dynamicquests.spawn   ← DQSpawnSystem events
  └─ module.dynamicquests.context ← tag resolution
```

AC's logger hierarchy walks upward automatically:
`module.dynamicquests.courier` → `module.dynamicquests` → `module` → `root`

### .dq debug on/off
```cpp
// on:
sLog->SetLogLevel("module.dynamicquests", static_cast<int32>(LOG_LEVEL_DEBUG), true);
// off:
sLog->SetLogLevel("module.dynamicquests", static_cast<int32>(LOG_LEVEL_INFO), true);
```
One call on the parent propagates to all six subcategories. No per-subcat calls needed.

### worldserver.conf (optional static override)
```ini
Logger.module.dynamicquests=4,Console Server   # 4 = INFO (default)
```

### Usage
```cpp
// player != nullptr → "[PlayerName] " is prepended automatically
DQ_LOG_DEBUG(DQ_LOG_CAT_COURIER, player, "Courier arrived. Template={}", ps.activeTemplateId);
DQ_LOG_INFO (DQ_LOG_CAT_STATE,   nullptr, "Initialized. CatalogueEntries={}", n);
DQ_LOG_WARN (DQ_LOG_CAT_SESSION, player, "menuId mismatch. sent={} received={}", s, r);
DQ_LOG_ERROR(DQ_LOG_CAT_STATE,   player, "No template found (level={}, zone={}).", lv, z);
```

### Adding logging to a new file
1. `#include "DQLog.h"` (includes `Log.h` transitively — do not include `Log.h` directly)
2. Pick the closest subcategory constant (`DQ_LOG_CAT_*`)
3. Use `DQ_LOG_DEBUG` for trace/diagnostic lines, `DQ_LOG_INFO` for lifecycle events

---

## DQClientSession — Client Communication Layer

**Files:** `src/core/DQClientSession.h`, `src/core/DQClientSession.cpp`

### Purpose
Single owner of every gossip send/receive for DQ+ interactions. Before this system
existed, gossip sending lived in `DQDialogueMgr`, gossip receiving was split between
`DQ_CourierScript` and `DQ_CourierCreatureAI`, timeout lived in the AI's `_waitTimer`,
and session validation was implicit (no record of what was sent). There was no single
place that could answer: "does this response belong to the menu we sent?"

The immediate motivation: `GossipMenu::ClearMenu()` does NOT reset `_menuId`, and
`SetMenuId()` was never called, so every gossip packet left the server with `menuId=0`.
The WoW 3.3.5a client may require a non-zero `menuId` to render the gossip window.
`DQClientSession::Open` stamps a unique non-zero `menuId` before sending, fixing this.

### DQPendingSession — lifecycle
```cpp
struct DQPendingSession
{
    ObjectGuid npcGuid;   // creature the gossip was sent from
    uint32     menuId;    // non-zero = session active; 0 = no session (sentinel)
    uint32     expiryMs;  // ms remaining before Tick() fires expiry
};
```
Stored in `PlayerDQState::pendingSession`. Default-constructed with `menuId=0` = closed.

| State    | menuId | Meaning                        |
|----------|--------|--------------------------------|
| Closed   | 0      | No gossip waiting for response |
| Open     | 1-65535| Gossip was sent, awaiting reply|
| Expired  | reset to 0 after Tick() fires  |

### API
```cpp
// Build menu + SetMenuId + Send. Fills |out| with session identity.
// expiryMs defaults to 120s; caller passes sDQMgr->cfg_courierTimeoutMs.
DQClientSession::Open(player, courier, def, beat, ps.pendingSession, expiryMs);

// Validate an incoming gossip select. Returns false on npcGuid or menuId mismatch.
DQClientSession::Validate(player, creature, incomingMenuId, ps.pendingSession);

// Advance expiry countdown. Returns true once when session expires.
DQClientSession::Tick(ps.pendingSession, diff);

// Zero the session.
DQClientSession::Close(ps.pendingSession);
```

### menuId generation
`NextMenuId()` uses a module-static `std::atomic<uint32>` counter. Monotonically
increasing, skips 0, wraps at 0xFFFF (fits in the WoW gossip packet's uint32 field).

### DQDialogueMgr responsibility boundary
`DQDialogueMgr::BuildBeatMenu(Player*, const ArchetypeDef&, const ArchetypeBeat&)` — builds only:
- `ClearGossipMenuFor`
- `AddGossipItemFor` (buttons based on mechanic type)

Does NOT call `SendGossipMenuFor`. `DQClientSession::Open` calls `BuildBeatMenu` first,
then stamps `menuId`, then sends. This ordering guarantees the packet carries the ID.

### Data flow
```
HandleGossipHello
  └─ DQClientSession::Open
       ├─ DQDialogueMgr::BuildBeatMenu  (build items, no send)
       ├─ GossipMenu::SetMenuId(N)       (stamp unique non-zero ID)
       └─ SendGossipMenuFor              (SMSG_GOSSIP_MESSAGE → client)

                        client echoes menuId in CMSG_GOSSIP_SELECT_OPTION

sGossipSelect (AI override)
  └─ caches menuId as _lastMenuId

HandleGossipSelect
  ├─ DQClientSession::Validate(_lastMenuId, pendingSession)  → false = discard
  ├─ DQClientSession::Close(pendingSession)
  └─ sDQMgr->OnInteractionAccepted / OnInteractionDeclined

DynamicQuestMgr::OnPlayerUpdate (DQ_STATE_DELIVERING)
  └─ DQClientSession::Tick(pendingSession, diff)
       └─ on expiry → AbortInteraction (→ Close inside Abort)
```

### Adding a new gossip response type
1. Assign a new action constant (avoid 0–3 = choices, 0xFE = decline)
2. Add the button in `DQDialogueMgr::BuildBeatMenu` via `AddGossipItemFor`
3. Route the new action in `HandleGossipSelect` before the existing `if (action == DQ_GOSSIP_DECLINE)` block
4. Validation and session close are already handled generically — no changes needed there

---

## Phase 2 — NPC Builder

**Files:** `src/core/DQNPCBuilder.h`, `src/core/DQNPCBuilder.cpp`
**Also modifies:** `src/core/DQSpawnSystem.h`, `src/core/DQSpawnSystem.cpp`

### Problem
NPC profile construction (entry lookup, level, faction, HP) is scattered.
Mid-quest mutations (faction flip, HP drain) have no central home.

### DQSpawnDesc extension
```cpp
int8 levelOffset = 0;   // applied in ApplyScaling: clamp(player_level + offset, 1, 80)
```

### DQNPCProfile
```cpp
struct DQNPCProfile {
    DQSpawnDesc spawnDesc;
    uint8  concedePct = 15;   // beat.fightThreshold
    uint32 entrySpell = 0;    // beat.entrySpell
    uint32 exitSpell  = 0;    // beat.exitSpell
    uint32 auraSpell  = 0;    // beat.auraSpell
};
```

### DQNPCBuilder interface
```cpp
class DQNPCBuilder {
public:
    static DQNPCProfile BuildProfile(const ArchetypeDef& def,
                                     const ArchetypeBeat& beat,
                                     Player* player);
    static void FlipToHostile(Creature* npc, Player* player);
    static void FlipToFriendly(Creature* npc);
    static void SetHPPercent(Creature* npc, uint8 percent);
};
```

### BuildProfile logic
1. **Entry + DisplayId:** if `beat.displayId != 0` → social courier entry + explicit displayId.
   Else: parse `def.appearance` CSV → `GetCourierEntryForTheme(tags[0])` + `GetWorldCourierDisplayId(...)`.
2. **Level:** `desc.scaleLevel = true`, `desc.levelOffset = beat.npcLevelOffset`
3. **Faction:** `desc.faction = 0` (Fight Handler flips)
4. **Combat:** `desc.attackPlayer = false` (Fight Handler starts)
5. Fill `concedePct`, `entrySpell`, `exitSpell`, `auraSpell` from beat

### FlipToHostile
```cpp
npc->SetFaction(FACTION_MONSTER);
npc->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
npc->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
npc->AI()->AttackStart(player);
```

### FlipToFriendly
```cpp
npc->CombatStop(true);
npc->SetFaction(FACTION_FRIENDLY);
npc->AddUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
```

---

## Phase 3 — Spawn Animation Handoff

**No new files.** Wires NPC Builder output to Animation system in `MechanicArchetype`.

- `OnStart` (after courier pointer): call `sDQAnimation->PlayEntry(courier, beat, profile)`
- `CompleteBeat` (before AdvanceBeat): call `sDQAnimation->PlayExit(courier, beat, profile)`
- `OnCleanup` (before despawn): call `sDQAnimation->RemovePersistentAura(courier, inst.auraSpell)`

---

## Phase 4 — Animation System

**Files:** `src/core/DQAnimationMgr.h`, `src/core/DQAnimationMgr.cpp`

```cpp
class DQAnimationMgr {
public:
    static DQAnimationMgr* instance();

    void PlayEntry(Creature* npc, const std::string& style, uint32 spellId);
    void PlayExit(Creature* npc, const std::string& style, uint32 spellId, Milliseconds delay = 2500ms);
    void ApplyPersistentAura(Creature* npc, uint32 spellId);
    void RemovePersistentAura(Creature* npc, uint32 spellId);
    void PlayBeatTransition(Creature* npc, Player* player);
};
#define sDQAnimation DQAnimationMgr::instance()
```

### Entry style dispatch
| style          | action                                                                |
|----------------|-----------------------------------------------------------------------|
| `"approaches"` | MoveFollow already set by SpawnCourier; cast `spellId` if non-zero  |
| `"from_portal"`| `npc->CastSpell(npc, spellId, true)` (portal visual immediate)      |
| `"fade_in"`    | `npc->CastSpell(npc, spellId, true)`                                 |
| `"materialize"`| `npc->CastSpell(npc, spellId, true)`                                 |

### Exit style dispatch
| style           | action                                             |
|-----------------|----------------------------------------------------|
| `"despawn"`     | `DespawnOrUnsummon(delayMs)`                       |
| `"fade_out"`    | Cast spellId, then `DespawnOrUnsummon(delayMs)`    |
| `"portal_exit"` | Cast spellId, then `DespawnOrUnsummon(delayMs)`    |
| `"dissolve"`    | Cast spellId, then `DespawnOrUnsummon(delayMs)`    |

### Persistent aura
- Apply: `npc->AddAura(spellId, npc)`
- Remove: `npc->RemoveAurasDueToSpell(spellId)`

### Beat transition
- `npc->HandleEmoteCommand(EMOTE_ONESHOT_APPLAUD)` — objective complete cue

---

## Phase 5 — Passive Mechanic Detection

**New file:** `src/scripts/DQ_PassiveHooks.cpp`
**Modifies:** `src/core/DynamicQuestMgr.h`, `src/core/DynamicQuestMgr.cpp`

### DynamicQuestMgr additions
```cpp
// Reverse lookup for passive hooks (O(1) per damage/heal event)
private:
    std::unordered_map<uint64, uint64> _courierToPlayer;  // courier raw guid → player raw guid

public:
    void RegisterActiveCourier(ObjectGuid courier, ObjectGuid player);
    void UnregisterActiveCourier(ObjectGuid courier);
    void OnCourierHealed(ObjectGuid courierGuid, Player* healer, uint32 amount);
    void OnCourierTookDamage(ObjectGuid courierGuid, uint32 remainingHealthPct);
    void OnPlayerCastOnCourier(Player* player, Spell* spell);
    void OnCourierConceded(Player* player);
```

`RegisterActiveCourier` called from `SpawnCourier` after courier GUID is known.
`UnregisterActiveCourier` called from `ReleasePhase`.

### DQ_PassiveHooks.cpp — two script classes

**`DQ_UnitHooks : public UnitScript`**
- `OnHeal(healer, receiver, gain)`: if healer is player + receiver is DQ courier + beat=CAST → `sDQMgr->OnCourierHealed(...)`
- `OnDamage(attacker, victim, damage)`: if victim is DQ courier → compute remaining% → `sDQMgr->OnCourierTookDamage(...)`

**`DQ_SpellCastHook : public PlayerScript`**
- `OnPlayerSpellCast(player, spell, skipCheck)` → `sDQMgr->OnPlayerCastOnCourier(player, spell)`

### Handler logic
- `OnCourierHealed`: beat.mechanic == CAST + school check + → `CompleteBeat`
- `OnCourierTookDamage`: beat.mechanic == FIGHT + remainPct <= concedePct → `OnCourierConceded`
- `OnPlayerCastOnCourier`: check target GUID == courierGuid + school mask → `CompleteBeat`

### InteractionInstance additions
```cpp
uint8  concedePct = 15;   // from DQNPCProfile.concedePct, set in OnStart
uint32 auraSpell  = 0;    // from beat.auraSpell, for cleanup
```

---

## Phase 6 — Dialogue System Rewrite

**New files:** `src/core/DQDialogueMgr.h`, `src/core/DQDialogueMgr.cpp`
**Modifies:** `src/scripts/DQ_CourierAI.cpp`

### DQDialogueMgr
```cpp
class DQDialogueMgr {
public:
    static DQDialogueMgr* instance();
    void LoadFromDB();   // SELECT from dq_text_variant_pool at startup

    void OpenBeatGossip(Player* player, Creature* npc,
                        const ArchetypeDef& def, const ArchetypeBeat& beat);

    std::string GetVariantText(const std::string& emotion,
                               const std::string& contextTags,
                               const std::string& textType) const;
};
#define sDQDialogue DQDialogueMgr::instance()
```

### OpenBeatGossip button logic

| mechanic  | passive | buttons                                            | actions        |
|-----------|---------|----------------------------------------------------|----------------|
| WITNESS   | 0       | [Continue]                                         | 0              |
| COURIER   | 0       | [Give item] (gated by itemPrereq) or [I'll find it]| 0              |
| KILL      | 0       | [I'll handle it]                                   | 0              |
| GOTO      | 0       | [I'm on my way]                                    | 0              |
| ACTIVATE  | 0       | [On it]                                            | 0              |
| FIGHT     | 0       | [Let's fight]                                      | 1 (FIGHT)      |
| CAST      | 1       | *(no button — listener active)*                    | —              |
| BRANCHING | 0       | one per choice                                     | 0, 1, 2, 3     |

Always: [Walk away] → action 0xFF (DECLINE)

**Header text:** `beat.textGreeting` → else `GetVariantText(emotion, tags, "chase")` → else empty

### DQ_CourierScript rewrites

**OnGossipHello:**
```cpp
bool OnGossipHello(Player* player, Creature* creature) override {
    uint32 archetypeId = DecodeArchetypeId(sDQMgr->GetActiveTemplateId(player));
    uint8  currentBeat = sDQMgr->GetCurrentBeat(player);
    const ArchetypeDef*  def  = sArchetypeMgr->Get(archetypeId);
    const ArchetypeBeat* beat = sArchetypeMgr->GetBeat(archetypeId, currentBeat);
    if (!def || !beat) return false;
    sDQDialogue->OpenBeatGossip(player, creature, *def, *beat);
    return true;
}
```

**OnGossipSelect:**
```cpp
bool OnGossipSelect(Player* player, Creature*, uint32, uint32 action) override {
    CloseGossipMenuFor(player);
    if (action == 0xFF) { sDQMgr->OnInteractionDeclined(player); return true; }
    sDQMgr->OnInteractionAccepted(player, sDQMgr->GetActiveTemplateId(player), action);
    return true;
}
```

**DynamicQuestMgr addition:**
```cpp
uint8 GetCurrentBeat(Player* player) const;  // returns ps.activeInst.currentPhase
```

---

## Phase 7 — Action Router

**Modifies:** `src/mechanics/MechanicArchetype.h`, `src/mechanics/MechanicArchetype.cpp`

New private methods:
```cpp
void BeginCourierObjective(Player*, InteractionInstance&, const ArchetypeBeat*);
void BeginKillObjective(Player*, InteractionInstance&, const ArchetypeBeat*);
void BeginGotoObjective(Player*, InteractionInstance&, const ArchetypeBeat*);
void BeginFight(Player*, InteractionInstance&, const ArchetypeBeat*);
void RegisterPassiveCast(Player*, InteractionInstance&, const ArchetypeBeat*);
```

### Revised OnChoice dispatch
```cpp
switch (beat->mechanic) {
    case DQ_BEAT_WITNESS:   CompleteBeat(player, inst);                   break;
    case DQ_BEAT_COURIER:   BeginCourierObjective(player, inst, beat);    break;
    case DQ_BEAT_KILL:      BeginKillObjective(player, inst, beat);       break;
    case DQ_BEAT_GOTO:      BeginGotoObjective(player, inst, beat);       break;
    case DQ_BEAT_ACTIVATE:  /* props already scattered in OnStart */      break;
    case DQ_BEAT_FIGHT:     BeginFight(player, inst, beat);               break;
    case DQ_BEAT_CAST:      RegisterPassiveCast(player, inst, beat);      break;
}
```

Encounter_count and timer transition logic wraps ABOVE the mechanic switch (unchanged from current).

---

## Phase 8 — Fight Handler

**Modifies:** `src/mechanics/MechanicArchetype.h`, `src/mechanics/MechanicArchetype.cpp`

### New methods
```cpp
public:
    void OnFightConcede(Player* player, InteractionInstance& inst);
private:
    void BeginFight(Player* player, InteractionInstance& inst, const ArchetypeBeat* beat);
```

### BeginFight
```cpp
DQNPCBuilder::FlipToHostile(courier, player);
sDQAnimation->PlayBeatTransition(courier, player);
inst.concedePct = beat->fightThreshold;
```

### OnFightConcede
```cpp
DQNPCBuilder::FlipToFriendly(courier);
// Say concede line from dq_text_variant_pool (emotion + "concede")
courier->Say(sDQDialogue->GetVariantText(beat->emotion, "", "concede"), LANG_UNIVERSAL);
CompleteBeat(player, inst);   // success path
```

### Hostile-on-fail (timer expiry)
```cpp
// In OnUpdate when phaseTimer expires:
if (beat->choiceFailTransition != 0) {
    inst.choiceIndex = DQ_CHOICE_FAIL_SENTINEL;  // 0xFE
    CompleteBeat(player, inst);
} else {
    CompleteBeat(player, inst);
}
```

### DynamicQuestMgr::OnCourierConceded
```cpp
void DynamicQuestMgr::OnCourierConceded(Player* player) {
    IMechanicModule* m = GetMechanic(ps->activeInst.templateId);
    if (auto* arch = dynamic_cast<MechanicArchetype*>(m))
        arch->OnFightConcede(player, ps->activeInst);
}
```

---

## Phase 9 — Resolution Consolidation

**Modifies:** `src/mechanics/MechanicArchetype.cpp`

### Sentinel
```cpp
static constexpr uint32 DQ_CHOICE_FAIL_SENTINEL = 0xFE;
```

### AdvanceBeat — choice-aware routing
```cpp
bool isFail = (inst.choiceIndex == DQ_CHOICE_FAIL_SENTINEL);

uint8 nextBeat;
if (isFail && beat->choiceFailTransition != 0)
    nextBeat = beat->choiceFailTransition;
else if (!isFail && beat->choiceSuccessTransition != 0)
    nextBeat = beat->choiceSuccessTransition;
else
    nextBeat = /* existing sequential / branching logic */;

bool completed = (nextBeat == 0xFF || nextBeat > def->totalBeats);

// Exit animation fires before despawn
if (courier && beat)
    sDQAnimation->PlayExit(courier, beat->exitAnimation, beat->exitSpell);

SaveBeatState(...);
sDQMgr->OnArchetypeBeatAdvanced(player, archetypeId, completed ? 0xFF : nextBeat);
```

### OnCleanup — consolidated
```cpp
if (courier && inst.auraSpell)
    sDQAnimation->RemovePersistentAura(courier, inst.auraSpell);
if (courier)
    courier->DespawnOrUnsummon(2500ms);
for (ObjectGuid guid : inst.auxGuidList)
    if (GameObject* go = ...) go->Delete();
inst.auxGuidList.clear();
sDQMgr->UnregisterActiveCourier(inst.courierGuid);
```

---

## Legacy Deletion

### Files deleted
- `src/mechanics/MechanicHungryChild.h/.cpp`
- `src/mechanics/MechanicSuccubus.h/.cpp`
- `src/scripts/DQ_HungryChildAI.cpp`
- `src/scripts/DQ_SuccubusAI.h/.cpp`
- `src/core/InteractionTemplateLibrary.h/.cpp`

### Code removed from existing files
- `DynamicQuestMgr.cpp/h`: `RegisterMechanics` body (keep only MechanicArchetype), Ileana episode gating, `ileanaEpisode` / `soulDebtUntil` / `lastEpisodeId` fields, `GetIleanaEpisode()` / `SetSoulDebt()`
- `DQ_CourierAI.cpp`: `SetupVariants`, legacy gossip menu (`OpenCourierGossipMenu`)
- `loader.cpp`: `AddSC_DQ_HungryChildAI`, `AddSC_DQ_SuccubusAI`

---

## File Changes Summary

| Action | File |
|--------|------|
| CREATE | `src/core/DQNPCBuilder.h/.cpp` |
| CREATE | `src/core/DQAnimationMgr.h/.cpp` |
| CREATE | `src/core/DQDialogueMgr.h/.cpp` |
| CREATE | `src/scripts/DQ_PassiveHooks.cpp` |
| MODIFY | `src/core/DQSpawnSystem.h/.cpp` — `int8 levelOffset` in DQSpawnDesc + ApplyScaling |
| MODIFY | `src/core/DynamicQuestMgr.h/.cpp` — reverse courier map, Phase 5 callbacks, legacy removal |
| MODIFY | `src/mechanics/IMechanicModule.h` — add `concedePct` + `auraSpell` to InteractionInstance |
| MODIFY | `src/mechanics/MechanicArchetype.h/.cpp` — phases 3/7/8/9 |
| MODIFY | `src/scripts/DQ_CourierAI.cpp` — Phase 6 gossip rewrite |
| MODIFY | `src/loader.cpp` — register DQ_PassiveHooks, remove legacy |
| DELETE | `src/mechanics/MechanicHungryChild.*` |
| DELETE | `src/mechanics/MechanicSuccubus.*` |
| DELETE | `src/scripts/DQ_HungryChildAI.cpp` |
| DELETE | `src/scripts/DQ_SuccubusAI.*` |
| DELETE | `src/core/InteractionTemplateLibrary.*` |

---

## Implementation Order

1. `IMechanicModule.h` — add 2 fields (everything reads this)
2. `DQSpawnSystem.h/.cpp` — levelOffset extension
3. `DQNPCBuilder.h/.cpp` — pure new code
4. `DQAnimationMgr.h/.cpp` — pure new code
5. `DQDialogueMgr.h/.cpp` — pure new code
6. `DynamicQuestMgr.h/.cpp` — reverse map + callbacks + legacy removal
7. `DQ_PassiveHooks.cpp` — registers hooks that call sDQMgr
8. `MechanicArchetype.h/.cpp` — wire all new systems
9. `DQ_CourierAI.cpp` — gossip rewrite
10. `loader.cpp` — register / deregister
11. Delete legacy files
12. `modules.vcxproj` — add new .cpp, remove deleted .cpp

---

## Open Questions (deferred to Phase 10–11)

- `DQ_BEAT_GOTO` needs `OnPlayerZoneChange` wiring for zone-based completion
- `BeginKillObjective` kill_entry column not yet in schema; use `objectiveEntry = 0` (any kill) until Phase 11 authoring pass adds it
- `TryTrigger` still routes through `InteractionTemplateLibrary` — Phase 10 replaces with `ArchetypeMgr::GetEligible`. Until then archetypes trigger via `.dq trigger <id>`
