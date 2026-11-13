---
tags: [silent-storm, каталог, справочник, движок]
studied: 2026-11-12
---

> [!note] Справочный файл
> Собранные 12.11.2026 сведения с источниками: что движок даёт готовым для каждого требования каталога и что придётся писать. Вывод — в [23 Выбор движка](<../23 Выбор движка.md>).
> Пометки: **[стр.]** — прочитано на странице, **[выдача]** — только фрагмент поиска, **[память]** — не проверялось.

# Unity 6 для ремейка тактической игры 2003 года с разрушаемыми многоэтажными зданиями: что экономит, что писать самим

Главный вывод: Unity почти полностью закрывает презентацию, ассет-пайплайн, редакторный каркас и ray-запросы, но ядро игры (разрушение, солвер, сетка, правила, скрипты с сохранением, шаблоны карт) пишется самостоятельно. Источники помечены: [стр.] — прочитано на странице, [выдача] — только сниппет поиска, [память] — сейчас не проверялось.

## 0. Какая версия актуальна (на 12.11.2026)

- **6.3 LTS** — действующая LTS: выпущена 04.12.2025, поддержка до 12.2027, последний патч 6000.3.28f1 от 03.11.2026. [стр.] https://endoflife.date/unity, https://unity.com/releases/release-overview
- **6.6** — текущий Update-релиз (с 31.08.2026, патч 6000.6.9f1). Ветки 6.4 и 6.5 уже сняты с поддержки, поддержка 6.0 LTS закончилась 16.10.2026. [стр.] https://endoflife.date/unity
- **6.7 LTS** — не выпущена; по трекеру её нет, по вторичным источникам в октябре была в бете, релиз «в конце 2026». Это последний релиз на Mono. [стр.] https://endoflife.date/unity, https://discussions.unity.com/t/coreclr-scripting-and-serialization-update-june-2026/1723299; [выдача] https://www.issoh.co.jp/md/tech/10417
- **Unity 7** — анонсирован 21.07.2026: превью в декабре 2026, полный релиз в Q1 2027, заявлен как «прямое продолжение Unity 6 без ломающих изменений». Включает CoreCLR, .NET 10, C# 14 и частичную перезагрузку кода. [стр.] https://unity.com/blog/unite-seoul-keynote-2026-recap
- **Практически:** стартовать на 6.3 LTS и перейти на 6.7 LTS после выхода — там обещан Surface Cache GI, важный для требования 2.

## 1. Здания: тысячи блоков, до 22 кусков, собственный солвер

**(a) Готовое в движке**

- **Mesh API.** `Mesh.AllocateWritableMeshData` / `Mesh.ApplyAndDisposeWritableMeshData`, `Mesh.AcquireReadOnlyMeshData`, `CombineMeshes` — сборка мешей в Jobs/Burst. [память] https://docs.unity3d.com/ScriptReference/Mesh.MeshData.html
- **Коллайдеры.** `Physics.BakeMesh(EntityId, bool, MeshColliderCookingOptions)` потокобезопасен и ставится в `IJobParallelFor`. Нельзя печь один меш из двух потоков; в плеере мешу нужен Read/Write; опции cooking должны совпадать с опциями MeshCollider. [выдача] https://docs.unity.com/en-us/engine/6000.3/script-reference/unityengine/physics/bakemesh
- **GPU Resident Drawer (URP/HDRP).** Автоматически рисует MeshRenderer-объекты через BatchRendererGroup. Требует Forward+, compute-шейдеры и «BatchRendererGroup Variants = Keep All». Документация рекомендует при нём отключить static batching; нагрузка на GPU слегка растёт. [выдача] https://docs.unity3d.com/6000.3/Documentation/Manual/urp/gpu-resident-drawer.html, https://docs.unity3d.com/Manual/urp/gpu-resident-drawer-performance.html
- **BatchRendererGroup** напрямую — для полного ручного контроля. [выдача] https://docs.unity3d.com/Manual/batch-renderer-group
- **ECS.** Entities, Collections, Mathematics и Entities Graphics с 6.4 — core-пакеты. В 6.6 управляемые (class) компоненты объявлены deprecated, EntityId стал 8-байтным; унификация с GameObject ещё идёт. [стр.] https://discussions.unity.com/t/ecs-development-status-for-unity-6-6/1724480
- **Static batching** для меняющегося содержимого не годится: батч строится при сборке или через `StaticBatchingUtility.Combine`, и удаление куска требует пересборки. [память]

