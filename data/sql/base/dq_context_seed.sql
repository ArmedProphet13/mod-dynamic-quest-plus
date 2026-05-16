-- DynamicQuests+ System 5: Tag hierarchy defaults and landmark seeds
-- Re-runnable: safe to run multiple times (DELETE + INSERT pattern).

-- -------------------------------------------------------------------------
-- Tag hierarchy
-- Every child_tag automatically inherits its parent_tag when tags are
-- resolved. Authors only need to specify the specific tag; parent tags
-- are added by the engine. Add new relationships here with a single INSERT.
-- -------------------------------------------------------------------------
DELETE FROM `dq_tag_hierarchy`;
INSERT INTO `dq_tag_hierarchy` (`child_tag`, `parent_tag`) VALUES
  -- Settlement hierarchy
  ('city',             'urban'),
  ('town',             'urban'),
  ('village',          'urban'),
  ('hamlet',           'urban'),
  ('urban',            'settled'),
  ('farm',             'settled'),
  ('sanctuary',        'settled'),
  -- Wilderness hierarchy
  ('wilderness',       'unsettled'),
  -- Instance hierarchy
  ('dungeon',          'instance'),
  ('raid',             'instance'),
  ('battleground',     'instance'),
  ('arena',            'instance'),
  -- Eerie/horror hierarchy
  ('graveyard_nearby', 'eerie'),
  ('haunted',          'eerie'),
  ('eerie',            'unsettled'),
  -- Terrain
  ('coastal',          'outdoor');

-- -------------------------------------------------------------------------
-- Landmarks
-- Use .gps in-game to find coordinates for new landmarks.
-- Add rows here; the tag becomes available to archetypes immediately after
-- .dq reload. No C++ changes required.
--
-- Example:
--   INSERT INTO dq_landmarks (map_id, x, y, z, tag, radius, comment)
--   VALUES (0, -9465.0, 67.0, 56.0, 'inn_nearby', 40.0, 'SW Gilded Rose Inn');
-- -------------------------------------------------------------------------
DELETE FROM `dq_landmarks`;
-- (Seed landmarks added here by server author using .gps coordinates)
