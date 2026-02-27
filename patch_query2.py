import re

with open('src/Wt/Dbo/session/Query_impl.h', 'r') as f:
    lines = f.read().splitlines()

in_impl = False
out_lines = []

for i, line in enumerate(lines):
    if line.startswith('template <class Result>') and 'std::vector<FieldInfo> Query<Result>::fields() const' in lines[i+1]:
        out_lines.append('} // namespace Impl')
        out_lines.append('')
    
    if line == '} // namespace Impl':
        # Skip this line if we already closed it! We know it was around 421.
        # We only want to keep the one that we just added (or maybe we already added it so let's just write fresh).
        pass
    else:
        out_lines.append(line)

with open('src/Wt/Dbo/session/Query_impl.h', 'w') as f:
    f.write('\n'.join(out_lines))