**(b) Asset Store**

- Пакеты разрушения (DestroyIt, RayFire и подобные) ориентированы на rigid-body и рантайм-фрактуру, а не на «заранее нарезанные куски + свой солвер». Для этого проекта практически бесполезны. [выдача] https://discussions.unity.com/t/released-destroyit-cinematic-destruction-system/540859; оценка применимости — [память].

**(c) Что писать самим — самая дешёвая надёжная схема (моя рекомендация)**

- **Данные:** блок — запись в NativeArray (битовая маска из 22 кусков, материал, HP). GameObject-на-кусок не делать: при тысячах блоков × 22 это 10^4–10^5 объектов, дорогих по трансформам и памяти.
- **Рендер:** чанки, например 8×8×этаж блоков, с одним объединённым мешем. При изменении чанк пересобирается в Burst-джобе из заранее запечённых вершинных буферов кусков. Альтернатива — BRG с инстансом на кусок; подходит, если уникальных мешей мало.
- **Коллизия:** отдельный упрощённый MeshCollider на чанк, `Physics.BakeMesh` в джобе, подмена на главном потоке. Либо вообще без PhysX для логики: лучи по собственной воксельной сетке (см. п. 3).
- **Падающие обломки:** временные GameObject/Rigidbody из пула или частицы.
- **Объём:** среднее–большое, но это чисто ваша логика — движок не мешает и не помогает. ECS не обязателен: достаточно Jobs + Burst + NativeContainers.

**(d) Подводные камни**

