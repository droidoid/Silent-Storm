---
tags: [silent-storm, каталог, справочник, движок]
studied: 2026-11-13
---

> [!note] Справочный файл
> Собранные сведения с источниками: чем Unreal Engine 4.27 отличается от 5.8 для каждого требования каталога. Вывод — в [23 Выбор движка](<../23 Выбор движка.md>).
> Пометки: **[стр.]** — прочитано на странице, **[выдача]** — только фрагмент поиска, **[память]** — не проверялось.

# UE 4.27 против UE 5.8 для ремейка Silent Storm: чем отличается 4.27

**Обозначения.** [стр. N] — я прочитал это на странице из списка источников под номером N. [выдача N] — видел только в сниппете поиска. [память] — сейчас не проверял, URL нет. «Вывод» — моя собственная оценка. Все данные о том, как UE5 работает с инстансами, относятся к версиям 5.4–5.7. Замеров на 5.8 я не нашёл.

**Коротко.** 4.27 выигрывает в одном, но главном для нас месте: массовое добавление и удаление инстансов вместе с коллизией (PhysX) и в целом меньшая нагрузка на CPU. Почти во всём остальном проигрывает: динамический свет, инструменты редактора, шаблоны уровней, экосистема, поддержка тулчейнов и платформ в 2026 году.

## 1. Здания из кусков
- **PerInstanceCustomData** есть начиная с 4.25 [стр. 2, 3; выдача 4]. Баг, при котором RemoveInstance портил custom data, по словам пользователя исправлен в 4.25.3 [стр. 2]. Для HISM отчёты расходятся: «In 4.27 I have this same problem» (2021) против «in 4.27.2 I don't have the issue» на миллионе инстансов (2023) [стр. 3]. В release notes 4.27 сказано, что отдельные материалы на instanced static mesh не поддерживаются, так что различать куски можно только через custom data [стр. 5].
- **Добавление и удаление инстансов.** В ветке форума про UE5 ISM пишут «basically unusable after 10000 instances», тогда как в 4.27 «great at 80000». Этот замер сделан в редакторе, с выключенными Lumen и Nanite. Там же: «250k instances in ~10 seconds (4.27). Now it takes 5 minutes (5.7)». Виноватыми называют коллизию и BVH для трассировки лучей. Ответа Epic нет. Обходные пути оттуда же: пакетный AddInstances и выключенная коллизия у ISM с отдельными коллизионными прокси [стр. 1].
- **Физика.** По умолчанию в 4.27 работает PhysX [стр. 8, 9, 71]. Chaos был только в отдельной сборке «4.27-Chaos» Preview. Epic писала, что её нельзя использовать «for the active development» [стр. 6]. К сентябрю 2022 года эту сборку убрали из лаунчера [стр. 7]. Chaos Destruction в 4.26 требовал сборки из исходников или отдельной Chaos-сборки [стр. 9]. APEX помечен deprecated [выдача 10], в UE5 его удалили [выдача 11]. Для схемы «свой солвер + удаление кусков + обломки как rigid body» PhysX достаточно (вывод).
- **ProceduralMeshComponent** умеет асинхронно готовить коллизию [стр. 1]. Но mesh distance fields строятся только заранее и только для Static Mesh, ISM, Foliage и Landscape [стр. 12]. Значит, чанки, слитые в PMC, потеряют DF-тени и DFAO. Geometry Scripting и UDynamicMesh для геометрии во время игры появились только в UE5, и то как Experimental [выдача 14].
- **Отсечение.** У ISM в UE4 нет LOD и отсечения на уровне отдельного инстанса, у HISM есть. В UE5 ISM получил отсечение на GPU [выдача 15].

