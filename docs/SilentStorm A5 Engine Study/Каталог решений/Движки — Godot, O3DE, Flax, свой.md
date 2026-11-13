---
tags: [silent-storm, каталог, справочник, движок]
studied: 2026-11-12
---

> [!note] Справочный файл
> Собранные 12.11.2026 сведения с источниками: что движок даёт готовым для каждого требования каталога и что придётся писать. Вывод — в [23 Выбор движка](<../23 Выбор движка.md>).
> Пометки: **[стр.]** — прочитано на странице, **[выдача]** — только фрагмент поиска, **[память]** — не проверялось.

# Оценка движков для ремейка тактики 2003 года с разрушаемыми многоэтажными зданиями

**Вывод.** Godot 4.7 экономит больше всего суммарно (редактор, UI, анимация, пайплайн, кадры), но слаб в динамическом GI. Flax 1.13 лучше всех закрывает свет и C++-запросы, но у него крошечное сообщество. O3DE даёт наименьшую экономию. Требования 3, 4, 5, 8, 9 и солвер пишутся самим на любом движке, поэтому ядро симуляции стоит делать независимой C++-библиотекой.

Метки: [стр.] — прочитано на странице, [выдача] — только сниппет поиска, [память] — сейчас не проверялось. Оценки трудоёмкости и рекомендации — мои выводы, а не факты из источников.

## Актуальные версии на 12.11.2026

- **Godot:** стабильная ветка 4.7, последний патч 4.7.2 от 18.08.2026; 4.8 в стадии beta 3 (09.11.2026), RC и stable нет [стр.] https://godotengine.org/blog/
- **O3DE:** 26.05.0 — последний релиз в документации; 26.10 там нет [стр.] https://docs.o3de.org/docs/release-notes/ . План на 2026 был 26.5.0 и 26.10.0 [стр.] https://o3de.org/o3de-2025-in-review-and-a-look-into-2026/
- **Flax:** 1.13 от 21.10.2026 [стр.] https://flaxengine.com/blog/flax-1-13-released/ ; 1.13.1 примерно неделю назад [стр.] https://flaxengine.com/blog/

## (A) Godot 4.7 / 4.8

### 1. Здания

**Готово:**
- Автоматический инстансинг MeshInstance3D с одинаковыми мешем и материалом, только в Forward+ и только для непрозрачных и alpha-test материалов [выдача] https://docs.godotengine.org/en/stable/tutorials/performance/optimizing_3d_performance.html
- MultiMesh с per-instance custom data (читается в шейдере как INSTANCE_CUSTOM) [выдача] https://docs.godotengine.org/en/4.4/classes/class_multimesh.html
- ArrayMesh/SurfaceTool и прямые вызовы RenderingServer без узлов [память].

**Писать:** чанковый менеджер и свой солвер.
- У MultiMesh есть только глобальный `visible_instance_count`; скрывать отдельный инстанс придётся нулевым масштабом или через шейдер [выдача, тот же URL; приём — память].
- При тысячах блоков × 22 куска (порядка 10^5 кусков) отдельные инстансы упрутся в CPU-подготовку кадра. Дешевле всего чанки (например, 4×4 блока на этаж) со слитым ArrayMesh, пересобираемым при изменении, плюс коллизия на чанк (моя оценка).

**Подводные камни:**
- В Jolt смена формы у тела в дереве вызывает полную пересборку compound-формы; сообщалось до 8 мс на обновление 64 форм [выдача] https://github.com/godot-jolt/godot-jolt/issues/702
- `face_index` у лучей по умолчанию −1; включение стоит +25% памяти ConcavePolygonShape3D [стр.] https://docs.godotengine.org/en/stable/tutorials/physics/using_jolt_physics.html
- Occlusion culling — CPU-растеризация через Embree. Перемещение окклюдера требует пересборки BVH, переключение видимости дешевле; ArrayOccluder3D можно создавать из скрипта; MultiMesh в запекание не попадает [стр.] https://docs.godotengine.org/en/stable/tutorials/3d/occlusion_culling.html
- Для тактической камеры со срезом этажей occlusion culling почти не нужен (моя оценка).
- Опубликованных замеров пересборки trimesh в Jolt не нашёл.