- Пик cooking при обрушении большого здания: размазывать по кадрам.
- GRD не работает с MaterialPropertyBlock и скриптами с per-instance callbacks ([выдача] https://docs.unity3d.com/cn/current/Manual/urp/make-object-compatible-gpu-rendering.html); несовместимый с SRP Batcher шейдер рисуется magenta ([выдача] https://issuetracker.unity.com/issues/24288/).
- Read/Write-меши удваивают память.

**Вышедшие игры**

- **Menace (Overhype, EA 05.02.2026)** — описывается как игра на Unity, команда 13 человек. [выдача] https://indiegames.substack.com/p/hard-contact-menace, https://www.gamesmarket.global/menace-from-overhype-and-hooded-pulls-off-a-resounding-launch/. Технического описания разрушения в дневниках не найдено. [выдача] https://wiki.hoodedhorse.com/MENACE/Dev_Diaries
- **Phoenix Point** — Unity; рецензии описывают разрушаемые укрытия и стены, но «без развитого разрушения окружения». Технических деталей нет. [выдача] https://www.Wikipedia.org/wiki/Phoenix_Point, https://www.pcgamer.com/uk/phoenix-point-review
- **Xenonauts 2** — Unity; тайл имеет 3 состояния (целый / повреждён / уничтожен), то есть подмена префаба, без структурного расчёта. [выдача] https://en.wikipedia.org/wiki/Xenonauts_2, https://pcgamer.com/xenonauts-dev-diary-exclusive-first-ground-combat-screens-and-walkthrough
- **Jagged Alliance 3** — не Unity, собственный движок Haemimont с Lua. [память]
- **Deliver At All Costs (Unity)** — самый близкий опубликованный разбор. Художники режут здание на части в Blender с метаданными, кастомный инструмент в Unity дробит дальше, запекание «одной кнопкой». Около 100 физических частей на здание, жёсткий лимит живых физ-объектов, уровни «здание → физ-части → меш-частицы», 60 fps на PS5. Terrain пришлось запечь в меш, чтобы сэкономить пару мс. [стр.] https://unity.com/blog/breaking-down-the-completely-destructible-world-of-deliver-at-all-costs
- **Вывод:** публичного примера Unity-тактики со структурным обрушением многоэтажек на тысячах блоков я не нашёл; это будет собственная разработка.

## 2. Освещение: полностью динамическое, много теневых источников

**(a) Готовое**

- **URP Forward+:** источников на объект — без лимита, на камеру — до 256 (desktop). [стр.] https://docs.unity.com/en-us/engine/6000.6/manual/render-pipelines/universal-render-pipeline/introduction/urp-concepts/rendering-paths/comparison
- **Тени в URP:** один общий атлас на все spot и point. Point-источник занимает 6 карт, spot — 1; атлас 1024² вмещает шестнадцать карт 256². [стр.] https://docs.unity.com/en-us/engine/6000.3/manual/lighting-overview/lighting/shadows-in-urp/shadow-resolution-urp
- При переполнении атласа часть источников теряет тени, и это «by design». [выдача] https://issuetracker.unity3d.com/issues/urp-some-lights-and-shadows-are-not-visible-when-light-dot-shadowattenuation-is-used-with-custom-shadow-shader
- **HDRP:** настройка Max Shadows on Screen; есть кэшируемые теневые атласы для punctual-источников и `HDCachedShadowManager`. Значение по умолчанию не подтверждено. [выдача] https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.1/manual/Shadows-in-HDRP
- В HDRP есть SSGI, SSR с ray-traced-режимом и path tracing — по реплике сотрудника Unity в треде. [стр.] https://discussions.unity.com/t/feedback-request-changes-to-unitys-dynamic-gi-roadmap/1660897
- **Enlighten Realtime GI:** Unity 6 — последний поддерживаемый релиз, и геометрия в нём всё равно статична. [выдача] https://discussions.unity.com/t/feedback-request-global-illumination-changes-with-the-unity-6-beta-2023-3-release/941601
- **APV:** пробы расставляются предрасчётом, то есть геометрия статична, меняются только свет и небо. Dynamic APV официально отменён. [стр.] https://discussions.unity.com/t/feedback-request-changes-to-unitys-dynamic-gi-roadmap/1660897
- **Surface Cache GI (URP):** полностью динамический диффузный GI, реагирует на добавление и разрушение геометрии. Аппаратный RT не обязателен (compute-fallback; аппаратный RT только на DX12). Превью с мая 2026, плановая общая доступность — 6.7 LTS. Известные проблемы: шум, запаздывание реакции, только diffuse, нужно задавать объём покрытия. [стр.] https://discussions.unity.com/t/surface-cache-gi-preview/1720494, https://unity.com/blog/unite-seoul-keynote-2026-recap

**(c) Писать самим**

- Менеджер теней для помещений: приоритет по видимости и этажу, spot вместо point где возможно, кэширование теней неподвижных источников с инвалидацией при разрушении (в URP кэша теней «из коробки» нет [память]).
- Объём малое–среднее.

**(d) Подводные камни**

- «Много теневых point-источников в помещении» упирается в атлас, а не в лимит числа источников.
- На 6.3 LTS без запекания нет GI вообще, кроме ambient и своих решений.
- SCGI к моменту выбора ещё не имеет production-истории.
- Выбор URP или HDRP почти необратим.

## 3. Запросы: все попадания по лучу с входом и выходом, вокселизация

**(a) Готовое**

- `RaycastCommand.ScheduleBatch` с `QueryParameters`: результаты команды N лежат с индекса N×maxHits, конец списка — первый результат с null-коллайдером. Флаг `hitMultipleFaces` даёт несколько попаданий на один меш; есть управление back-face и триггерами. Для «сплошных» форм (Sphere, Capsule, Box, Convex) — максимум одно попадание. [стр.] https://docs.unity.com/en-us/engine/6000.3/script-reference/unityengine/raycastcommand
- `Physics.RaycastAll` возвращает неотсортированный массив. [память]
- Материал берётся через `RaycastHit.collider.sharedMaterial` (PhysicsMaterial) либо через `triangleIndex` и субмеш; в документации RaycastCommand материал не упоминается. [память]

**(c) Писать самим**

- Точки выхода движок не даёт. Варианты: обратный луч; back-face-попадания на невыпуклых MeshCollider (double-sided); либо, что надёжнее для этого проекта, трассировка по собственной сетке блоков и кусков (DDA в Burst) с аналитической толщиной по геометрии куска. PhysX тогда нужен только для юнитов и пропсов.
- CPU-вокселизации в движке нет: писать в Burst по MeshData. Объём малое–среднее, алгоритмы стандартные. [память]

**(d) Подводные камни**

- Convex-коллайдеры дают одно попадание, и выход получается только обратным лучом.
- Буфер maxHits фиксирован на команду.
- Изменения коллайдеров видны запросам после `Physics.SyncTransforms` или шага симуляции. [память]

## 4. Навигация: тайлы 0.625 м, до 8 этажей, позы, стоимость в AP

**AI Navigation (NavMeshSurface, `NavMeshBuilder.UpdateNavMeshDataAsync`, links, agent types)**

- Рантайм-перестройка есть, включая частичную по bounds. [выдача] https://docs.unity3d.com/Packages/com.unity.ai.navigation@1.1/api/Unity.AI.Navigation.NavMeshSurface.UpdateNavMesh.html
- Навмеш не знает тайлов, поз на тайл и стоимости в AP. Позы пришлось бы эмулировать типами агентов (отдельный навмеш на каждый). Для дискретной AP-модели не подходит — моя оценка.

**A* Pathfinding Project Pro (версия 5.4.7)**

- `LayerGridGraph`: многослойная сетка, 16 слоёв по умолчанию (до 256 через define), ширина и глубина до 1024, рантайм-обновления через `IUpdatableGraph`. Только в Pro. [стр.] https://arongranberg.com/astar/documentation/stable/layergridgraph.html
- Для пошаговых игр: `BlockManager`, `SingleNodeBlocker`, `ITraversalProvider`; есть теги и штрафы. [стр.] https://arongranberg.com/astar/documentation/stable/turnbased.html, https://arongranberg.com/astar/features
- Цена Pro около $100. [выдача] https://arongranberg.com/astar/freevspro

**Вывод**

- Проходимость выводится из вашей же сетки блоков, а не из сканирования коллайдеров, и нужны позы на тайл, лазание и AP. Поэтому своя сетка с A*/Dijkstra в Burst нужна в любом случае (среднее, алгоритмически простое).
- A* Pro может сэкономить на прототипе, но подгонять его модель графа под позы и инкрементальное обновление из своих данных выйдет не дешевле собственной реализации.

## 5. Пошаговые правила

- **Официального** ничего нет. [память]
- **Turn Based Strategy Framework (Crooked Head):** $45, 114 отзывов; демо в духе Heroes of Might and Magic (2D-сетка, заклинания); в отзывах — «учебный, есть баги». [выдача] https://assetstore.unity.com/packages/templates/systems/turn-based-strategy-framework-50282/reviews
- **Turn Based Toolkit (TBTK 2/3)** — есть на Asset Store. [выдача] https://assetstore.unity.com/packages/tools/game-toolkits/turn-based-toolkit-3-tbtk-3-138465/reviews
- **Курс CodeMonkey** по XCOM-подобной игре — учебный код, качество сейчас не проверено. [память]
- **Вывод:** 17 навыков со стекающимися таймерными модификаторами, дерево перков, ранения и AP — чистый C# без зависимости от движка. Писать самим, среднее–большое. Киты под точное воспроизведение оригинала не подходят.

## 6. Редакторные инструменты

**(a) Готовое**

- `EditorTool` / `EditorToolContext`, SceneView Overlays, `Handles`, UI Toolkit для инспекторов и окон, `Undo`, `PrefabUtility`, `[ExecuteAlways]` — зрелые API. [память] https://docs.unity3d.com/Manual/UsingCustomEditorTools.html
- ProBuilder — моделирование, а не модульная сетка. [память]
- Tile Palette и GameObject Brush из 2D Tilemap Extras умеют ставить префабы по сетке, но это один слой в XY; стены по рёбрам и этажи не поддерживаются. [память]

**(b) Asset Store** — только частичные аналоги:

- Floor Plan Tracer — рисование плана с генерацией стен и проёмов. [выдача] https://discussions.unity.com/t/create-walls-window-openings-and-perfectly-fitted-windows-just-by-drawing-floor-plan-tracer-update-for-modular-window-builder/1649323
- WallPen — сеточное рисование стен и полов, несколько этажей. [выдача] https://meltpack.itch.io/wallpen
- Easy Grid Builder Pro — размещение по клеткам и рёбрам. [выдача] https://assetstore.unity.com/packages/tools/game-toolkits/easy-grid-builder-pro-239234
- Ни один не пишет в ваш формат «блок / куски / комнаты / подвалы».

**(c) Писать самим**

- Свой редактор на EditorTool + Overlay, пишущий в данные здания (ScriptableObject или бинарный формат). Немедленная разрушаемость получается бесплатно, если редактор и рантайм используют один и тот же код сборки чанков.
- Объём среднее. Каркас редактора даёт Unity; это одна из главных экономий.

**(d) Подводный камень:** domain reload при каждой правке кода инструмента. Ускорение обещано только в Unity 7; Fast Enter Play Mode по умолчанию включён с 6.6. [стр.] https://discussions.unity.com/t/coreclr-scripting-and-serialization-update-june-2026/1723299

## 7. Вложенные шаблоны карт

**(a) Готовое**

- Nested Prefabs и Prefab Variants — редакторные понятия: в билде это просто инстансы, выбора варианта по весам и seed нет. [память]
- Terrain API в рантайме: `TerrainData.SetHeights` / `SetHeightsDelayLOD` + `SyncHeightmap`, `SetAlphamaps`, `SetHoles`, `TerrainData` можно создавать на лету. [память] https://docs.unity3d.com/ScriptReference/TerrainData.html
- Terrain Tools — кисти редактора, не рантайм. [память]

**(c) Писать самим**

- Шаблон = свой ассет (ScriptableObject): слоты дочерних шаблонов, веса, наследуемые флаги, закреплённые варианты, «штамп» высот и сплат-карт. Детерминированный резолвер на собственном PRNG (не `UnityEngine.Random` с глобальным состоянием). Композиция штампов в массивы с одним вызовом SetHeights/SetAlphamaps.
- Объём среднее. Префабы пригодны как контейнер содержимого листа, не как механизм дерева.

**(d) Подводные камни**

- Глубокая вложенность prefab-override хрупка при рефакторинге.
- Разрешения heightmap и alphamap различаются, нужна передискретизация штампов.
- Terrain LOD стоил упомянутой выше игре пару миллисекунд на кадр. [стр.] https://unity.com/blog/breaking-down-the-completely-destructible-world-of-deliver-at-all-costs

## 8. Данные (~150 таблиц, рулетки, оверлеи модов)

- **ScriptableObject** — типизированные записи со ссылками, но полевого оверлея для модов нет. **Addressables** — загрузка и каталоги контента, включая сторонние каталоги для модов. [память]
- **Практичнее:** источник истины — текст (JSON/TSV) с ID-ссылками; свой загрузчик со слиянием оверлеев по полям; в редакторе — ScriptedImporter или кодогенерация типов. Newtonsoft.Json доступен как пакет `com.unity.nuget.newtonsoft-json`. [память]
- `com.unity.serialization` объявлен deprecated с 6.6; Unity советует System.Text.Json, Odin Serializer или MemoryPack. [стр.] https://discussions.unity.com/t/coreclr-scripting-and-serialization-update-june-2026/1723299
- **SQLite:** официального пакета нет; используют sqlite-net или Microsoft.Data.Sqlite с нативной библиотекой. Статус сейчас не проверял. [память]
- **Объём:** среднее; движок почти ничего не даёт и ничем не мешает.

## 9. Скрипты и сохранения

- **C# coroutines и async** — состояние не сериализуется. **Timeline** — можно сохранить и восстановить время PlayableDirector, но не произвольную логику. **Unity Visual Scripting** — штатного сохранения состояния исполнения графа нет. [память]
- **Lua:**
  - MoonSharp (Lua 5.2, работает на IL2CPP/AOT), форки Coderz.MoonSharp.Plus, SolarSharp; Lua-CSharp — более быстрый интерпретатор, Unity 2021.3+. [выдача] https://github.com/moonsharp-devs/moonsharp, https://github.com/annulusgames/lua-csharp
  - Сериализацию приостановленной корутины MoonSharp не подтвердил ни одним источником; по памяти её там нет. [память]
  - Для нативного Lua существует Eris (персистентность Lua 5.2/5.3, включая yielded-корутины). [выдача] https://www.Github.com/fnuecke/eris. Потребуется свой нативный плагин и биндинги; интеграция Eris с xLua/NLua не проверялась.
- **Самый дешёвый надёжный путь (моя оценка):** собственный маленький интерпретатор или байткод-VM для линейных скриптов (sleep, wait_arrive, параллельные потоки, триггеры) с явно сериализуемым состоянием: IP + локальные переменные + стек. Катсцены писать тем же языком, а не Timeline, иначе сохранение в середине катсцены не гарантировать. Объём среднее.
- **Сохранения:** встроенной системы нет. [память]. Easy Save 3 (3.5.27, обновление 18.07.2026) сериализует типы Unity и ссылки. [выдача] https://assetstore.unity.com/packages/tools/utilities/easy-save-the-complete-save-load-tool-for-unity-768. Для вашей модели (свои массивы блоков, таблицы, VM) проще собственный бинарный формат; Easy Save мало что добавит.

## 10. Презентация — здесь Unity экономит больше всего

- **Анимация:** Mecanim (Animator, Animation Events, слои, blend trees), пакет Animation Rigging (IK), Ragdoll Wizard и CharacterJoint. [память]
- **Лицо и липсинк:** официального нет.
  - SALSA LipSync Suite v2 — заявлена работа с Unity 6, но в отзывах были проблемы с Timeline-модулем. [выдача] https://assetstore.unity.com/packages/tools/animation/salsa-lipsync-suite-148442
  - uLipSync — open source, Jobs + Burst. [выдача] https://github.com/hecomi/uLipSync
  - Rhubarb — офлайн-расчёт. [выдача] https://fr.esotericsoftware.com/forum/d/27352-lipsync-in-realtime-unity
- **Частицы:** VFX Graph (GPU) и Shuriken. [память]
- **Декали:** Decal Projector в URP (режимы DBuffer и Screen Space) и HDRP проецируются на текущую геометрию. При удалении куска декаль не «висит», но попадёт на то, что за ним; нужны Rendering Layers и свой учёт декалей по кускам. [память]. В Deliver At All Costs оптимизация декалей дала заметную экономию. [стр.] https://unity.com/blog/breaking-down-the-completely-destructible-world-of-deliver-at-all-costs
- **Трава с разрушением:** Terrain details можно править через `SetDetailLayer`; для выжигания и сминания обычно пишут свой инстансинг. [память]
- **UI:** uGUI (зрелый) и UI Toolkit (рантайм + редактор). **Аудио:** встроенное или FMOD/Wwise. **Localization package, Cinemachine 3** — официальные пакеты; их статус на 6.3 сейчас не проверял. [память]
- **Туман войны, срез этажей:** готового нет. Делается шейдером (clip по высоте, Shader Graph), слоями или Rendering Layers плюс своей картой видимости. Если чанки нарезаны по этажам (п. 1), срез почти бесплатен. Объём малое–среднее.

## 11. Пайплайн из Blender

- FBX-импорт сохраняет иерархию дочерних объектов. [память]
- `AssetPostprocessor`: `OnPostprocessModel`, `OnPostprocessMeshHierarchy`, `OnPostprocessGameObjectWithUserProperties` (читает custom properties из Blender), плюс `ScriptedImporter` для своих форматов. Этого достаточно для автоматики по именам. [память] https://docs.unity3d.com/ScriptReference/AssetPostprocessor.html
- Подход подтверждён практикой: метаданные из Blender и запекание одной кнопкой в Deliver At All Costs. [стр.] https://unity.com/blog/breaking-down-the-completely-destructible-world-of-deliver-at-all-costs
- **Писать самим:** объём куска (сумма знаковых тетраэдров; меш должен быть замкнутым), смежность (совпадение граней или близость AABB с допуском), валидатор. Объём малое.
- **Подводные камни:** масштаб и оси FBX из Blender; import-версию постпроцессора надо повышать вручную, иначе не будет реимпорта.

## 12. Факторы стоимости

**Цены**

- Personal — бесплатно при выручке и финансировании до $200 000; заставка необязательна.
- Pro — $2 310 в год за место ($210 в месяц) с 12.01.2026; обязателен выше $200K.
- Enterprise — от $25M выручки, цена по запросу.
- Runtime Fee отменён 12.09.2024 и не применяется.
- Объявлений о ценах на 2027 год не найдено.
- [стр.] https://unity.com/products/pricing-updates

**Доступ к исходникам движка**

- Только в составе Enterprise (цена по запросу). [выдача] https://unity.com/products/pricing-updates
- C#-часть движка доступна для чтения (UnityCsReference); исходники URP/HDRP и пакетов открыты и модифицируемы. [память]
- Глубокие правки нативного ядра (PhysX-обвязка, рендер-ядро, Terrain) на Pro практически недоступны.

**Платформы**

- Windows, macOS, Linux, iOS, Android, Web (WebGPU «production-ready» в 6.6 [стр.] https://unity.com/blog/unite-seoul-keynote-2026-recap), консоли. Полный список не перепроверял. [память]

**Скорость итерации**

- Fast Enter Play Mode по умолчанию для новых проектов с 6.6; Burst стал встроенным модулем в 6.6.
- Domain reload после компиляции остаётся до Unity 7 (CoreCLR). Серьёзная оптимизация производительности CoreCLR отложена на LTS после 7.0, то есть на 2027 год.
- [стр.] https://discussions.unity.com/t/coreclr-scripting-and-serialization-update-june-2026/1723299

**Разработчики**

- Unity — около 51% релизов в Steam за 2024 год и около 49% за 2025; по выручке Steam 26% против 31% у Unreal. [выдача] https://gamedevreports.substack.com/p/video-game-insights-game-engines, https://newsletter.gamediscover.co/p/hows-pc-game-engine-usage-changing

**ИИ-ассистенты**

- Официальный Unity MCP-сервер (пакет `com.unity.ai.assistant`, открытая бета с 11.05.2026; клиенты — Claude Code, Cursor и другие). Обещано, что Unity CLI и MCP станут бесплатными без подписки. [выдача] https://unity.com/blog/unity-ai-mcp-how-to-get-started; [стр.] https://unity.com/blog/unite-seoul-keynote-2026-recap
- Независимых бенчмарков качества LLM именно на Unity C# не найдено.

**Размеры команд**

- Menace — 13 сотрудников. [выдача] https://www.gamesmarket.global/menace-from-overhype-and-hooded-pulls-off-a-resounding-launch/
- Phoenix Point (Snapshot) — около 30–35 разработчиков, позже 50+. [выдача] https://phoenixpoint.info/blog/2018/9/4/the-growth-of-snapshot-games, https://www.mcvuk.com/julian-gollop-how-facebook-is-keeping-the-cash-rolling-in-for-phoenix-point/
- Xenonauts 2 (Goldhawk) — ядро 8–9 человек, всего одна–две дюжины; источник недатирован. [выдача] https://videogamegeek.com/videogamedeveloper/22874/goldhawk-interactive

**Доверие и стабильность (факты)**

- Runtime Fee объявлен в 2023 году, отменён 12.09.2024. [стр.] https://unity.com/products/pricing-updates
- Ежегодные повышения цен: +5% с 01.2026. [стр.] там же
- Havok Physics убран из подписок начиная с 6.3. [стр.] там же
- CVE-2025-59489 (CVSS 8.4): затронуты сборки с 2017.1, патчи выпущены 02.10.2025, требовалась пересборка игр. [выдача] https://unity.com/en/security/sept-2025-01
- Dynamic APV отменён в 2025 году. [стр.] https://discussions.unity.com/t/feedback-request-changes-to-unitys-dynamic-gi-roadmap/1660897
- Сроки CoreCLR сдвигались (6.8 → 7.0). [выдача] https://discussions.unity.com/t/coreclr-scripting-and-ecs-status-update-march-2026/1711852
- В 2026 году закрыта ironSource Ads и продан Supersonic. [выдача] https://yespress.io/unity
- Сообщений об увольнениях в 2026 году поиск не дал.

## Итоговая таблица

| # | Требование | Статус | Объём своей работы |
|---|---|---|---|
| 1 | Разрушаемые здания (рендер, коллизия, солвер) | писать самим (движок даёт Mesh API, Jobs/Burst, BakeMesh, GRD/BRG) | большое |
| 2 | Динамический свет и тени | частично (свет и тени готовы; GI без запекания — только SCGI в 6.7) | малое–среднее |
| 3 | Лучи «все попадания, вход и выход», вокселизация | частично (RaycastCommand есть; выход, толщина и воксели — сами) | среднее |
| 4 | Навигация по тайлам, этажи, позы, AP | писать самим (A* Pro — частичная замена) | среднее |
| 5 | Правила (навыки, перки, ранения, AP) | писать самим | среднее–большое |
| 6 | Редактор зданий | частично (каркас редактора готов, сам инструмент — сами) | среднее |
| 7 | Вложенные шаблоны и штампы террейна | частично (Terrain API и префабы; резолвер — сами) | среднее |
| 8 | Данные и моды | частично (SO, Addressables; оверлеи — сами) | среднее |
| 9 | Скрипты с сохранением в ожидании, сейвы | писать самим | среднее |
| 10 | Анимация, IK, ragdoll, VFX, UI, звук, камера, локализация | готово | малое |
| 10 | Липсинк | готово через плагины (SALSA, uLipSync) | малое |
| 10 | Декали на меняющейся геометрии, трава, туман войны, срез этажей | частично | малое–среднее |
| 11 | Импорт из Blender, объём и смежность | частично (постпроцессоры готовы) | малое |
| 12 | Платформы, сборка, инструментарий | готово | — |

## Три главных риска Unity для этого проекта

1. **Освещение без запекания при меняющейся геометрии.** На 6.3 LTS нет production-готового динамического GI; Surface Cache GI — превью, ожидается в ещё не вышедшей 6.7 LTS. Число теневых источников в URP ограничено атласом и требует своего менеджера, а смена пайплайна позже крайне дорога.
2. **Ядро игры не покрывается движком.** Разрушение, солвер, сетка, правила, скриптовая VM и шаблоны карт пишутся самостоятельно при любом выборе. Реальная экономия Unity сосредоточена в презентации и инструментах. Нативное ядро закрыто без Enterprise, так что узкие места (cooking PhysX, Terrain) обходятся только своими данными.
3. **Движущаяся платформа в 2026–2027.** Идёт переход Mono → CoreCLR (Unity 7, Q1 2027), ECS-унификация с deprecated-API, отмены вроде Dynamic APV и Havok, ежегодный рост цены. Долгому проекту придётся фиксировать LTS и планировать одну-две миграции.

## Три главные экономии

1. **Презентационный слой целиком:** анимация, IK, ragdoll, VFX Graph, декали, UI, звук, Cinemachine, локализация, плюс готовые плагины липсинка.
2. **Редактор и ассет-пайплайн:** EditorTool, Overlays, UI Toolkit, AssetPostprocessor, префабы. Инструмент дизайнера и импорт нарезанных блоков строятся поверх готового каркаса, без собственного редактора с нуля.
3. **Производительная база и экосистема:** Jobs, Burst и NativeContainers для солвера, вокселей и поиска пути на C#; RaycastCommand; GPU Resident Drawer; мультиплатформенные сборки; самый большой рынок разработчиков; официальный MCP для ИИ-ассистентов.

## Что не удалось проверить

- Выход 6.7 LTS и его точная дата; официальная страница релизов 6.7 не упоминает.
- Как именно реализовано разрушение в Menace, Phoenix Point и Xenonauts 2. Движок Menace подтверждён только сниппетом вторичного источника.
- Точные лимиты теневых источников: значение Max Shadows on Screen по умолчанию в HDRP; правило отбора источников при переполнении атласа в URP.
- Умеет ли MoonSharp или xLua сериализовать приостановленные корутины (по памяти — нет); работоспособность Eris в связке с Unity.
- Текущий статус на Unity 6.3/6.6 для Asset Store-пакетов: TBS Framework, редакторы зданий, Easy Save, SALSA, A* Pro. Цена A* Pro — только из сниппета.
- Всё с пометкой [память]: Mesh API, Terrain API, AssetPostprocessor, декали, UVS, Timeline, список платформ, открытость C#-исходников.
- Независимые данные о качестве ИИ-ассистентов на Unity C# — не найдены.
- Страница-рекап Unite на gamedev.net вернула 403; использован первоисточник на unity.com.