## 2. Освещение
- **Movable-источники.** Стоимость их теней определяется в первую очередь числом мешей в зоне света и количеством треугольников в них. Кэш теней помогает, только пока и свет, и окружение неподвижны [стр. 16]. Пример из документации 4.26: три точечных света рендерили тени за 14,89 мс без кэша и за 0,9 мс с кэшем [выдача 17]. Обрушение сбрасывает кэш, поэтому будут скачки времени кадра (вывод).
- **DF-тени и DFAO** работают только с жёсткими мешами: Static Mesh, ISM, foliage, landscape. Неравномерный масштаб и WPO дают артефакты. Глобальное DF обновляется локально вокруг изменившихся объектов [стр. 12, 13]. Заранее нарезанные куски, у каждого из которых свой DF, сюда подходят (вывод).
- **SSGI** учитывает только то, что видно на экране, и в документации описан как дополнение к запечённому свету [стр. 18]. LPV учитывает только directional light [выдача 20]. В UE5 его заменил Lumen [стр. 21].
- **DXR-трассировка** объявлена production-ready в 4.25 [выдача 22]. В 4.27 RT-тени и отражения поддерживают WPO для ISM/HISM, Path Tracer имеет статус Beta, DFAO и RTAO нельзя включить одновременно [стр. 5]. RTGI Final Gather экспериментальный, а Brute Force самый дорогой [выдача 23]. Нужны DX12 и видеокарта с RT.
- **NVIDIA RTXGI (DDGI).** В репозитории лежит плагин только под 4.27 [стр. 24], выходил он и на Marketplace [выдача 25]. Тоже требует RT.
- **Итог по свету.** Без RT-видеокарты 4.27 не даёт полноценного непрямого освещения от многих ламп в интерьерах. В UE5 это закрывают Lumen (включая программный режим), VSM и MegaLights (вывод; прямого сравнения я не нашёл).

## 3. Трассировки
- API одинаковый в обеих версиях: LineTraceMulti*, асинхронные трассы, bReturnPhysicalMaterial, FaceIndex для complex-коллизии [память]. Multi-trace возвращает пересечения до первого блокирующего, точку выхода получают обратной трассой [память].
- Различий в поведении PhysX и Chaos в источниках я не нашёл.
- Скорость: при переносе проекта с 4.25 на 5.3/5.4 разработчик намерил world tick в 1,5–2 раза медленнее и рост стоимости «collision checks and traces» в UE5 [стр. 26]. Пользователи также сообщают, что Chaos медленнее PhysX [выдача 27].

## 4. Навигация
Recast с режимами генерации Static, DynamicModifiersOnly и Dynamic — одна и та же система в обеих версиях [выдача 28; для 4.27 — память]. Раз тайловую навигацию мы пишем сами, разница несущественна (вывод).

## 5. GAS
В 4.27 он есть, в release notes перечислены доработки [стр. 5]. Для пошаговой игры в обоих движках нужна своя логика длительности эффектов «в ходах» [выдача 29]. В 5.3 GameplayEffect перевели на компоненты, а старые свойства пометили deprecated [выдача 30]. Поэтому примеры под 5.x к 4.27 напрямую не подходят.

## 6. Инструменты редактора
- Основной механизм в 4.27 — FEdMode. UEdMode уже есть (API 4.26), и в 4.27 есть контекст Interactive Tools Framework для него [выдача 31, 32].
- Modeling Mode в 4.27 — экспериментальный плагин на FEdMode (FModelingToolsEditorMode). В UE5 он переписан на UEdMode [выдача 33, 34].
- Scriptable Tools появились только в 5.2 [выдача 35].
- Editor Utility Widgets в 4.27 есть [стр. 5]. Python — версии 3.7 [стр. 36].

## 7. Вложенные шаблоны карт
Level Instancing по документации Epic «not possible in previous versions» [стр. 37]. В 4.27 остаются подуровни, level streaming, Child Actor и «префабы» на Blueprint [память]. Раз вложенные шаблоны мы пишем на C++, потеря умеренная (вывод).

## 8. Данные и моды
- DataTable, DataAsset и Primary Assets работают одинаково [память].
- В 4.27 уже есть Data Registries (Beta) и Game Features [стр. 5; выдача 38].
- Моды через pak: пример Epic рассчитан на 4.25.1 и новее [выдача 39]. Плагин mod.io поддерживает только три последние версии UE [выдача 40], то есть 4.27 в него не входит.