### 2. Свет

**Готово:**
- Forward+: лимит 512 кластерных элементов в кадре (omni, spot, декали, пробы), настраивается [стр.] https://docs.godotengine.org/en/stable/tutorials/3d/lights_and_shadows.html
- Атлас теней по умолчанию вмещает 88 теневых omni/spot в кадре (4+4+16+64). Тень перерисовывается только если изменились влияющие объекты [стр., тот же URL]. Для пошаговой игры это выгодно.
- 4.7: AreaLight3D [стр.] https://godotengine.org/releases/4.7/
- 4.8: screen-space contact shadows, multi-bounce AO [стр.] https://godotengine.org/article/dev-snapshot-godot-4-8-beta-1/

**GI — слабое место:**
- VoxelGI требует запекания; SDFGI «полу-реалтайм», подходит для геометрии, создаваемой в игре, но динамические объекты в GI не вносят вклад, есть утечки света [стр.] https://docs.godotengine.org/en/stable/tutorials/3d/global_illumination/introduction_to_global_illumination.html
- HDDAGI: PR #119869 открыт с 29.05.2026, не слит, веха «4.x». Отражения не начаты, поддержка динамических объектов отложена на потом [стр.] https://github.com/godotengine/godot/pull/119869 . В анонсах 4.7 и 4.8 beta 1 его нет [стр., URL выше].
- Реалистично: прямой свет с тенями, SSIL/SSAO и ambient либо ReflectionProbe (моя оценка).

### 3. Запросы

**Готово мало:**
- `intersect_ray` возвращает только первое попадание; есть `hit_from_inside` и `exclude` [выдача] https://docs.godotengine.org/en/stable/tutorials/physics/ray-casting.html . Предложения «все попадания» в поиске не нашёл.
- 4.8 добавляет неаллоцирующие типизированные результаты (`PhysicsIntersectRayResult3D`) и Jolt 5.6.0 [стр., 4.8 beta 1].
- Физика по умолчанию не потокобезопасна; нужен «Run on Separate Thread» [стр.] https://docs.godotengine.org/en/stable/tutorials/performance/thread_safe_apis.html . Jolt-модуль потокобезопасен, но это «экспериментально» [стр., using_jolt_physics].

**Писать:**
- Свою BVH или воксельную сетку по кускам с материалом — она всё равно нужна солверу. Тысячи лучей на выстрел с входом/выходом через API Godot — тупик (моя оценка).
- Сам Jolt умеет back-face режимы и перегрузку с коллектором [выдача] https://jrouwe.github.io/JoltPhysics/class_ray_cast_settings.html . Но это доступно только из модуля движка, не из GDExtension [память/оценка].
- CPU-вокселизации в движке нет [память].

### 4. Навигация

- AStar3D: точки с весом, односторонние связи, отключение точек, переопределяемые `_compute_cost` и `_filter_neighbor`. Стоимости на ребро нативно нет, `get_point_path` не потокобезопасен [стр.] https://docs.godotengine.org/en/stable/classes/class_astar3d.html
- NavigationServer3D — навмеш с чанковым перезапеканием; парсинг геометрии идёт в главном потоке, есть жалобы на швы между чанками [выдача] https://docs.godotengine.org/en/4.6/tutorials/navigation/navigation_using_navigationmeshes.html
- AStarGrid2D только 2D [память].
- Вывод: писать своё на C++ (Дейкстра по состояниям тайл×этаж×поза с ценой в ОД). AStar3D годится максимум для прототипа.

### 5. Правила

- OctoD godot-gameplay-systems / godot-gameplay-abilities: GDExtension-версия 1.0.0rc1 под 4.4, последний push в августе 2025 [выдача] https://store.godotengine.org/asset/octod/godot-gameplay-abilities/
- GodotGAS: GDScript, 4.6+ [выдача] https://store.godotengine.org/asset/indiegamedad/godotgas/
- Оба рассчитаны на реальное время и находятся в статусе rc/малых проектов. Пошаговые модификаторы с длительностью в ходах проще написать самим (моя оценка).
- Шаблон тактики есть только демо-уровня: https://github.com/ramaureirac/godot-tactical-rpg [выдача].

