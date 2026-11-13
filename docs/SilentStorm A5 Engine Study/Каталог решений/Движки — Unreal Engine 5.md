---
tags: [silent-storm, каталог, справочник, движок]
studied: 2026-11-12
---

> [!note] Справочный файл
> Собранные 12.11.2026 сведения с источниками: что движок даёт готовым для каждого требования каталога и что придётся писать. Вывод — в [23 Выбор движка](<../23 Выбор движка.md>).
> Пометки: **[стр.]** — прочитано на странице, **[выдача]** — только фрагмент поиска, **[память]** — не проверялось.

# Unreal Engine 5.8 для ремейка тактики 2003 года с разрушаемыми зданиями: что экономит, что писать самим

Метки: **[стр.]** — прочитано на странице, **[выдача]** — только сниппет поиска, **[память]** — сейчас не проверено. Документация dev.epicgames.com и unrealengine.com не открылись (JS-рендер / 403), поэтому всё оттуда — [выдача].

## Версия

- UE 5.8 вышел 17.06.2026. В посте о релизе Production-Ready объявлены MegaLights, Dataflow, Movie Render Graph, Live Link Hub. [стр.] https://forums.unrealengine.com/t/unreal-engine-5-8-released/2729274
- Актуальный хотфикс — 5.8.4 (20–27.10.2026, даты в источниках расходятся). [выдача] https://forums.unrealengine.com/t/5-8-4-hotfix-released/2842775
- 5.8 описывают как последний крупный релиз UE5; UE6 анонсирован 17.06.2026, ранний доступ называют на конец 2027 (вторичные источники, страница Epic дала 403). [выдача] https://www.unrealengine.com/en-US/news/the-road-to-ue-6

## 1. Здания

**Готово:**
- `UInstancedStaticMeshComponent`: `AddInstances`, `RemoveInstances`, per-instance custom data. [выдача] https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UInstancedStaticMeshComponent/RemoveInstances
- Geometry Collection управляется кодом: `RemoveAllAnchors`, `SetAnchoredByIndex/ByBox`, `ApplyExternalStrain`, `CrumbleCluster`, `CrumbleActiveClusters`; в C++ — `FCollectionAnchoringFacade::SetAnchored`. [стр.] https://forums.unrealengine.com/t/how-to-turn-off-anchor-at-runtime/2332757 ; [выдача] https://forums.unrealengine.com/t/what-is-a-chaos-crumbling-event/2272545
- Выпущенный пример «здание из блоков + своя логика связности»: в LEGO Fortnite Odyssey связность держит высокоуровневый граф (плагин GameplayGraph), каждый связный остров — свой `ClusterUnionComponent`. Решение «должна ли конструкция упасть» оставлено разработчику. [стр.] https://forums.unrealengine.com/t/geometry-collections-cluster-unions-and-connectivity-graph/2075192
- Отладка связей: `p.Chaos.Solver.DebugDraw.Cluster.Constraints 1`. [стр.] там же

**Писать самим:**
- Структурный солвер, граф смежности, хранилище состояния блоков — целиком свои. Документированного API «подменить damage-модель» я не нашёл; пороги задаются массивом DamageThreshold, покостная настройка у Epic «в wish list». [выдача] https://forums.unrealengine.com/t/best-way-to-configure-a-geometry-collection-to-allow-for-different-damage-thresholds/2404784
- Самый дешёвый надёжный путь (мой вывод, не цитата): каждый кусок — инстанс обычного ISM с Nanite-мешем, компоненты по чанкам; коллизия ISM выключена, запросы идут по собственной сетке кусков. Geometry Collection или обычные rigid body спавнить только для падающих обломков как визуал.
- Отдельный GC на каждый из тысяч блоков — без подтверждений производительности; вопрос об этом в ветке остался без ответа. [стр.] ветка cluster unions выше