## 9. Скрипты и сохранения
- USaveGame и то, что latent-действия не сериализуются, — одинаково в обеих версиях [память].
- UnLua поддерживает «4.17.x – 5.x» [стр. 41].
- sluaunreal полностью поддерживает UE 4.18 и 4.26, а из UE5 — только 5.1 [стр. 42]. Здесь 4.2x поддержана лучше, чем 5.8.
- Angelscript описан как плагин «for UE5», без гарантий сопровождения. Ветку под 4.27 подтвердить не удалось [стр. 43].

## 10. Презентация
- Control Rig и Full Body IK в 4.27 есть [стр. 5]. IK Rig и Motion Matching — только UE5 [память].
- Niagara в 4.27 зрелая: есть Debugger и версии модулей [стр. 5].
- MetaSounds — только UE5 [выдача 44].
- MetaHuman: с 05.04.2026 версию 4.27 убрали из MetaHuman Creator, веб-приложение закрыли 05.11.2026. Epic прямо пишет, что MetaHuman'ов из 4.27 нельзя перенести в новые версии [стр. 45]. Плагина Quixel Bridge для 4.27 нет [стр. 46].
- Рэгдоллы, декали, UMG, локализация и Sequencer по базовым возможностям такие же [память]. Варианты lip-sync для 4.27 проверить не успел.

## 11. Пайплайн из Blender
- Обычный FBX-импорт есть в обеих версиях [память]. Interchange появился только в UE5, и FBX через него до сих пор включается экспериментальными флагами [выдача 47].
- Автоматизация импорта через Python в 4.27 есть [стр. 36]. Datasmith в 4.27 развивался [стр. 5].
- Аддон «Send to Unreal» требовал UE 4.23 и новее. Сейчас его поддерживает сообщество, а свежие версии тестировались на 5.x [выдача 48].

