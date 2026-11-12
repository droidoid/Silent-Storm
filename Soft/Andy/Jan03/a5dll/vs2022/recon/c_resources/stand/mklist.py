"""Turn the port's Main object list (C:/a5b/s/Main_objs.rsp) into the matching source list main_src.rsp."""
import re, os
here = os.path.dirname(os.path.abspath(__file__))
src = []
for l in open(r'C:\a5b\s\Main_objs.rsp').read().split():
    l = l.replace('/', os.sep)
    m = re.search(r'Main\.dir\\\.\\C_\\(.*)\.obj$', l)
    if m:
        src.append('C:\\' + m.group(1))
    elif 'LifeStudioNull' in l:
        src.append(r'C:\Users\admin\SS\Silent-Storm\Soft\Andy\Jan03\a5dll\vs2022\stubs\lifestudio\LifeStudioNull.cpp')
    else:
        print('??', l)
missing = [s for s in src if not os.path.exists(s)]
open(os.path.join(here, 'main_src.rsp'), 'w').write('\n'.join('"%s"' % s for s in src) + '\n')
print(len(src), 'sources;', len(missing), 'missing', missing[:3])
