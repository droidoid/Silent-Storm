import json, sys, urllib.request, collections, os

BR = [('Mikepicker', 'Silent-Storm', 'macos-port'), ('timskap', 'silent-storm-port', 'android-port'),
      ('sssr33', 'Silent-Storm', 'feature/vs2022_build'), ('sssr33', 'Silent-Storm', 'feature/vs2026_build'),
      ('mrartanis', 'Silent-Storm', 'main'), ('TanghaohanSC', 'Silent-Storm', 'main'), ('cheaterBY', 'Silent-Storm', 'main')]


def get(url):
    req = urllib.request.Request(url, headers={'Accept': 'application/vnd.github+json', 'User-Agent': 'recon-script'})
    with urllib.request.urlopen(req) as r:
        return json.load(r)


os.makedirs('cmp', exist_ok=True)
for owner, repo, br in BR:
    fn = 'cmp/%s_%s.json' % (owner, br.replace('/', '_'))
    if not os.path.exists(fn):
        d = get('https://api.github.com/repos/nival/Silent-Storm/compare/main...%s:%s:%s' % (owner, repo, br))
        json.dump(d, open(fn, 'w'))
    d = json.load(open(fn))
    commits = d.get('commits', [])
    files = d.get('files', [])
    print('=== %s/%s @ %s: status=%s ahead=%s behind=%s commits_listed=%d files_listed=%d' % (
        owner, repo, br, d.get('status'), d.get('ahead_by'), d.get('behind_by'), len(commits), len(files)))
    if commits:
        dates = [c['commit']['author']['date'][:10] for c in commits]
        print('   dates %s .. %s' % (min(dates), max(dates)))
        for c in commits[:6] + ([] if len(commits) <= 12 else [None]) + commits[-6:] if len(commits) > 12 else commits:
            if c is None:
                print('   ...')
                continue
            print('   %s %s' % (c['commit']['author']['date'][:10], c['commit']['message'].splitlines()[0][:110]))
    top = collections.Counter('/'.join(f['filename'].split('/')[:3]) for f in files)
    st = collections.Counter(f['status'] for f in files)
    print('   file status:', dict(st), ' +%d -%d' % (sum(f['additions'] for f in files), sum(f['deletions'] for f in files)))
    for k, v in top.most_common(12):
        print('     %-60s %d' % (k, v))
