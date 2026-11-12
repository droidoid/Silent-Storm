"""Group commit messages by topic (multi-label keyword rules). usage: python commit_themes.py <repo> <out>"""
import subprocess, re, sys, collections
repo, out = sys.argv[1], sys.argv[2]
raw = subprocess.run(['git', '-C', repo, 'log', '--reverse', '--format=%h%x1f%ad%x1f%B%x1e', '--date=short'], capture_output=True, text=True, encoding='utf-8', errors='replace').stdout
commits = [c.strip().split('\x1f') for c in raw.split('\x1e') if c.strip()]
TOP = [
 ('merge/README/CI', r'^merge |readme|screenshot|\bci\b|workflow'),
 ('build/toolchain', r'\bbuild\b|cmake|compile|linker|\blink\b|warning|vs ?20|\bshim\b|\.def\b|implib|x64|crash on start'),
 ('DB/data format', r'game\.db|\bdb\b|database|columnar|import\(|dataimport|\btables?\b|\brecords?\b'),
 ('save/load', r'\bsave|\bload(ing)?\b|quicksave|autosave|serializ'),
 ('AI', r'\bai\b|enem|tactical|guard|patrol|retreat|panic|commander|threat|logic|npc'),
 ('combat/RPG', r'shoot|shot|\bfir(e|ing)\b|weapon|grenade|melee|to-?hit|tohit|damage|critical|burst|ammo|reload|knife|\bmines?\b|perk|\bxp\b|skill|level|heal|bleed|armor|\bpk\b|panzerklein|attack|interrupt|hide|sniper|cover|ricochet'),
 ('world/movement/physics', r'ladder|stair|path|movement|\bmov(e|ing|ement)\b|door|passab|collapse|stability|debris|physic|corpse|carry|body|jump|waypoint|zone|turn-based|turn based|realtime|real-time|\btb\b'),
 ('UI/menus/HUD', r'\bui\b|menu|button|cursor|journal|hint|inventory|dialog|panel|\bscreens?\b|portrait|tooltip|caption|\bicons?\b|widescreen|resolution|\boptions?\b|\bstore\b|medal|subtitle|console|font|\btext\b'),
 ('scripts/Lua', r'\blua\b|script|scenario|objective|goal|callback|trigger'),
 ('rendering/effects', r'render|shadow|light|effect|rain|snow|weather|particle|texture|terrain|grass|decal|stencil|\baa\b|anti-?alias|shader|camera|fade|anim|occlu|fog|vertex|raster|tint|colou?r'),
 ('audio/video', r'sound|audio|music|fmod|voice|bink|video|ambient'),
 ('campaign/globalmap/hotseat', r'hotseat|multiplayer|global[- ]map|global game|chapter|campaign|recruit|difficulty|globe'),
 ('mods', r'\bmod(s|ding)?\b|mod manager'),
]
lab = collections.Counter(); per = collections.defaultdict(list)
restore = fix = 0; rva_msgs = 0
for h, d, b in commits:
    s = b.lower()
    hit = [n for n, rx in TOP if re.search(rx, s)]
    if not hit: hit = ['misc/unclassified']
    for n in hit: lab[n] += 1; per[n].append('%s %s %s' % (h, d, b.splitlines()[0][:110]))
    if re.search(r'restor|retail|parity|converge', s): restore += 1
    if re.search(r'\bfix', s): fix += 1
    if re.search(r'@0x[0-9a-f]{4,}|0x[0-9a-f]{5,}|\brva\b', s): rva_msgs += 1
months = collections.Counter(d[:7] for h, d, b in commits)
L = ['commits: %d (%s .. %s)' % (len(commits), commits[0][1], commits[-1][1]),
     'by month: ' + ', '.join('%s:%d' % kv for kv in sorted(months.items())),
     'messages mentioning restore/retail/parity/converge: %d; mentioning fix: %d; citing addresses/RVA: %d' % (restore, fix, rva_msgs),
     '', 'topic counts (multi-label keyword rules):']
for n, _ in TOP + [('misc/unclassified', '')]:
    L.append('  %-24s %d' % (n, lab[n]))
for n, _ in TOP + [('misc/unclassified', '')]:
    L.append('\n== %s (%d)' % (n, lab[n]))
    L += ['  ' + x for x in per[n]]
open(out, 'w', encoding='utf-8').write('\n'.join(L) + '\n')
print('\n'.join(L[:20]))
