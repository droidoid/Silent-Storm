# Наше ответвление Silent-Storm-Reconstruction

`reconstruction/` — основа работы в этом репозитории: импорт
[met-nikita/Silent-Storm-Reconstruction](https://github.com/met-nikita/Silent-Storm-Reconstruction)
(ветка `develop`) **с полной историей** через `git subtree`. Почему именно он, а не порт Jan03, — в
`Soft/Andy/Jan03/a5dll/vs2022/FORKS_REVIEW.md`: он уже восстановил по PDB и дизассемблированию то, чего в
Jan03 нет (game.db релиза, 215 Lua-функций, интерфейс, ИИ, FMOD/Bink/LifeStudio).

| | |
|---|---|
| Импортирован | 12.11.2026, upstream `6a32950` («Restored retail stability check»), 327 коммитов |
| Лицензия | у upstream своего файла лицензии нет; исходники Nival — `../LICENSE.md` (некоммерческая). Вопрос отложен |
| Наши изменения | всё после коммита импорта; в коде отличия от upstream помечены `[FORK]` |

## Что мы изменили

| Что | Где |
|---|---|
| Оконный режим: обычное окно с заголовком и рамкой, не сворачивается при потере фокуса и продолжает рисовать | `Main/Gfx.cpp`, `Game/WinFrame.cpp`, `Game/Main.cpp` |
| Игра не отнимает мышь: без загруженных привязок и в оконном режиме указатель не удерживается и не ограничивается | `Main/Cursor.cpp`, `Input/Bind.cpp` |
| Снимок кадра для фона диалогов берётся из положения окна, а не из угла экрана | `Main/Gfx.cpp` |
| `tools/make_run_dir.cmd` — каталог запуска из данных этого репозитория | `tools/` |
| `tools/run_game.ps1` — запуск игры со сторожем, который завершает её по истечении лимита | `tools/` |
| `tools/mdf2sqlite.py` — база редактора (MDF SQL Server 2000) в SQLite без SQL Server | `tools/` |

## Сборка

Windows, Visual Studio 2022 (или Build Tools) с C++ x86 и Windows SDK, CMake 3.21+. Только Win32.
Каталог сборки — вне репозитория:

```
cmake -S reconstruction -B C:\build\ss -G "Visual Studio 17 2022" -A Win32
cmake --build C:\build\ss --config Release -- /m
```

Результат: `C:\build\ss\Release\Game.exe` и инструменты (`PkgBuilder`, `DataImport`, `TexConv`, …).
Чистая сборка с Build Tools 2022 (MSVC 14.44) проходит без ошибок, около 12 минут: `Main` собирается без `/MP`.
`ShaderCompiler` пропускается, если нет DirectX SDK; `MapEdit` не собирается вовсе (см. ниже).

## Запуск на данных этого репозитория

Upstream рассчитан на папку Steam-версии v1.2. На наших `Complete/` + `bin/` игру запускает каталог,
который собирает `tools/make_run_dir.cmd`:

```
reconstruction\tools\make_run_dir.cmd C:\run\ss C:\build\ss\Release
powershell -NoProfile -Command "& 'reconstruction\tools\run_game.ps1' -RunDir C:\run\ss -Minutes 10"
```

Что кладёт скрипт (всё копируется, в `Complete/` игра писать не может):

- `Game.exe`, `zlib.dll` из сборки; `fmod`, `binkw32`, `LifeStudioHeadAPI`, `GDPFile`, `IMorphFile`, `TriangLib` из `bin/`;
- `game.db`, `tree.mma` (без него миссия падает при загрузке), `scripts\*.l` (общие библиотеки Lua; без них
  скрипты миссий не работают), `cfg\*.cfg`;
- свой `cfg\autoexec.cfg` с относительными путями: в `Complete\cfg` он ссылается на `c:\a5\cfg\input.cfg`,
  с ним управление не загружается;
- 23 пакета `res\<Тип>.res`, упакованные собранным `PkgBuilder`; готовые пакеты при повторе не пересобираются;
- `start.cfg` с командой `mainmenu`.

Каталог занимает около 2 ГБ.

**Запускать только через `run_game.ps1`.** Игра выходит на передний план живого рабочего стола; сторож
завершает её по лимиту (по умолчанию 10 минут), даже если она зависла.

- Через удалённый рабочий стол полноэкранный режим не включается («Failed to set display mode»); оконный режим
  задан в `cfg\config.cfg` (`gfx_fullscreen = 0`), размер — ключом `-800`, `-1024`, … или `gfx_resolution`.
- Выход с клавиатуры по `cfg\input.cfg`: `Ctrl+Q` (привязка `exitgame`; в работе не проверялась).

### Управление игрой без рук (`-harness`)

С ключом `-harness` игра раз в кадр читает одну строку из `_harness_cmd.txt` в каталоге запуска и пишет
вывод консоли в `_console.log`, а служебные сообщения и трассу падения — в `_saveload.log`:

- `console <команда>` — консольная команда, например `console template 3589 730 with Day` (учебная миссия,
  то же, что кнопка Tutorial);
- `quit` — штатный выход.

## Что проверено (12.11.2026)

- Главное меню с трёхмерной сценой.
- Учебная миссия загружается, сценарий стартует (первая подсказка обучения), ошибок Lua нет.
- Окно с рамкой: мышь остаётся свободной (проверено пользователем).

Не проверено: прохождение миссии дальше первой подсказки, другие миссии и кампания, перетаскивание окна,
вращение камеры в окне, полноэкранный режим, сохранения.

Замечено в учебной миссии и не разобрано: шахматная заглушка вместо одной текстуры земли; предупреждение
«Template #4359 was not marked as grid-creating»; предупреждения Lua о чтении пустых глобальных `false`,
`OnEnterZone`, `CameraLockRemoved` (ошибок вызова за ними нет).

Известные расхождения данных: 65 файлов Terrain и 7 Chapters в новой раскладке upstream не читает
(см. `FORKS_REVIEW.md`).

## Редактор карт

`MapEdit` лежит в исходниках (360 файлов, потомок Jan03), но не собирается: нужны MFC и Stingray Objective
Toolkit, код старше релизного exe. Готовый `Versions/Current/MapEdit.exe` (ноябрь 2003) требует службу
SQL Server `MSSQLSERVER` с базой `A5GAME` формата SQL Server 2000. Установка SQL Server отложена.

Данные редактора доступны без сервера: `tools/mdf2sqlite.py` переносит `A5GAME_Data.MDF` в SQLite
(283 таблицы, 268 540 строк; против `Complete/game.db` расходятся 49 значений из 4,9 млн).

```
python -I reconstruction\tools\mdf2sqlite.py Versions\Current\res\A5GAME_Data.MDF C:\run\ss-db\A5GAME.sqlite
```

## Обновление из upstream

```
git subtree pull --prefix=reconstruction https://github.com/met-nikita/Silent-Storm-Reconstruction develop
```

Конфликты возможны только в файлах, которые мы сами меняли (помечены `[FORK]`).
