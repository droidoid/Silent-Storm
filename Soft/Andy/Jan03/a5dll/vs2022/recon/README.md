# Материалы разведки (к RECON_REPORT.md)

Скрипты и результаты, по которым написан `../RECON_REPORT.md`. Всё только читает файлы данных; Python 3,
без сторонних пакетов. Пути вида `C:\Users\admin\SS\Silent-Storm`, `C:\a5b\s` (сборка порта) и
`C:\a5b\recon\...` (рабочий каталог) — пути машины, на которой шла разведка; перед запуском их нужно
поправить или передать свои аргументы.

| Каталог | Что внутри |
|---|---|
| `a_gamedb/` | Разбор game.db по `FileIO/BasicChunk1.cpp`: `ss.py` (чтение чанков, таблица объектов), `tree.py` (дерево чанков), `tables.py` / `meta.py` (таблицы `CDBTableDataStorage`, описатели колонок), `schema_diff.py` (сравнение с `REGISTER_DATABASE_CLASS` + `Import()` Jan03). Результаты: `schema_diff.txt`, `release_columns.txt` (все колонки релиза), `rel_tables.txt`. |
| `b_mdf/` | Чтение MDF SQL Server 2000 без сервера: `mdf2000.py` (страницы, sysobjects/syscolumns, строки), `code_fields.py` (поля, которые читает код Jan03), `diff.py`, `cmpcopies.py`, `boot.py`, `dates.py`. Результаты: `mdf_schema_*.txt`, `diff_*.txt`, `code_expected.txt`, `copies_compare.txt`, `dates.txt`. |
| `c_resources/` | Ресурсы Complete/: `ss.py` (повтор чтения Jan03), `check.py` / `scan.py` (проверка схем по всей папке), `packs.py`, `geomvar.py`, `top4.py` и др. Тестовый стенд `stand/` (`stand.cpp` вызывает настоящие загрузчики Jan03; `build.cmd` перекомпилирует исходники Main флагами порта — список даёт `mklist.py`). Результаты: `scan_<тип>.txt`, `check_out.txt`, `stand_*.txt`. |
| `d_exe/` | Game.exe и PDB: `pdb.py` (MSF 7.0: GUID/age, публичные символы, поиск констант в коде), `near.py` (указатели рядом с адресом → символы), `tpi.py` (раскладка классов из TPI), `exeinfo.py` (секции, RTTI, строки). |
| `notes_a_d.md` | Рабочие заметки по пунктам a и d. |
| `gaps/` | Материалы второй части, `../RECON_GAPS.md`: скрипты, интерфейс, ресурсы карты, мультимедиа, дрейф кода (см. `gaps/README.md`). |

Примеры:

```
python a_gamedb/schema_diff.py <a5dll> <Complete/game.db> <out-dir>
python d_exe/pdb.py info Soft/Andy/RussianGold/Game.pdb
python d_exe/tpi.py Soft/Andy/RussianGold/Game.pdb CDBTableDataStorage NDb::CRPGWeapon
```

Не включены: копии игровых файлов, бинарники стенда, полный список публичных символов PDB
(`pdb.py pubs` воспроизводит его за секунды).