**Подводные камни:**
- Add/Remove в ISM в UE5 медленнее, чем в 4.27: «непригодно после 10 000 инстансов» против ~80 000; 250k инстансов — 10 с в 4.27 против ~5 мин в 5.7. Основная причина по отчётам — коллизия; async-кука коллизии для ISM нет; ответа Epic в ветке нет. [стр.] https://forums.unrealengine.com/t/what-happened-to-the-instanced-static-mesh-performance-in-ue5/2107140
- `RemoveInstance` сдвигает индексы оставшихся — нужна своя таблица «кусок → инстанс» либо скрытие вместо удаления. [выдача] https://forums.unrealengine.com/t/remove-instance-shifts-index-values-of-remaining-instances/ (answers.unrealengine.com/questions/385682)
- HISM при изменениях перестраивает дерево кластеров; для Nanite-мешей советуют обычный ISM (неавторитетный блог). [выдача] https://www.strayspark.studio/blog/hism-vs-ism-unreal-engine-explained
- Nanite нельзя собрать в рантайме; `DynamicMeshComponent` не поддерживает Nanite и Lumen. Заранее нарезанные художником куски — подходящий случай, генерация мешей на лету — нет. [выдача] https://forums.unrealengine.com/t/is-generation-of-nanite-meshes-during-runtime-possible/655107 ; https://dev.epicgames.com/documentation/unreal-engine/geometry-scripting-users-guide-in-unreal-engine
- Geometry Script в 5.8.3: функции записи в Dynamic Mesh падают в упакованных сборках, фикса нет (отчёт от 02.10.2026, подтверждён вторым пользователем). [стр.] https://forums.unrealengine.com/t/all-geometry-script-mesh-writing-functions-crash-on-first-call-in-packaged-non-shipping-and-shipping-builds-ue-5-8-3-windows/2836311
- The Finals (UE): разрушение целиком на сервере; использует ли оно Chaos — не подтверждено, доклад GDC за пейволлом. [выдача] https://gdcvault.com/play/1034280/Eng
- Kingmakers остались на UE 4.27 ради скорости физики разрушений (вторичная вики). [выдача] https://thegameswiki.com/kingmakers/wiki/destructible-environments

## 2. Освещение

**Готово:**
- MegaLights — Production Ready в 5.8. [стр.] форум-релиз выше
- Тени MegaLights: трассировка лучей (по умолчанию, без доплаты за источник) или VSM (карта на каждый источник). [выдача] https://dev.epicgames.com/documentation/unreal-engine/megalights-in-unreal-engine
- В 5.8 MegaLights экономит 0.3–1 мс на консоли; Lumen Lite — новый Medium-путь GI. [стр.] https://tomlooman.com/unreal-engine-5-8-performance-highlights/
- Lumen: ISM поддерживаются только с Nanite; при множестве перекрывающихся инстансов производителен только software-трейсинг. [выдача] https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine

**Подводные камни:**
- RT-сцена MegaLights строится из упрощённых Nanite-fallback мешей — возможны протечки и пропавшие тени на мелких кусках. [выдача] там же (megalights)
- Сотни одинаковых инстансов дают сотни наборов карточек Lumen; card sharing в 5.8 упомянут только в форумном посте, там же жалоба на регресс game thread до 44 мс. [выдача] https://forums.unrealengine.com/t/ue-5-8-editor-fps-regression-vs-5-3-when-r-lumen-diffuseindirect-allow-1-ism-mannequin-grid-game-thread-44ms/2835880
- VSM держатся на кэше страниц; не-Nanite геометрия, покрывающая много страниц, дорога. [выдача] https://www.unrealengine.com/en-US/tech-blog/virtual-shadow-maps-in-fortnite-battle-royale-chapter-4
- Троттлинг инвалидаций: `r.Shadow.Virtual.DeferredInvalidationBudget`. [стр.] tomlooman
- Измерений «обрушение здания при десятках затенённых источников» я не нашёл — нужен свой прототип.

## 3. Запросы

