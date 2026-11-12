# Наше ответвление Silent-Storm-Reconstruction

`reconstruction/` — основа дальнейшей работы в этом репозитории: импорт
[met-nikita/Silent-Storm-Reconstruction](https://github.com/met-nikita/Silent-Storm-Reconstruction)
(ветка `develop`) **с полной историей** через `git subtree`. Почему именно он, а не порт Jan03, — в
`Soft/Andy/Jan03/a5dll/vs2022/FORKS_REVIEW.md`: он уже восстановил по PDB и дизассемблированию то, чего в
Jan03 нет (game.db релиза, 215 Lua-функций, интерфейс, ИИ, FMOD/Bink/LifeStudio).

| | |
|---|---|
| Импортирован | 12.11.2026, upstream `6a32950` («Restored retail stability check»), 327 коммитов |
| Лицензия | у upstream своего файла лицензии нет; исходники Nival — `../LICENSE.md` (некоммерческая). Вопрос отложен |
| Наши изменения | всё, что меняется в `reconstruction/` после коммита импорта; upstream-файлы правим свободно |

## Сборка

Windows, Visual Studio 2022 (или Build Tools) с C++ x86 и Windows SDK, CMake 3.21+. Только Win32.
Каталог сборки — вне репозитория:

```
cmake -S reconstruction -B C:\build\ss -G "Visual Studio 17 2022" -A Win32
cmake --build C:\build\ss --config Release -- /m
```

Результат: `C:\build\ss\Release\Game.exe` и инструменты (`PkgBuilder`, `DataImport`, `TexConv`, …).
Чистая сборка с Build Tools 2022 (MSVC 14.44) проходит без ошибок, около 12 минут: `Main` собирается без `/MP`.

## Запуск на данных этого репозитория

Upstream рассчитан на папку Steam-версии v1.2. На наших `Complete/` + `bin/` игру запускает
`tools/make_run_dir.cmd`: копирует exe и нужные DLL, `game.db`, `cfg`, пакует папки ресурсов `Complete/`
в `res\<Тип>.res` собранным `PkgBuilder` (как `Complete/makeCurrent.bat`) и пишет `start.cfg` с `mainmenu`.

```
reconstruction\tools\make_run_dir.cmd C:\run\ss C:\build\ss\Release
cd /d C:\run\ss
Game.exe -windowed -800
```

- Каталог запуска ~2 ГБ (пакеты ресурсов); повторный запуск скрипта оставляет готовые пакеты.
- Через удалённый рабочий стол полноэкранный режим не включается («Failed to set display mode»), отсюда `-windowed`.
- Проверено: главное меню с трёхмерной сценой. Миссии на наших данных ещё не проверялись.
- Известные расхождения данных: 65 файлов Terrain и 7 Chapters в новой раскладке upstream не читает
  (см. `FORKS_REVIEW.md`).

## Обновление из upstream

```
git subtree pull --prefix=reconstruction https://github.com/met-nikita/Silent-Storm-Reconstruction develop
```

Конфликты возможны только в файлах, которые мы сами меняли.
