-- DynamicQuests+ | Exit System migration
-- Adds exit_style column to dq_archetype_beat.
-- Default 'reverse': NPC walks back to spawn point before despawning.
-- Styles: reverse | forward | away | portal_out | instant | fade

ALTER TABLE `dq_archetype_beat`
  ADD COLUMN `exit_style` VARCHAR(32) NOT NULL DEFAULT 'reverse'
  AFTER `exit_animation`;
