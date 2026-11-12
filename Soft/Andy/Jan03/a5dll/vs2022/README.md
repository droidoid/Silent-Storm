# Silent Storm (A5, снапшот Jan03) — сборка в Visual Studio 2022

Здесь всё, что добавлено для сборки исходников `Soft/Andy/Jan03/a5dll` современным компилятором.
Исходные `.sln/.vcproj` не изменены; списки файлов CMake читает прямо из них.

## Что собирается

| Компонент | Состояние |
|---|---|
| `Game.exe` | собирается, запускается (см. «Запуск») |
| `Main.dll` (движок, 269 файлов) | собирается |
| `Misc`, `FileIO`, `MiscDll`, `Input`, `Script` (Lua 4.0), `Image`, `zlib`, `libpng` | собираются |
| `DBFormat`, `ADOImport`, `ADOFake` | собираются |
| `FModSound` | собирается с заглушкой «без звука» |
| `MemoryMngr`, `MemoryMngrDll` | собираются, по умолчанию не подключаются |
| Инструменты `DataImport`, `PkgBuilder`, `TexConv`, `TexMipStrip`, `FontGen` | собираются и работают (TexConv — без DXT) |
| `MapEdit`, `Sound` | **не собираются**: нужны MFC и коммерческая Stingray Objective Toolkit |
| `A5ExportModel` | не собирается: нужен Maya SDK 4.0 |
| `LSConverter` | не собирается: нужен LifeStudio:HEAD SDK |
| `ShaderCompiler` | не включён: нужен D3DX; готовый `Main/GfxShaders.cpp` уже лежит в исходниках |
| `GfxTest`, `OpenDynamix`, `Scintilla`, `AItest`, `AIPathTest` | не включены (стенды и библиотеки, не нужные игре) |

## Требования

- Windows 10/11 или Server, x64.
- Visual Studio 2022 или Build Tools 2022 с компонентами «MSVC x64/x86», «C++ CMake tools for Windows»
  и Windows SDK 10. CMake и Ninja берутся из той же установки Visual Studio, отдельно ставить не нужно.
- Python 3 в `PATH` (генерация списка экспорта `Main.dll`).
- DirectX SDK, STLport, FMOD, Bink **не нужны**.

## Сборка

```
git clone --depth 1 -c core.longpaths=true https://github.com/droidoid/Silent-Storm C:\ss
cd C:\ss
Soft\Andy\Jan03\a5dll\vs2022\build.cmd C:\a5build
```

- Вся сборка идёт инструментами **одной** установки Visual Studio: `vcvarsall`, `cl`, `link`, `cmake`, `ninja`
  (CMake из `PATH` не используется). По умолчанию берётся новейшая VS 2022 с C++ x86/x64, выбор печатается
  строками `[build] ...`. Переопределение: `set A5_VSDIR=<путь установки>`, `set A5_CMAKE=<cmake.exe>`
  (не старше 3.24). Если каталог сборки настроен другим компилятором или CMake, build.cmd сам
  перенастраивает его с нуля (`cmake --fresh`).
- Списки экспорта всех DLL пишет `tools/gen_def.py` (по объектным файлам, без `cmake -E __create_def`),
  поэтому результат не зависит от версии CMake.
- Первый аргумент — каталог сборки (вне репозитория). Держите пути короткими: у MSVC предел 260 символов.
  Если репозиторий лежит глубоко, подставьте диск: `subst S: C:\...\Soft\Andy\Jan03\a5dll` и запускайте
  `S:\vs2022\build.cmd C:\a5build`.
- Второй аргумент — конфигурация (`RelWithDebInfo` по умолчанию, `Release`, `Debug`); далее — цели.
- Результат: `C:\a5build\bin\` — `Game.exe`, `Main.dll`, остальные DLL, инструменты и `.pdb`.
- Чистая сборка на 16 ядрах занимает около полутора минут.

Параметры CMake (передавать через `cmake -D... C:\a5build`, затем снова `build.cmd`):

| Параметр | По умолчанию | Назначение |
|---|---|---|
| `A5_NULL_SOUND` | ON | заглушка FMOD; OFF требует `A5_FMOD_INCLUDE_DIR`, `A5_FMOD_LIBRARY` |
| `A5_NULL_LIFESTUDIO` | ON | заглушка LifeStudio (лица не отображаются) |
| `A5_STUB_S3TC` | ON | без сжатия DXT; OFF требует `A5_S3TC_INCLUDE_DIR`, `A5_S3TC_LIBRARY` |
| `A5_USE_MEMORY_MANAGER` | OFF | исходный пул-аллокатор вместо кучи CRT (не проверялся) |
| `A5_DEV_PARTS` | ON | вспомогательные цели `Main_part_*` для компиляции движка по частям |

## Запуск

Игра работает из каталога данных: ей нужны `game.db`, `cfg\`, папки ресурсов и `start.cfg`.
Скрипт готовит такой каталог, не изменяя оригинальные данные (ресурсы подключаются ссылками):

```
Soft\Andy\Jan03\a5dll\vs2022\tools\make_run_dir.cmd C:\a5run C:\ss\Complete C:\a5build\bin
cd C:\a5run
Game.exe
```

> **Ограничение.** Все `game.db` в репозитории записаны более поздней версией движка (таблицы хранятся
> как объекты класса `0xA1843130`, которого нет в исходниках января 2003). Игра читает такой файл без
> ошибок, но все таблицы остаются пустыми, поэтому сборка показывает окно «A5: incompatible game.db» и
> завершает работу. Дальше экрана загрузки базы игра с имеющимися данными не идёт.
>
> Что работает до этого места: загрузка всех DLL, чтение `game.db`, инициализация Direct3D 9, ввода и
> звука-заглушки, исполнение `cfg\autoexec.cfg`, создание интерфейса.
>
> Чтобы пойти дальше, нужна `game.db` в формате января 2003. Её делает собранный `DataImport.exe` из базы
> контента: `Data\A5GAME_Data.MDF` (MS SQL Server 2000). Нужен установленный SQL Server с подключённой
> базой; `Data\game.mdb` не подходит — это оболочка Access со ссылками на тот же сервер.
> `DataImport.exe -dbserver <сервер> C:\a5run\game.db` (или `-connect "<строка ADO>"`).
> Совпадёт ли схема этой базы с кодом января 2003 — неизвестно.

Ключи командной строки (из `Game/Main.cpp`): `-nosound`, `-fullscreen`, `-640` … `-1600`, `-noai`,
`-load`, `-cfg <файл>` (по умолчанию `start.cfg` — консольные команды, запускающие игру).

## Диагностика

- `tools\build_a5dbg.cmd <каталог>` собирает `a5dbg.exe` — запускатель, который печатает
  `OutputDebugString` и стек с именами функций при падении (отладчика в Build Tools нет):
  `a5dbg.exe -t 30 Game.exe`.
- `tools\chunks.py <файл>` показывает блочную структуру `game.db`, ресурсов и сохранений.
- `tools\b.py <цель>` — сборка с кратким отчётом об ошибках; `tools\ed.py` — правка файлов в cp1251.

## Документы

- `BUILD_LOG.md` — журнал: окружение, решения, изменения поведения, ход работ.
- `PORTING.md` — правила, по которым правился код, и типовые замены STLport → MSVC STL.