## 12. Стоимость и поддержка в 2026
- **Роялти.** 5% с выручки продукта сверх $1 млн. С 01.01.2025 действует ставка 3,5% для игр, которые выходят в Epic Games Store не позже других магазинов (программа Launch Everywhere with Epic) [выдача 49]. Что условия EULA одинаковы для 4.27 и 5.x — [память].
- **Тулчейн.** Release notes 4.27 рекомендуют VS2019 16.5, минимум — VS2017 [стр. 5]. Хотфикс 4.27.2 от 07.12.2021 исправлял генерацию проектов на машинах, где стоит только VS2022 [стр. 36]. VS2026 официально не поддерживается. Один пользователь собрал движок из ветки 4.27-plus и пишет, что она «still being updated» (сентябрь 2026) [стр. 50]. Известна внутренняя ошибка компилятора в VS2022 17.1 [выдача 51].
- **Лаунчер.** В ноябре 2025 обновление лаунчера сломало у части пользователей запуск 4.26/4.27 и генерацию проектов. Помогали откат лаунчера или запуск от администратора, ответа Epic не было [стр. 52].
- **Консоли.** В release notes 4.27 указаны SDK для PS5 (3.00), Switch (12.3.2) и Xbox One XDK [стр. 5]. По словам пользователя форума (не Epic), ветку 4.27-plus создали ради обновлённых SDK и поддержки PS5 и Xbox Series [стр. 8]. Epic обновляла 4.27Plus на GitHub в мае 2023 [стр. 53]. Про Switch 2 Epic 02.04.2025 объявила, что работает над поддержкой в UE4 и UE5 [стр. 54]. Подтверждения именно для 4.27 нет, вопрос на форуме остался без ответа [стр. 55], а документация доступна только под NDA [стр. 56].
- **Лаунчер и Fab.** 4.27 в 2025–2026 годах ставили и использовали через лаунчер, но совместимость каждого ассета Fab с 4.27 приходится проверять вручную [стр. 52, 57]. Официальной позиции Fab по 4.27 я не нашёл.
- **macOS.** В 4.27 заявлена «preliminary support for native Apple Silicon for macOS targets» [стр. 5]. С Xcode 15/16 и новыми macOS сборка ломается, ответа Epic нет [стр. 58].
- **Linux и Steam Deck.** Разработчики Everspace 2 назвали Vulkan в UE4 «broken and incomplete». По их словам, Epic UE4 чинить не будет, поэтому на Linux игра идёт через Proton [стр. 59]. Valve в документации по Steam Frame рекомендует свою ветку «4.27-plus» наряду с 5.6/5.7 [стр. 60].
- **Миграция 4.27 → 5.x.** Проект открывается в 5.0 и новее, вернуться обратно нельзя. Chaos заменяет PhysX, Lumen заменяет LPV, меняются настройки SSGI и RT, нужна VS2019 или новее [стр. 21]. FVector становится double [выдача 61]. Ломается API GAS из-за изменений 5.3 [выдача 30]. MetaHuman'ы из 4.27 не переносятся [стр. 45]. При вынужденном переезде проблема ISM + Chaos вернётся (вывод).
- **Рынок.** В 2024 году 72% новых релизов на Unreal в Steam вышли на UE5 [стр. 62]. Пример релиза на UE4 в 2025 году — The First Berserker: Khazan [стр. 63]. Долю именно 4.27 найти не удалось.
- **Тактики на UE4.** Gears Tactics [стр. 64], Mutant Year Zero [стр. 65] и Marvel's Midnight Suns [стр. 66] сделаны на UE4. У Miasma Chronicles в Википедии указан просто «Unreal Engine», а пресса пишет UE5 [стр./выдача 67], так что как пример UE4 она не подходит.
- **Разрушения на UE4.**
  - Kingmakers. Разработчик в интервью Digital Foundry говорит: «It's all dynamic, courtesy of Unreal 4's fast physics calculations». Команда пробовала 5.1 и 5.3, но «nothing has been as fast on the CPU side as Unreal 4» [выдача 68]. Что это именно 4.27, утверждает только фанатская вики [выдача 68].
  - Fortnite со строительством и разрушением по сетке работал на UE4 до перехода на 5.1 в декабре 2022 [выдача 69].
  - В Deep Rock Galactic на UE4 местность вырезается в мешах («mesh-carving») [стр. 70].

## Сводная таблица

| Требование | 4.27 по сравнению с 5.8 | Почему |
|---|---|---|
| 1. Массовое удаление кусков вместе с коллизией | лучше | 80k и 250k инстансов в 4.27 против ~10k в 5.4–5.7, PhysX [1] |
| 1б. Слияние кусков и процедурная геометрия | хуже | нет Geometry Script, у ISM нет отсечения на GPU [14, 15] |
| 2. Динамический свет и GI | хуже | нет Lumen, VSM и MegaLights; GI только через SSGI, RT или DDGI [18, 23, 24] |
| 3. Трассировки | так же (вероятно, быстрее) | API тот же; пользователи меряли меньшую нагрузку на CPU [26] |
| 4. Навигация | так же | тот же Recast [28] |
| 5. GAS | так же | в 5.3 изменился API эффектов [30] |
| 6. Инструменты редактора | хуже | нет Scriptable Tools, Modeling Mode экспериментальный [33, 35] |
| 7. Шаблоны карт | хуже | нет Level Instances [37] |
| 8. Данные и моды | так же | mod.io не поддерживает 4.27 [40] |
| 9. Скрипты и сохранения | так же | Lua работает в обеих; Angelscript ориентирован на UE5 [41–43] |
| 10. Презентация | хуже | нет MetaSounds, MetaHuman и Bridge для 4.27 отключены [44–46] |
| 11. Пайплайн из Blender | так же | FBX и Python одинаково, Interchange не критичен [47] |
| 12. Стоимость и поддержка | хуже | тулчейны, лаунчер, консоли, Switch 2, macOS [5, 50, 52, 55, 58] |