**Готово:**
- `LineTraceMulti*` возвращает попадания только до первого блокирующего включительно. [выдача] https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UKismetSystemLibrary/LineTraceMulti
- Точек выхода нет: применяют обратный луч или перезапуск от точки попадания; описаны ненадёжность и баг при старте из точки попадания. [выдача] https://forums.unrealengine.com/t/linetrace-undefined-behavior-when-starting-from-hit-confirmed-bug/755915
- `AsyncLineTraceByChannel` отдаёт результат на следующем кадре. [выдача] https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UWorld/AsyncLineTraceByChannel
- Лучи из рабочих потоков под read-lock сцены на практике работают, но гарантии в документации я не нашёл; есть отчёт о дедлоке. [выдача] https://forums.unrealengine.com/t/chaos-scene-lock-type-and-hitching-in-cooked-builds/2587590 ; https://issues.unrealengine.com/issue/UE-195356
- Физматериал приходит в `FHitResult` (`bReturnPhysicalMaterial`); все попадания получают, выставив каналу ответ Overlap. [память]

**Писать самим:**
- Свой лучевой запрос по сетке кусков (вход/выход/материал, пакеты в `ParallelFor`) — малый/средний объём; заодно убирает зависимость от коллизии ISM.
- Готового CPU-вокселизатора уровня нет. Distance fields живут на GPU, воксели Recast наружу не отдаются [память]. Вокселизацию для взрыва и видимости проще строить из собственных данных блоков.

## 4. Навигация

- Recast: режим Dynamic перестраивает меш при изменении геометрии, DynamicModifiersOnly — только модификаторы. [выдача] https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/NavigationSystem/ERuntimeGenerationType
- В 5.8 сокращена память навмеша и зависание `RemoveTiles` при множестве «грязных» тайлов. [стр.] tomlooman
- Supported Agents различаются радиусом и высотой, nav links покрывают лазание. [память]
- Тайлы 0.625 м, позы на тайл и цена в очках действия Recast не выражает. Нужна своя сетка и свой A* с инкрементальным обновлением — средний объём. Движок не мешает: путь исполняется своим компонентом движения. [память]

## 5. Правила

- GAS даёт атрибуты, эффекты со стеками, теги, cues, задачи способностей — пригодны и в пошаговой игре. [стр.] https://forums.unrealengine.com/t/gameplay-ability-system-turn-based/126620
- Длительности только в секундах, политики «Has Turns» нет. Обходы: бесконечные эффекты + счётчик ходов в SetByCaller, либо свой таймер-менеджер; `CheckDurationExpired` не виртуальная. Участники называют это «не для того спроектировано». [стр.] там же
- Образец с модифицированным GAS: https://github.com/BinaryBard996/TurnBasedSample (не открывал). [стр.] упомянут там же
- Пошагового сэмпла от Epic я не нашёл.
- Киты: Advanced Turn Based Tile Toolkit, Turn-Based Tactic Strategy Template (листинг для 5.1). Актуальность под 5.8 не подтверждена; разрушаемости и этажности в описаниях нет. [выдача] https://www.fab.com/migrated-listing/a22867b136314748af7437c635b9ddba ; https://alexquevillon.gumroad.com/l/TurnBasedTactic

## 6. Инструменты редактора

- Scriptable Tools открывает Interactive Tools Framework для Blueprint; базовый класс `UScriptableInteractiveTool`; числится в списке Beta. [выдача] https://dev.epicgames.com/documentation/unreal-engine/scriptable-tools-system-in-unreal-engine ; https://dev.epicgames.com/documentation/unreal-engine/beta-features
- В 5.7.0 включённый плагин ломал shipping-сборку (UE-355270). [выдача] https://forums.unrealengine.com/t/can-not-package-shipping-builds-when-enable-scriptabletoolsframework-plugin-in-ue5-7-0/2683385
- PCG — production-ready с 5.7, детерминизм по seed, генерация в редакторе и рантайме. [выдача] https://www.tweaktown.com/news/107858/unreal-engine-5-7-preview-now-out-with-production-ready-procedural-content-generation-framework/index.html
- Рекомендация (мой вывод): свой `UEdMode` на ITF в C++ для рисования стен и плит, комнат и подвалов; результат пишется в данные сетки, а ISM перестраиваются тем же кодом, что в игре. Объём средний/большой.