### 6. Редактор

**Готово:** EditorPlugin, EditorNode3DGizmoPlugin, @tool [память]; в 4.7 появился редактор MeshLibrary [стр., releases/4.7]; в 4.6 — рисование GridMap линией Брезенхэма [стр.] https://godotengine.org/releases/4.6/

**GridMap как представление блоков — нет:**
- Ячейка хранит только индекс предмета и одну из 24 ориентаций, без метаданных.
- GridMap не наследует VisualInstance3D, поэтому слои и cull-маски к нему не применимы [стр.] https://docs.godotengine.org/en/stable/classes/class_gridmap.html
- Он не даст ни поштучного скрытия кусков, ни среза этажей.

**Писать:** свой узел «Building» с ресурсом данных и плагином (стены по рёбрам сетки, заливка плит, комнаты). Это самая крупная экономия Godot: оболочка редактора, undo/redo и инспектор уже есть.

### 7. Шаблоны карт

- **Готово:** инстансинг и наследование сцен, сидируемый RandomNumberGenerator [память].
- **Писать:** взвешенные варианты, наследуемые флаги, штамповку.
- Terrain3D: 1.0.2 в библиотеке ассетов (19.05.2026) [выдача] https://godotengine.org/asset-library/asset/3892 ; 1.1.0 якобы вышла 30.10.2026 (слабый источник) [выдача] https://gamineai.com/blog/what-godot-terrain3d-1-1-0-changed-for-indies-2026
- Режим динамической коллизии делает возможным изменение рельефа в рантайме [выдача] https://80.lv/articles/godot-engine-s-terrain3d-received-dynamic-collision-mode
- API штамповки высот и статус HTerrain не проверены.

### 8. Данные

- **Готово:** типизированные Resource со ссылками, `load_resource_pack` для PCK [память]; delta-патчи PCK в 4.6 [стр., releases/4.6].
- **Писать:** PCK заменяет файлы целиком, оверлей на уровне полей придётся делать самим.
- Для 150 таблиц разумнее свой табличный формат с кодогенерацией (моя оценка).
- Аддон godot-sqlite не проверял [память].

### 9. Скрипты и сейвы

- `await` в GDScript не сериализуется [память]. В 4.7 добавлен `Tween.tween_await()` [стр., releases/4.7], но сохранению это не помогает.
- lua-gdextension: 0.8.2 от 12.07.2026 под Godot 4.5, сборки Lua 5.4 и LuaJIT [выдача] https://www.godotengine.org/asset-library/asset/2330 . 484 звезды, корутины есть; о сериализации состояния ничего, ожидание сигналов числится в TODO [стр.] https://github.com/gilzoide/lua-gdextension
- Eris: Lua 5.2/5.3, сохраняет приостановленные корутины, не работает с LuaJIT, репозиторий архивирован 08.08.2026 [стр.] https://github.com/fnuecke/eris
- Вывод: встраивать свой Lua 5.3 + Eris в C++-ядро или делать скрипты миссий как командные списки с явным счётчиком команд. От движка это не зависит.
- Системы сохранений в Godot нет [память].

### 10. Презентация

Здесь основная экономия.

- AnimationTree, method-треки, AnimationPlayer для катсцен, UI, аудиошины, локализация, слои видимости и cull mask [память].
- IK: в 4.6 новый фреймворк на SkeletonModifier3D (TwoBoneIK3D, FABRIK3D, CCDIK3D, JacobianIK3D, SplineIK3D); документация «в процессе» [стр., releases/4.6].
- 4.8: Trail3D, анимированные декали [стр., 4.8 beta 1].
- Декали — проекционные, входят в лимит 512 [стр., lights_and_shadows]. При удалении куска декаль спроецируется на то, что за ним, поэтому нужна привязка декалей к кускам (моя оценка).
- Липсинк: аддон Baked Lipsync рассчитан на 4.3, есть жалобы на 4.4–4.6 [выдача] https://forum.godotengine.org/t/problems-with-baked-lipsync/128181 . Скорее всего, запекать visemes в Blender.
- PhysicalBoneSimulator3D с Jolt не проверял [память].