## Три главных довода за 4.27
1. По отчётам пользователей, добавление и удаление инстансов вместе с коллизией в 4.27 работает на порядок быстрее: 80k против 10k, 10 секунд против 5 минут [стр. 1]. Это ядро нашей механики.
2. Меньше нагрузка на CPU, физика и трассы быстрее по замерам пользователей [стр. 26; выдача 27]. Kingmakers остался на UE4 именно ради скорости физики [выдача 68].
3. Платформа заморожена и уже не меняется. На ней выпущены тактики [стр. 64–66], для 4.2x есть зрелые Lua-биндинги [стр. 41, 42]. Визуальная планка ремейка игры 2003 года ниже той, ради которой делали Lumen и Nanite (вывод).

## Три главных довода против
1. Свет: много теневых ламп в интерьере плюс обрушения означают постоянный сброс кэша теней, а нормального GI без RT-видеокарты нет [стр. 16, 18; выдача 17, 23].
2. Платформа доживает свой срок. Официально поддерживается VS до 2022, бывали поломки лаунчера, консоли идут через 4.27-plus и NDA, Switch 2 и новые версии Xcode не подтверждены. Экосистема уходит: MetaHuman, Bridge, mod.io [стр. 5, 45, 46, 50, 52, 55, 58; выдача 40].
3. Меньше инструментов, и при переезде придётся платить второй раз. Нет Scriptable Tools, Level Instances и Geometry Script. Поздняя миграция упрётся в Chaos (та же проблема с ISM), в переход FVector на double и в изменения GAS 5.3 [стр. 21, 37; выдача 14, 30, 35, 61].

## Вердикт
В 2026 году 4.27 — разумный, но узкий выбор. Он подходит только для ПК-релиза: Windows/Steam, а Steam Deck и Linux через Proton. Движок нужно собирать из исходников (лучше из ветки 4.27-plus), и команда должна быть готова жить без освещения уровня Lumen и без новых инструментов.

Выбирать 4.27 стоит при таком результате прототипа обрушения: сцена на 50–100 тыс. инстансов, за ход удаляется 2–5 тыс. кусков, затем идут трассы и обломки.
- В 5.8 этот сценарий не укладывается в бюджет даже с выключенной коллизией у ISM, своими коллизионными прокси или запросами по сетке кусков и пакетными обновлениями. Например, рывок больше 100 мс на шаг или больше 1 секунды на крупное обрушение.
- В 4.27 тот же сценарий укладывается.
- Свет 4.27 команда принимает: кэшированные тени, DF-тени и DFAO, SSGI, при желании RTXGI.

Если 5.8 с выключенной коллизией ISM укладывается в бюджет, главный плюс 4.27 пропадает, и тогда выбирать 5.8. Если в планах Switch 2 или будущие консольные SDK, 4.27 без письменного подтверждения Epic под NDA не брать.

## Что не удалось проверить
- Официальные статусы Beta или Experimental у SSGI, Control Rig, Full Body IK и Python в 4.27, а также версию, где LPV объявили устаревшим.
- Можно ли сейчас скачать 4.27 из лаунчера и какова политика Fab по 4.27 (есть только косвенные данные).
- Поддержку Switch 2 и актуальных SDK для PS5 и Xbox Series именно в 4.27 / 4.27-plus, а также реальное состояние ветки 4.27-plus.
- Есть ли ветка Angelscript под 4.27.
- Что Kingmakers сделан именно на 4.27; оригинал интервью Digital Foundry не открылся.
- Движок Miasma Chronicles.
- Различия PhysX и Chaos в трассах, физматериалах и точках выхода; бенчмарки трасс.
- Замеры ISM на 5.8, стоимость перестройки HISM и глобального DF при массовом удалении в 4.27.
- Варианты lip-sync для 4.27 (упёрся в лимит поиска).
- Текст EULA: сайт unrealengine.com не открылся.
- Позицию Epic по Steam Deck для 4.27.
- Число игр, выпущенных именно на 4.27 в 2025–2026 годах.
- Визуальное сравнение 4.27 с Lumen — это только мой вывод.

