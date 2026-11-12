# Материалы к RECON_GAPS.md

Пути в скриптах — пути машины разведки (`C:\Users\admin\SS\Silent-Storm`, `C:\a5b\...`), как и в остальной `recon/`.

| Каталог | Что внутри |
|---|---|
| `scripts_ui/` | `rows.py` (декодер строк таблиц `CDBTableDataStorage`), `dump_all.py` (выгрузка всех таблиц в TSV — сами TSV не включены, 24 МБ), `reglist.py` (таблица `pRegList` Jan03 и релиза по адресу из PDB), `lua_calls.py`, `arity.py`, `breakage.py`, `ui_check.py`, `cfg_check.py`. Результаты: `reglist_diff.txt`, `script_calls.tsv`, `script_breakage.tsv`, `ui_check.txt`, `cfg_check.txt` и др. |
| `maps/` | Проверка остальных типов ресурсов карты: `gaps.py` (статическая), результаты прогона стенда с настоящими загрузчиками Jan03 `stand_<тип>.txt` (сам стенд удалён вместе с портом), `gaps_out.txt`, покрытие записей базы `cover_out.txt`, пакеты `packx_out.txt`, поиск классов по PDB `pdbfind_out.txt`. |
| `media/` | FMOD, LifeStudio, Bink: экспорты и импорты DLL/exe, пробная линковка Jan03 FModSound с `fmod.dll` 3.70 (`fmod_linktest.txt`), таблицы виртуальных функций LifeStudio по PDB, форматы звуков (`media_formats.txt`). |
| `drift/` | `drift.py` — сравнение функций Jan03 и релиза по PDB; `drift.txt`, полные списки `release_only_functions.txt`, `jan03_only_functions.txt`. |