## 7. Вложенные шаблоны карт

- Level Instance и `LoadLevelInstance` подходят для геометрии и акторов. [память]
- Ландшафт внутри Level Instance не работает: два экземпляра одного уровня получают одинаковый GUID, второй падает с assert; навмеш на таком ландшафте не строится. [выдача] https://issues.unrealengine.com/issue/UE-94532 ; https://forums.unrealengine.com/t/adding-landscape-to-level-instance/924272
- Landscape Patch — инструмент редактора; результат запекается, данные editor-only. [выдача] https://dev.epicgames.com/documentation/unreal-engine/landscape-patch-system
- Изменение высот Landscape в упакованной игре не поддерживается (функции импорта вырезаны). [выдача] https://forums.unrealengine.com/t/runtime-landscape-editing/631305
- Mesh Terrain в 5.8 — экспериментальный (по слову пользователя форума). [стр.] форум-релиз
- Вывод: штамповка высот вложенных шаблонов штатно возможна только в редакторе (запекание по seed). В рантайме нужен свой террейн (меш из собственной карты высот). Дерево шаблонов с весами, наследованием флагов и закреплёнными вариантами — свой код, PCG лишь помогает.

## 8. Данные

- DataTable (включая Composite), Data Assets / Primary Data Assets, Gameplay Tags, Data Registry — готово. [память]
- Оверлея модов на уровне поля нет; Composite DataTable заменяет строку целиком. [память]
- Моды: pak-файлы сортируются по приоритету и порядку монтирования; mod.io рекомендует content-only плагин через DLC cooker и поиск через Asset Registry. [выдача] https://forums.unrealengine.com/t/primer-loading-content-at-runtime/536669 ; https://docs.mod.io/unreal/ugc-best-practices
- ~150 таблиц с перекрёстными ссылками и «рулетками» ложатся на USTRUCT + DataTable; полевой merge и текстовый формат для моддеров — малый/средний объём своего кода.

## 9. Скрипты и сохранения

- SaveGame сериализует только свойства с флагом SaveGame. Delay, латентные действия и таймеры не сохраняются. [выдача] https://forums.unrealengine.com/t/serializing-delays-into-a-savegame-and-restarting-them-with-the-same-timeremaining-when-loading/417620
- Сохранение позиции Sequencer в катсцене — источников не нашёл; делается вручную. [память]
- Angelscript (Hazelight): «набор модификаций движка и плагин», то есть свой билд движка; на нём вышли It Takes Two и Split Fiction; hot reload. О сериализации состояния скриптов страница молчит. [стр.] https://angelscript.hazelight.se/
- Verse вне UEFN недоступен; ответов Epic в ветке нет, только мнения участников от 06.10.2026. [стр.] https://forums.unrealengine.com/t/questions-about-verse-roadmap-performance-standalone-games-and-c-future/2837472
- UnLua заявляет UE 4.17–5.x, sluaunreal — до 5.3; поддержка 5.8 и сериализация корутин не подтверждены. [выдача] https://github.com/Tencent/sluaunreal/releases
- SkookumScript заброшен. [память]
- Вывод: сохраняемую посреди ожидания скриптовую VM (линейные скрипты, параллельные потоки, триггеры) писать самим или встраивать — средний/большой объём; движок здесь почти не помогает.

## 10. Презентация