### 11. Пайплайн

- **Готово:** glTF, EditorScenePostImport, дочерние объекты как узлы [память].
- **Писать:** расчёт объёма и смежности кусков при импорте (малое).

### 12. Стоимость

- **C++:** модуль компилируется статически и требует пересборки движка; GDExtension — нет, но доступ к движку менее глубокий [выдача] https://docs.godotengine.org/en/4.4/tutorials/scripting/gdextension/what_is_gdextension.html . В 4.6 появилась поддержка Tracy/Perfetto для GDExtension [стр., releases/4.6]. Свежих отчётов крупных проектов о hot reload не нашёл.
- **Консоли:** официальных портов нет, только сторонние поставщики [стр.] https://godotengine.org/consoles/
- **W4 Consoles:** Switch, Switch 2 (бета), PS5, Xbox Series; поддерживаются Godot 4.4–4.6, то есть отставание от 4.7/4.8. Цена $800–10 000 в год; C# в бете; GDExtension на странице не упомянут [стр.] https://www.w4games.com/w4consoles
- **Сообщество:** 120k звёзд на GitHub [стр.] https://github.com/godotengine/godot
- **ИИ-ассистенты:** строгих исследований нет, только вендорские блоги и малый бенчмарк; типичная проблема — скатывание в API Godot 3 [выдача] https://www.summerengine.com/blog/best-llm-for-godot
- **Выпущенные игры:** 3D-тактику уровня XCOM с разрушением на Godot не нашёл [выдача]; команды и размеры не установлены.
- **Производительность:** пост «Optimizing CPU-side Rendering Code» (15.09.2026) виден в списке блога [стр.], но сама страница не открылась.

## (B) O3DE 26.05

- **1. Здания:** инстансинг мешей в Atom и White Box [память, не проверено]. Чанки и перестройку PhysX-мешей писать.
- **2. Свет:** Diffuse Probe Grid в реальном времени требует аппаратной трассировки (DXR/Vulkan RT), иначе запечённый режим [стр.] https://docs.o3de.org/docs/user-guide/components/reference/atom/diffuse-probe-grid/ . В 26.05 сетка учитывает отскоки от ламп и эмиссива [стр.] https://docs.o3de.org/docs/release-notes/2605-0-release-notes/ . Лимиты теневых источников не проверены.
- **3. Запросы:** `m_reportMultipleHits` возвращает все попадания до `m_maxResults` [выдача] https://docs.o3de.org/docs/user-guide/interactivity/physics/nvidia-physx/scene-queries/ . PhysX 5 по умолчанию, PhysX 4 удаляется [стр., release notes]. Точки выхода и материалы по толщине всё равно свои.
- **4. Навигация:** гем Recast с явным `UpdateNavigationMeshAsync` [выдача] https://docs.o3de.org/docs/user-guide/interactivity/navigation-and-pathfinding/recast-navigation/ . Тайловую модель писать.
- **5, 7, 8, 9:** писать самим. Lua и Script Canvas есть [стр., release notes упоминают оба], но сериализуемых корутин нет [память].
- **6. Редактор:** C++-компоненты, новый мастер «New Component» [стр., release notes], Python-автоматизация [память]. Порог входа выше, чем у Godot.
- **10. Презентация:** Open Particle System экспериментален, «не для продакшена» [стр., release notes]. Анимация (EMotionFX) [память].
- **12. Стоимость:**
  - 9.9k звёзд, 3.4k открытых issues [стр.] https://github.com/o3de/o3de
  - Два релиза в год; загрузки +87.82% за 2025; фокус «игры, робототехника, кино» [стр., обзор 2025].
  - Выпущенные игры: State of Matter (07.2024), остальные TBD [выдача] https://docs.o3de.org/docs/learning-guide/made-with-o3de/
  - Документация неровная, порог входа высокий (источники в основном 2022 года) [выдача] https://hackaday.com/2022/01/10/open-3d-engine-amazons-old-clothes-or-a-game-engine-to-truly-get-excited-about
  - Консоли: публичной поддержки не нашёл.

