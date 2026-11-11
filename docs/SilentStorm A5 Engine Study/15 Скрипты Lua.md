---
tags: [silent-storm, a5-engine, скрипты, lua]
---

# Скрипты Lua

← [[00 Главная]]

## Виртуальная машина

- `Script/` — Lua 4.0, исходники переведены в `.cpp`, с обёрткой `Script` (`Script/Script.h/.cpp`).
- VM доработана: хранит список потоков (`m_state->threads`).
- `Script/lsaver.cpp` делает состояние Lua целиком сериализуемым — поэтому сохранение возможно посреди скрипта (см. [[14 Сериализация]]).
- `NScript::CScript` (`Main/A5Script.h`) — один на мир, сохраняется вместе с ним.
- Потоки исполняются в конце каждого сегмента мира: `ExecuteThreads()` — см. [[01 Архитектура]].

## Привязки

Таблица `pRegList` в `Main/ScriptFunctions.cpp`, реализации в `Main/script*.cpp`.

| Категория | Примеры |
|---|---|
| Общие | `out`, `Sleep`, `StartThread`, `random` |
| Указатели | `Ptr`, `IsValid` |
| Дипломатия | `SetDiplomacy` |
| Камера | `CameraMove` |
| Последовательности | `c_BeginSequence` |
| Юниты (около 40) | `UnitMoveToWaypoint`, `UnitShoot`, `UnitAI` |
| Диалоги | `DialogPlay` |
| Объекты | `ObjectOpen` |
| Сценарий | `ScenarioGiveClue`, `ExitToChapter` |
| UI | `uiShowStore` |
| Шаблоны | `PlaceTemplate` |
| Расстояния | `GetDistance` |

Также есть категории групп, маршрутов и предметов.

## Откуда берутся скрипты

- Файлы из таблицы `CDBAutoLoadScript` запускает `CWorld::RunAutoLoadScripts`.
- Код, хранящийся в базе, — `RunScriptByID`.
- C++ вызывает Lua через `luaCallFunction` (`Main/scriptCommon.cpp`): `OnOpenObject`, `OnMineTriggered`, `OnTalk`.

## Что на Lua, что в C++

- **Lua:** триггеры миссий, катсцены, сценарные события. Примеры в `Complete/Scripts/`: `TriggersManager.l` опрашивает триггеры через `Sleep` / `StartThread`, `Common.l` — помощники вроде `WaitForUnit`.
- **C++:** ИИ ([[10 ИИ и поиск пути]]), бой и RPG-правила, состояния UI, разрушаемость ([[06 Разрушаемость]]), блок-схемы сценария (`Main/scFlowChart.cpp`).

Редактор скриптов встроен в [[16 Редактор карт]].