- Готово: AnimBP и notifies, Control Rig / IK, ragdoll и Physical Animation, Niagara, декали, UMG / CommonUI, аудио (MetaSounds), локализация, Sequencer и камеры. [память]
- Лицо: аудио-решатель MetaHuman Animator работает в реальном времени с 5.6, но Epic «напрямую не поддерживает» его в рантайме игры. [выдача] https://forums.unrealengine.com/t/mh-animator-runtime-lipsync-from-audio/2611561 ; https://dev.epicgames.com/documentation/en-us/metahuman/audio-driven-animation
- Офлайн-запекание липсинка в ассеты подходит для озвученной игры; рантайм — сторонние плагины (Runtime MetaHuman Lip Sync, заявлено 5.0–5.7). [выдача] https://forums.unrealengine.com/t/runtime-metahuman-lip-sync-real-time-offline-lip-sync-with-audio-tts-cc4-genesis-arkit-and-more/2383289
- Туман войны и срез этажей: штатного нет; есть плагины (Layered Fog of War с состоянием на этаж, Isometric Wall Hiding). [выдача] https://www.unrealengine.com/marketplace/en-US/product/isometric-wall-hiding-and-line-of-sight-plugin
- Срез по этажам на ISM делается через per-instance custom data или скрытие чанков — малый объём. [память]
- Декали на удаляемых кусках и трава с разрушением — готового решения не нашёл; своя логика, малый/средний объём.
- CommonUI не отмечен как production-ready (слово пользователя). [стр.] форум-релиз

## 11. Пайплайн из Blender

- Interchange: `UInterchangePipelineBase`, пайплайны на C++, Python или Blueprint; у `InterchangeGenericMeshPipeline` есть флаг объединения мешей. [выдача] https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/InterchangeCore/UInterchangePipelineBase ; https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/InterchangeGenericMeshPipeline
- Импорт блока с дочерними кусками по соглашению имён, расчёт объёма и смежности, запись в свой DataAsset — свой пайплайн или Python-скрипт, малый/средний объём. Готового примера именно такого я не нашёл.

## 12. Факторы стоимости

- **Роялти:** 5% с выручки продукта сверх $1 млн за всё время; выручка в Epic Games Store не облагается. [выдача] https://www.unrealengine.com/en-US/license (403 при чтении)
- **Сниженная ставка:** 3.5% по программе Launch Everywhere with Epic при одновременном выходе в EGS (с 01.01.2025). [выдача] https://gamefromscratch.com/unreal-engine-launch-everywhere-with-epic/
- **Плата за место:** $1,850 в год — только для не-игровых компаний с выручкой свыше $1 млн; для игр её нет. [выдача] https://videogameschronicle.com/news/epic-confirms-its-new-unreal-engine-pricing-keeps-its-promise-not-to-change-it-for-game-developers
- **Платформы:** редактор — Windows / macOS / Linux 64-бит. [выдача] https://www.cgchannel.com/2026/06/see-5-key-features-for-cg-artists-in-unreal-engine-5-8/ Цели PS5 / Xbox Series / Switch 2 / iOS / Android — [память], официальный список не прочитан.
- **Итерации C++:** Live Coding, долгие полные сборки. [память]
- **ИИ-ассистенты:** GameEngineBench (arXiv, июль 2026) — 110 задач на UE5 C++ из 9 репозиториев; лучшая из 12 конфигураций даёт 55.5% pass@1, 31 задачу не решил никто. [стр.] https://arxiv.org/abs/2607.03525 В 5.8 есть MCP-плагин с неполной документацией. [стр.] форум-релиз
- **Сопоставимые игры:**
  - Gears Tactics (UE4, Splash Damage + The Coalition, ~4.5 года по Wikipedia; сетки нет). [выдача] https://en.wikipedia.org/wiki/Gears_Tactics Превью не описывает разрушаемого окружения. [стр.] https://www.pcgamesn.com/making-it-in-unreal/gears-tactics
  - Mutant Year Zero (UE4, «небольшая независимая студия», числа нет). [выдача] https://www.unrealengine.com/spotlights/mutant-year-zero-road-to-eden-uses-stealth-to-offer-a-fresh-take-on-tactical-adventure
  - Midnight Suns — UE4, Firaxis; Phoenix Point — Unity (подтверждено). [выдача] https://en.wikipedia.org/wiki/Phoenix_Point
  - Research and Destroy — пошаговая на UE, команда из 4 человек. [выдача] https://www.unrealengine.com/developer-interviews/research-and-destroy-highlights-how-a-team-of-four-were-able-to-re-engineer-the-turn-based-strategy-formula
  - Miasma Chronicles: ничего не найдено. XCOM 2 на UE3 с разрушаемостью — [память].

