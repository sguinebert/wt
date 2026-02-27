import re

# 1. Update Query.h
with open('src/Wt/Dbo/session/Query.h', 'r') as f:
    h = f.read()

h = h.replace('SelectFieldList', 'Impl::SelectFieldList')
h = h.replace('SelectFieldLists', 'Impl::SelectFieldLists')
h = h.replace('Impl::Impl::', 'Impl::') # in case of double replace

with open('src/Wt/Dbo/session/Query.h', 'w') as f:
    f.write(h)

# 2. Update Session.h
with open('src/Wt/Dbo/session/Session.h', 'r') as f:
    sh = f.read()

sh = re.sub(r'\s*template <class C> friend class Impl::QueryBase;', '', sh)

with open('src/Wt/Dbo/session/Session.h', 'w') as f:
    f.write(sh)

# 3. Update Query_impl.h namespaces
with open('src/Wt/Dbo/session/Query_impl.h', 'r') as f:
    impl = f.read()

# We need to move the 'Query<Result>::' definitions outside of 'namespace Impl {'
# 'namespace Impl {' ends at 421. Actually it's easier to just insert '} // namespace Impl' before the Query methods
# Let's find first 'template <class Result>\nstd::vector<FieldInfo> Query<Result>::fields() const'

first_method = r'template <class Result>\nstd::vector<FieldInfo> Query<Result>::fields\(\) const'

impl = re.sub(first_method, '} // namespace Impl\n\n' + first_method.replace('\\(', '(').replace('\\)', ')'), impl)

# And we have an extra '} // namespace Impl' at 421 that we need to remove.
# We can find instances of '} // namespace Impl' and ensure only the first one is used, or just replace the last one.
# Let's find exactly the spot.

parts = impl.split('} // namespace Impl')
# parts[0] has 'namespace Impl {'
# We just moved '} // namespace Impl' before the fields() method, so now we have two such tokens.
# Let's write Python to do it properly.