## Источники
1 https://forums.unrealengine.com/t/what-happened-to-the-instanced-static-mesh-performance-in-ue5/2107140
2 https://forums.unrealengine.com/t/uinstancedstaticmeshcomponent-removeinstance-with-perinstancecustomdata-bug/468861
3 https://forums.unrealengine.com/t/hierarchical-instanced-static-mesh-does-not-support-custom-data-correctly/468750
4 https://docs.unrealengine.com/4.26/en-US/API/Runtime/Engine/Components/UInstancedStaticMeshComponent/PerInstanceSMCus-/index.html
5 https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-4.27-release-notes?application_version=4.27
6 https://forums.unrealengine.com/t/unreal-engine-4-27-chaos-preview-now-available-on-the-epic-games-launcher/248302
7 https://forums.unrealengine.com/t/installing-the-4-27chaos-preview-build-where-did-it-go/648577
8 https://forums.unrealengine.com/t/difference-between-4-27-plus-chaos-branches/739045
9 https://dev.epicgames.com/documentation/en-us/unreal-engine/chaos-destruction-overview?application_version=4.27
10 https://docs.unrealengine.com/4.27/en-US/API/Plugins/ApexDestruction/deprecated/1/index.html
11 https://forums.unrealengine.com/t/can-i-use-apex-destruction-in-ue5/1287502
12 https://dev.epicgames.com/documentation/en-us/unreal-engine/mesh-distance-fields?application_version=4.27
13 https://dev.epicgames.com/documentation/en-us/unreal-engine/distance-field-soft-shadows?application_version=4.27
14 https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine
15 https://forums.unrealengine.com/t/sm-ism-hism-lightweight-instances-and-auto-instancing-what-are-the-differences/609954
16 https://dev.epicgames.com/documentation/en-us/unreal-engine/movable-lights?application_version=4.27
17 https://docs.unrealengine.com/4.26/BuildingWorlds/LightingAndShadows/LightMobility/DynamicLights
18 https://dev.epicgames.com/documentation/en-us/unreal-engine/screen-space-global-illumination?application_version=4.27
20 https://docs.unrealengine.com/4.27/BuildingWorlds/LightingAndShadows/LightPropagationVolumes
21 https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-migration-guide
22 https://architosh.com/2020/05/epic-releases-unreal-engine-version-4-25-brings-production-ready-raytracing-tools
23 https://docs.unrealengine.com/4.27/RenderingAndGraphics/RayTracing
24 https://github.com/NVIDIAGameWorks/RTXGI-DDGI
25 https://www.guru3d.com/news-story/nvidia-releases-rtxgi-for-unreal-engine-marketplace-to-enable-global-illumination-with-rt.html
26 https://forums.unrealengine.com/t/large-performance-regression-in-ue5-cpu-performance/1524868
27 https://forums.unrealengine.com/t/chaos-physics-in-ue5-ea-and-beyond/231748
28 https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/NavigationSystem/ERuntimeGenerationType
29 https://forums.unrealengine.com/t/gameplay-ability-system-turn-based/126620
30 https://gas-reference.pages.dev/gameplay-effects/ge-components/
31 https://docs.unrealengine.com/4.26/API/Editor/UnrealEd/Tools/UEdMode/index.html
32 https://docs.unrealengine.com/4.27/API/Editor/EditorInteractiveToolsFramework/UEdModeInteractiveToolsContext/index.html
33 https://docs.unrealengine.com/4.27/API/Plugins/ModelingToolsEditorMode/FModelingToolsEditorMode/index.html ; https://gamefromscratch.com/unreal-engine-as-a-3d-modeling-sculpting-application/
34 https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/ModelingToolsEditorMode/UModelingToolsEditorMode
35 https://dev.epicgames.com/documentation/en-us/unreal-engine/scriptable-tools-system-in-unreal-engine ; https://developer.aliyun.com/article/1321514
36 https://forums.unrealengine.com/t/4-27-2-hotfix-released/264618
37 https://dev.epicgames.com/documentation/en-us/unreal-engine/level-instancing-in-unreal-engine
38 https://docs.unrealengine.com/4.27/API/Plugins/GameFeatures/UGameFeaturesSubsystem/index.html
39 https://www.unrealengine.com/en-US/blog/new-example-project-and-plugin-for-mod-support-released
40 https://docs.mod.io/unreal
41 https://github.com/Tencent/UnLua
42 https://github.com/Tencent/sluaunreal
43 https://angelscript.hazelight.se/ ; http://angelscript.hazelight.se/project/development-status/
44 https://cdm.link/2021/08/unreal-engine-4-27-out-now-with-goodies-like-web-ui-early-access-ue5-metasounds-makes-noise/
45 https://forums.unrealengine.com/t/metahuman-creator-web-application-is-being-discontinued/2695297
46 https://forums.unrealengine.com/t/feature-request-please-make-a-quixel-bridge-plugin-for-ue-4-27/2732036
47 https://dev.epicgames.com/documentation/en-us/unreal-engine/importing-assets-using-interchange-in-unreal-engine
48 https://www.unrealengine.com/en-US/blog/download-our-new-blender-addons ; https://github.com/poly-hammer/BlenderTools
49 https://www.cgchannel.com/2024/10/epic-games-to-cut-royalty-rate-on-unreal-engine-games/
50 https://forums.unrealengine.com/t/how-to-make-ue-4-27-2-work-with-visual-studio-2026/2727642
51 https://forums.unrealengine.com/t/multiple-internal-compile-errors-while-building-ue-4-27-from-source/1889154
52 https://forums.unrealengine.com/t/unreal-egnine-4-all-of-them-stopped-working-failed-to-launch-editor/2672744 ; https://forums.unrealengine.com/t/after-verifying-ue-4-27-2-files-unable-to-launch-editor-getting-ls-0013-error/2672998
53 https://forums.unrealengine.com/t/upcoming-disruption-of-service-impacting-unreal-engine-users-on-github/1155880
54 https://automaton-media.com/articles/newsjp/20250403-334025/
55 https://forums.unrealengine.com/t/about-nintendo-switch-2-support-in-ue4/2431964
56 https://dev.epicgames.com/documentation/unreal-engine/nintendo-switch-2
57 https://forums.unrealengine.com/t/feature-request-filter-vault-fab-library-by-engine-version/2732035
58 https://forums.unrealengine.com/t/any-plans-to-update-ue4-27-plus-branch-to-support-xcode-16/2037920 ; https://forums.unrealengine.com/t/unreal-4-27-plus-not-working-with-xcode-16-4/2688172 (выдача)
59 https://www.gamingonlinux.com/2023/04/everspace-2-out-now-developer-focus-on-proton-for-linux-steam-deck/
60 https://partner.steamgames.com/doc/steamframe/engines/unreal
61 https://dev.epicgames.com/documentation/en-us/unreal-engine/large-world-coordinates-project-conversion-guidelines-in-unreal-engine-5
62 https://gamedevreports.substack.com/p/video-game-insights-game-engines
63 https://gamingbolt.com/30-new-games-of-2025-with-amazing-graphics/26
64 https://en.wikipedia.org/wiki/Gears_Tactics
65 https://en.wikipedia.org/wiki/Mutant_Year_Zero:_Road_to_Eden
66 https://en.wikipedia.org/wiki/Marvel%27s_Midnight_Suns
67 https://en.wikipedia.org/wiki/Miasma_Chronicles ; https://videogames.si.com/reviews/miasma-chronicles (выдача)
68 https://www.eurogamer.net/digitalfoundry-2024-the-kingmakers-tech-interview ; https://thegameswiki.com/kingmakers/wiki/destructible-environments
69 https://80.lv/articles/fortnite-chapter-4-comes-powered-by-unreal-engine-5-1
70 https://www.pcgamesn.com/deep-rock-galactic/deep-rock-galactic-unreal-engine-4
71 https://forums.unrealengine.com/t/ue-4-27-2-default-physics-engine/2517097