## Итоговая таблица

| # | Требование | Статус | Объём своей работы |
|---|---|---|---|
| 1 | Здания, рендер кусков (ISM + Nanite) | частично | среднее |
| 1 | Структурный солвер, связность | писать самим | большое |
| 2 | Динамический свет | готово, нужен прототип под обрушение | малое (настройка) |
| 3 | Лучи с входом/выходом, пакеты | частично (надёжнее свой запрос) | малое/среднее |
| 3 | Вокселизация, flood-fill | писать самим | среднее |
| 4 | Навигация по тайлам | писать самим | среднее |
| 5 | Правила (GAS) | частично | среднее |
| 6 | Редактор зданий | частично (каркас ITF) | среднее/большое |
| 7 | Вложенные шаблоны, геометрия | частично | среднее |
| 7 | Штамповка рельефа в рантайме | писать самим (свой террейн) | большое |
| 8 | Данные и моды | частично | малое/среднее |
| 9 | Сохраняемые скрипты, сейв в катсцене | писать самим | среднее/большое |
| 10 | Анимация, VFX, UI, звук, локализация, камера | готово | малое |
| 10 | Туман войны, срез этажей, декали на кусках | частично | малое/среднее |
| 11 | Импорт блоков из Blender | частично | малое/среднее |

## Три главных риска

1. **Производительность изменения инстансов и коллизии.** Регресс Add/Remove ISM в UE5 при десятках тысяч инстансов без ответа Epic; придётся обходить коллизию движка своей сеткой и проверять прототипом вместе с Lumen / MegaLights / VSM при обрушении.
2. **Ландшафт.** Landscape не изменяется в рантайме и не живёт в Level Instance — требование «шаблон штампует рельеф в родителя» ломает штатный террейн, если карта собирается по seed в игре, а не в редакторе.
3. **Сохранение посреди скрипта и катсцены.** Blueprint-латентность и Sequencer не сериализуются, готовой сохраняемой VM под 5.8 не найдено; плюс скорый переход Epic на UE6 / Verse.

## Три главные экономии

1. **Рендер и свет:** Nanite + Lumen + MegaLights (production-ready) + VSM — полностью динамическое освещение без запекания.
2. **Презентационный слой целиком:** анимация, IK, ragdoll, Niagara, Sequencer, UMG, звук, локализация, офлайн-липсинк MetaHuman.
3. **Инфраструктура контента:** редактор с ITF / EdMode, Interchange + Python, DataTable / DataAsset, pak-моды, PCG, платформенные сборки.

Ядро игры (солвер, сетка, навигация, правила ходов, скрипты, шаблоны карт) движок почти не сокращает.

## Что не удалось проверить

- Официальные release notes 5.8, страница лицензии и документация Lumen / MegaLights / Nanite / Landscape Patch не прочитаны напрямую.
- Точный список целевых платформ 5.8.
- Статус Scriptable Tools именно в 5.8.
- Наличие card sharing Lumen в 5.8.
- Внутреннее устройство разрушений The Finals.
- Численность команд Gears Tactics, Mutant Year Zero, Midnight Suns; движок и разрушаемость Miasma Chronicles.
- Поддержка 5.8 в UnLua / sluaunreal / Angelscript и в китах с Fab.
- Реальная стоимость VSM / Lumen при покадровом изменении тысяч инстансов — замеров не найдено.
- Потокобезопасность трасс Chaos — только форумные свидетельства.