## (C) Flax 1.13

- **1. Здания:** инстансинг; Foliage с удалением инстанса по индексу и `RebuildClusters` [выдача] https://docs.flaxengine.com/api-cpp/FoliageInstance.html . В 1.13 — HZB и аппаратное occlusion culling без запекания [стр., 1.13]. Чанки писать.
- **2. Свет — лучший из трёх:**
  - DDGI + Global SDF + Global Surface Atlas, программная трассировка без RTX. Динамические объекты перерастеризуются чаще.
  - Документация прямо советует делить стены зданий на отдельные части, что совпадает с модульными блоками.
  - Ограничения: точность атласа 10–20 см, нужны SDF у моделей, 200–300 МБ VRAM [стр.] https://docs.flaxengine.com/manual/graphics/lighting/gi/realtime.html
  - 1.13: PCSS-тени, DDGI-отражения [стр., 1.13].
  - Поведение при массовом удалении кусков не проверено. SDF кусков придётся генерировать на 22 части каждого блока.
- **3. Запросы:** `Physics.RayCastAll`, в попадании есть FaceIndex и UV [стр.] https://docs.flaxengine.com/manual/physics/raycasting.html . Выход и толщина — свои.
- **4. Навигация:** Recast с частичной перестройкой по области в рантайме [выдача] https://git.flaxengine.com/Flax/FlaxEngine/src/branch/1.13/Source/Engine/Navigation/NavMeshBoundsVolume.cpp . Тайловую модель писать.
- **6. Редактор:** плагины, свои окна, гизмо на C# [выдача] https://docs.flaxengine.com/manual/editor/advanced/extending-editor.html . C++ — первоклассный, с hot reload [выдача] https://docs.flaxengine.com/manual/scripting/cpp/index.html
- **5, 7, 8, 9:** писать самим. В движке есть Behavior Trees [стр., 1.13]; готовых GAS-подобных решений не нашёл.
- **12. Стоимость:**
  - Консоли в списке платформ: PS4, PS5, Xbox One/Scarlett, Switch; Switch 2 нет [стр.] https://docs.flaxengine.com/manual/platforms/index.html
  - 7.1k звёзд, 649 открытых issues [стр.] https://github.com/FlaxEngine/FlaxEngine ; Discord около 3.6k участников [выдача] https://discord.com/servers/flax-437989205315158016
  - Каденция: 1.11 → 1.12 → 1.13 примерно раз в полгода, 999 коммитов и 130 PR на релиз; Flax 2.0 обещан на конец 2027 [стр., 1.13 и блог].
  - Выпущенные игры: Homeostasis [стр., 1.13], Explorers of Esmar в раннем доступе [выдача]. Размеры команд не установлены.
  - Риск: ядро держится на одном основном авторе [память].

## (D) Свой движок — базовая линия

- **Есть библиотеки:** SDL 3.4 с GPU API (01.2026) [выдача] https://discourse.libsdl.org/t/announcing-sdl-3-4-0/65438 ; Jolt 5.6.0 [стр., 4.8 beta 1] с лучами через коллектор и back-face режимами [выдача]; Dear ImGui, Recast, ozz-animation, cgltf, miniaudio [память].
- **Писать всё остальное:** кластерный рендер с атласом теней, GI, материалы, скелетную анимацию с блендингом/IK/ragdoll, частицы, декали, игровой UI, локализацию, редактор уровней с undo, импорт ассетов, консольные порты.
- Это на годы больше работы, чем в A–C. Выигрыш — только полный контроль над пунктами 1–4.
- Статус bgfx не проверял.

## (E) Другие

