# Code drift Jan03 vs release (RussianGold PDB publics vs objects of a fresh port build C:\a5b\s2, 374 objs)
Qualified plain functions/ctors/dtors, STL excluded, NDb:: stripped.
- release 13775, Jan03 13150, common 9387 (68% of release). release-only 4388 (32%), Jan03-only 3763.
- caveat: release publics lose functions removed by /OPT:REF or fully inlined -> "Jan03-only" overstated, "release-only" is a lower bound.
- scopes: release 1903, Jan03 2659, common 1420, release-only 483 (2649 funcs), Jan03-only 1239.
- release-only by subsystem (incl. new methods in old classes): NAI 1076, NWorld 691, NUI 568, NGame 433, NRPG 238, NGScene 192, NScript 114.
  NAI: CAIFireArmsWeaponBase, SAIUnitState, CAICombatLogic, CAIEventTrackerImpl, CStabilityTrackers, loot/melee/leave-PK actions.
  Jan03-only NAI: CAITacticalCommander, CAIState, CTask*, CAITaskCommander, CAISignal*, CAIPlayer -> AI architecture reworked.
  NGame: CMissionBase 87 new, multiplayer/autoplay/custom game/medals/hints/face-gen menus. NUI: video player, hints, medals, profiles.
  NGScene: CBinkVideoPlayer, rain, ambient animators, terrain texture blend, HW cursor. NScript: ~100 new lua* functions (luaFadeIn, luaCameraSequence, luaKillEmAll, ...).
- Side note: build dir C:\a5b\s lost Main/Game objects after switching CMake 4.4.5 <-> 3.31 (different object paths; ninja cleandead on regeneration); full rebuild in C:\a5b\s2 (411/411).
Files: C:\a5b\recon\gaps_d\drift.py, out\drift.txt, out\release_only_functions.txt, out\jan03_only_functions.txt
