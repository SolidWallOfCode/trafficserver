#include <ts/BufferView.h>
#include <ostream>

namespace ts
{

int compare(BufferView const& lhs, BufferView const& rhs)
{
  int zret;
  size_t n;

  // Seems a bit ugly but size comparisons must be done anyway to get the memcmp args.
  if (lhs.size() < rhs.size())
    zret = 1, n = lhs.size();
  else {
    n = rhs.size();
    zret = rhs.size() < lhs.size() ? -1 : 0;
  }

  int r = memcmp(lhs.data(), rhs.data(), n);
  if (0 != r)
    zret = r; // else use size based value.

  return zret;
}

int compare_nocase(BufferView lhs, BufferView rhs)
{
  while (lhs && rhs) {
    char l = ParseRules::ink_tolower(*lhs);
    char r = ParseRules::ink_tolower(*rhs);
    if (l < r) return -1;
    else if (r < l) return 1;
    ++lhs, ++rhs;
  }
  return lhs ? 1 : rhs ? -1 : 0;
}

// Do the template instantions.
template void detail::stream_padding(std::ostream &, std::size_t);
template void detail::aligned_stream_write(std::ostream &, const BufferView &);
template std::ostream &operator<<(std::ostream &, const BufferView &);

}