- **Unreal Engine 5:**
  - Из коробки: Chaos/Geometry Collections, Lumen, multi-line-trace со всеми попаданиями, GAS [память].
  - Выпущенная пошаговая тактика: Star Wars Zero Company (Bit Reactor, UE5, вышла 27.08.2026) [выдача] https://www.unrealengine.com/developer-interviews/star-wars-zero-company-bit-reactor-crafts-a-galaxy-of-tactical-possibilities-with-ue5
  - Закрывает пункты 2, 3, 5, 10 лучше любого из A–C.
  - Но Chaos — это физическое разрушение, а не детерминированный «свой солвер по кускам».
- **Unity:** Phoenix Point, Xenonauts 2, Menace — тактики с разрушением [память, поиск не подтвердил].
- Stride, Unigine, CryEngine и открытые тактические движки не исследовал.

## Сводная таблица

Обозначения: Г — готово, Ч — частично, П — писать самим; м/с/б — малое/среднее/большое усилие. Все оценки мои.

| Требование | Godot | O3DE | Flax | Свой |
|---|---|---|---|---|
| 1. Здания | Ч, б | Ч, б | Ч, б | П, б |
| 2. Свет | Ч (GI слабый), с | Ч (GI требует RT), с–б | Г/Ч, м–с | П, б |
| 3. Запросы | П, с | Ч, с | Ч, с | П, с |
| 4. Навигация | П, с | П, с | П, с | П, с |
| 5. Правила | П, с | П, с | П, с | П, с |
| 6. Редактор | Ч, с | Ч, б | Ч, с | П, б |
| 7. Шаблоны и террейн | Ч, с | Ч, с–б | Ч, с | П, б |
| 8. Данные | Ч, с | П, с | П, с | П, с |
| 9. Скрипты и сейвы | П, с | Ч (Lua есть), с | П, с | П, с |
| 10. Презентация | Г/Ч, м | Ч, с | Г/Ч, м–с | П, б |
| 11. Пайплайн | Г/Ч, м | Ч, с | Ч, м | П, с |
| 12. Стоимость | низкая | высокая | средняя | наивысшая |

## Три главные экономии и три главных риска

**Godot**
- Экономии: расширяемый редактор; презентация (анимация, IK, UI, аудио, локализация); кадры, сообщество и аддоны.
- Риски: динамический GI (HDDAGI не слит); физические запросы и глубокий доступ к Jolt только через модуль; консоли через W4 с отставанием версий и без готовых примеров 3D-тактики с разрушением.

**O3DE**
- Экономии: multi-hit запросы PhysX; встроенный Lua; C++-архитектура гемов.
- Риски: GI требует аппаратной трассировки; почти нет выпущенных игр и неровная документация; тяжёлая сборка и порог входа.

**Flax**
- Экономии: динамический GI без запекания и occlusion culling без запекания; первоклассный C++ с hot reload и `RayCastAll`; консольные платформы в самом движке.
- Риски: малое сообщество и зависимость от одного автора; мало аддонов и разработчиков на рынке; ломающие изменения на пути к 2.0.

**Свой движок**
- Экономии: полный контроль над пунктами 1–4; нет ограничений чужого API; детерминизм.
- Риски: рендер и GI; редактор; анимация, UI и консоли.

## Что не удалось проверить

- **Godot, производительность:** замеры пересборки ConcavePolygonShape3D в Jolt; потолок числа инстансов в Forward+; содержание поста об оптимизации CPU-рендера (404).
- **Godot, запросы:** потокобезопасность именно `PhysicsDirectSpaceState3D`; существование предложения «все попадания луча».
- **Godot, прочее:** API штамповки Terrain3D и статус HTerrain; godot-sqlite; PhysicalBoneSimulator3D с Jolt; маска теней для срезанных этажей; поддержка GDExtension на консолях W4.
- **O3DE:** лимиты теневых источников в Atom; консоли; инстансинг и White Box; число контрибьюторов.
- **Flax:** поведение DDGI/SDF при массовом удалении кусков; условия доступа к консолям; размеры команд выпущенных игр.
- **Прочее:** статус bgfx; движки Phoenix Point, Menace, Xenonauts 2; Stride, Unigine, CryEngine; строгие данные о качестве ИИ-ассистентов на GDScript/GDExtension.
- **По всем движкам:** размеры команд выпущенных 3D-тактик.