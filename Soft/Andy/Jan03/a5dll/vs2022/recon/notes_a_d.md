# (a) game.db  (scripts: C:\a5b\recon\a\*.py, lists: C:\a5b\recon\a\out\)
- Jan03 format (BasicDB.cpp NDatabase::Serialize): root chunk1 = hash_map<int tableID, CDBTableBase>; table = hash_map<int, CObj<CDBRecord>>;
  each record = separate object (class id = table id 0xE00000xx / small ints), fields as numbered chunks (Class::operator&, e.g. CRPGWeapon 1..39).
- Release Complete/game.db 34.6 MB: top chunks 4(4 bytes, =1, version?),1,0,2; 155 objects, all class 0xA1843130 = CDBTableDataStorage (PDB).
  root: hash_map<int, CObj<CDBTableDataStorage>> + list<SRelation> (RPGPers2Scripts).
  table object chunks: 2 = per record int+bool dwords (count + packed), 3 = floats, 4 = wstrings, 5 = columns (name cp1251, type 0 int/1 bool/2 float/3 string), 6/7/8 = int/float/string column names.
  Column types total: int 1103, bool 127, float 285, string 275. Data/game.db (3.3 MB, 56 tables) and Versions/Current/*/game.db (mods) same format, no chunk 4.
- Schema compare (Jan03 REGISTER_DATABASE_CLASS in DBFormat/DataFormat.cpp + ImportField names in Class::Import()):
  Jan03 130 tables + 1 relation; release 155 tables + 1 relation. All 130 Jan03 IDs present in release; 25 release-only (0x6A..0x7E, 0xE0000025..28).
  115 tables: every field Jan03 Import() reads exists in release columns. 5 tables: 16 names missing (Nationalities 12 Male/FemaleCustomHead1..6 -> heads redesigned, new tables 0x77..0x7A;
  RPGWeapons AmmoTypeID; Panzerkleins PersID; TranslatedStrings String; RPGMineDetectors ItemID). 10 template classes (CTMaterial etc.) not parsed by regex; 8 of them 2 columns in release.
  DamageMоd column keeps the Cyrillic 'о' typo exactly as Jan03 code.
# (d) Game.exe
- bin/Game.exe: 2003-08-06, VC 7.1 linker, monolithic (no Main.dll), RSDS {48FA0D7C-...} age 161, c:\Code\A5\Game\Release\Game.pdb - no matching PDB in repo. Not packed (.text entropy 6.46), no .reloc.
  1759 RTTI names (177 NDb::), .?AVCDBTableDataStorage@@, .?AVIOLETable@@, strings 'File game.db not found', SQL connect string. No __FILE__ .cpp strings.
- PDBs with exact match: Soft/Andy/RussianGold (2003-08-11, age 8), EnglishGold (09-30, age 9), RussianPatch1 (09-30, age 10); also Versions/Current/Game.exe 2003-11-19 (c:\Home\A5) no pdb.
  RussianGold PDB: 53575 publics, full TPI 190508 type records: CDBTableDataStorage layout (size 136), SColumnInfo{szName,eType}, NDb::CRPGWeapon (Jan03 fields - pAmmo + nBPM,nDamageModMax,fSilencer,nAIRating,nShotEffectType).
  Release API: IOLETable iface; CDBTableDataStorage::GetInt/GetBool/GetFloat/GetString/GetWString(name), MoveFirst/Next/IsEof, GetColumns, FindRecord; NDatabase::Import(bool), GetStorageTables, GetOLETable.
  0xA1843130 registered in static init next to NewCDBTableDataStorage (va 0x887A42 in RussianGold exe).
  TFilesPackage::ReadHeader in release accepts both 0x95938921 (Jan03) and 0x96948A22 (release) pack signatures.
