/*
 * DynamicQuests+ Module
 * System 4: Interaction Template Library
 *
 * Content catalog loaded from DB at startup.
 * Authors add new interactions by inserting DB rows — no C++ needed.
 */

#ifndef DQ_INTERACTION_TEMPLATE_LIBRARY_H
#define DQ_INTERACTION_TEMPLATE_LIBRARY_H

#include "Define.h"
#include <string>
#include <vector>
#include <unordered_map>

enum DQZoneFaction : uint8
{
    DQ_FACTION_ANY      = 0, // fires in any zone (default)
    DQ_FACTION_ALLIANCE = 1, // only fires in Alliance capitals
    DQ_FACTION_HORDE    = 2, // only fires in Horde capitals
    DQ_FACTION_NEUTRAL  = 3, // only fires in neutral capitals (Dalaran, Shattrath)
};

enum DQZoneType : uint8
{
    DQ_ZONE_ANY              = 0,
    DQ_ZONE_OUTDOOR          = 1,
    DQ_ZONE_CAPITAL          = 2,
    DQ_ZONE_WILDERNESS       = 3,
    DQ_ZONE_DUNGEON_ENTRANCE = 4,
};

enum DQMechanicType : uint8
{
    DQ_MECHANIC_SUCCUBUS         = 7,
    DQ_MECHANIC_HUNGRY_CHILD     = 8,
    DQ_MECHANIC_UNKNOWN          = 0xFF,
};

enum DQOutcomeType : uint8
{
    DQ_OUTCOME_REWARD        = 0,
    DQ_OUTCOME_DENY          = 1,
    DQ_OUTCOME_COMBAT        = 2,
    DQ_OUTCOME_FETCH         = 3,
    DQ_OUTCOME_FAIL          = 4,
    DQ_OUTCOME_PHASE_ADVANCE = 5,
};

struct InteractionChoice
{
    uint32        id            = 0;
    uint32        templateId    = 0;
    uint8         displayOrder  = 0;
    std::string   choiceText;
    uint32        prereqItemId  = 0;
    DQOutcomeType outcomeType   = DQ_OUTCOME_DENY;
    std::string   outcomeData;
    uint32        emoteOnSelect = 0;
};

struct InteractionTemplate
{
    uint32      id             = 0;
    std::string name;
    std::string theme;
    std::string npcTagsRequired;
    std::string npcTagsExcluded;
    DQZoneType  zoneType       = DQ_ZONE_ANY;
    uint8       levelMin       = 1;
    uint8       levelMax       = 80;
    uint32      prereqGold     = 0;
    uint32      prereqItemId   = 0;
    uint8       contextMask    = 0xFF;
    uint8       tier           = 1;
    DQMechanicType mechanicType = DQ_MECHANIC_UNKNOWN;
    int32       rarityWeight   = 10;

    int16         combatHpPct     = 0;
    int16         combatDmgPct    = 0;
    uint32        gossipNpcTextId = 0;
    float         combatHpRatio   = 0.f;
    float         combatDmgRatio  = 0.f;
    DQZoneFaction zoneFaction     = DQ_FACTION_ANY;

    std::vector<InteractionChoice>  choices;
    std::vector<std::string>        requiredTagList;
    std::vector<std::string>        excludedTagList;
    std::vector<std::string>        greetingVariants;
    std::vector<std::string>        chaseVariants;
};

class InteractionTemplateLibrary
{
public:
    static InteractionTemplateLibrary* instance();

    void LoadFromDB();
    const std::vector<InteractionTemplate>& GetAll() const { return _templates; }
    const InteractionTemplate* GetById(uint32 id) const;
    bool IsLoaded() const { return _loaded; }
    size_t GetCount() const { return _templates.size(); }

private:
    InteractionTemplateLibrary() = default;
    bool _loaded = false;
    std::vector<InteractionTemplate> _templates;
    std::unordered_map<uint32, size_t> _idIndex;

    static DQZoneType      ParseZoneType(const std::string& s);
    static DQZoneFaction   ParseZoneFaction(const std::string& s);
    static DQMechanicType  ParseMechanicType(const std::string& s);
    static DQOutcomeType   ParseOutcomeType(const std::string& s);
    static uint8           ParseContextMask(const std::string& contextTags);
    static std::vector<std::string> SplitTags(const std::string& csv);
};

#define sInteractionLib InteractionTemplateLibrary::instance()

#endif // DQ_INTERACTION_TEMPLATE_LIBRARY_H
