#include <ts/bwprint.h>
#include <ctype.h>
#include <ctime>

ts::detail::BW_GlobalTable ts::detail::BW_FORMAT_GLOBAL_TABLE;
const ts::BW_Spec ts::BW_Spec::DEFAULT;

ts::BW_Spec::BW_Spec(TextView fmt)
{
  TextView num;
  intmax_t n;

  _name = fmt.take_prefix_at(':');
  n     = svtoi(_name, &num, 10);
  if (num)
    _idx = static_cast<decltype(_idx)>(n);

  if (fmt) {
    TextView sz = fmt.take_prefix_at(':');
    _ext        = fmt;
    if (sz) {
      // fill and alignment
      if ('%' == *sz) {
        if (sz.size() < 4)
          throw std::invalid_argument("Fill URI encoding without 2 hex characters and align mark");
        if (Align::NONE == (_align = align_of(sz[3])))
          throw std::invalid_argument("Fill URI without alignment mark");
        char d1 = sz[1], d0 = sz[2];
        if (!isxdigit(d0) || !isxdigit(d1))
          throw std::invalid_argument("URI encoding with non-hex characters");
        _fill = isdigit(d0) ? d0 - '0' : tolower(d0) - 'a' + 10;
        _fill += (isdigit(d1) ? d1 - '0' : tolower(d1) - 'a' + 10) << 4;
        sz += 4;
      } else if (sz.size() > 1 && Align::NONE != (_align = align_of(sz[1]))) {
        _fill = *sz;
        sz += 2;
      } else if (Align::NONE != (_align = align_of(*sz))) {
        ++sz;
      }
      if (!sz)
        return;
      // sign
      if (is_sign(*sz)) {
        _sign = *sz;
        if (!++sz)
          return;
      }
      if ('#' == *sz) {
        _radix_lead_p = true;
        if (!++sz)
          return;
      }
      if ('0' == *sz) {
        if (Align::NONE == _align)
          _align = Align::SIGN;
        _fill    = '0';
        ++sz;
      }
      n = svtoi(sz, &num, 10); // don't get fooled by leading '0'. It's always decimal.
      if (num) {
        _min = static_cast<decltype(_min)>(n);
        sz.remove_prefix(num.size());
        if (!sz)
          return;
      }
      if ('.' == *sz) {
        n = svtoi(++sz, &num);
        if (num) {
          _prec = static_cast<decltype(_prec)>(n);
          sz.remove_prefix(num.size());
          if (!sz)
            return;
        } else {
          throw std::invalid_argument("Precision mark without precision");
        }
      }
      if (is_type(*sz)) {
        _type = *sz;
        if (!++sz)
          return;
      }
      if (',' == *sz) {
        n = svtoi(++sz, &num);
        if (num) {
          _max = static_cast<decltype(_max)>(n);
          sz.remove_prefix(num.size());
          if (!sz)
            return;
        } else {
          throw std::invalid_argument("Maximum width mark without width");
        }
        // Can only have a type indicator here if there was a max width.
        if (is_type(*sz)) {
          _type = *sz;
          if (!++sz)
            return;
        }
      }
    }
  }
}

void
ts::detail::bw_aligner(BW_Spec const &spec, BufferWriter &w, BufferWriter &lw)
{
  size_t size = lw.size();
  size_t min;
  if (size < (min = static_cast<size_t>(spec._min))) {
    size_t delta = min - size; // note - size <= extent -> size < min
    switch (spec._align) {
    case BW_Spec::Align::NONE: // same as LEFT for output.
    // fall through
    case BW_Spec::Align::LEFT:
      w.fill(size);
      while (delta--)
        w.write(spec._fill);
      size = 0; // cancel additional fill.
      break;
    case BW_Spec::Align::RIGHT:
      std::memmove(w.auxBuffer() + delta, w.auxBuffer(), size);
      while (delta--)
        w.write(spec._fill);
      break;
    case BW_Spec::Align::CENTER:
      if (delta > 1) {
        size_t d2 = delta / 2;
        std::memmove(w.auxBuffer() + (delta / 2), w.auxBuffer(), size);
        while (d2--)
          w.write(spec._fill);
      }
      w.fill(size);
      delta = (delta + 1) / 2;
      while (delta--)
        w.write(spec._fill);
      size = 0; // cancel additional fill.
      break;
    case BW_Spec::Align::SIGN:
      break;
    }
  }
  w.fill(size);
}

// Numeric conversions
namespace ts::detail
{
// Conversions from remainder to character, in upper and lower case versions.
namespace
{
  char UPPER_DIGITS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char LOWER_DIGITS[] = "0123456789abcdefghijklmnopqrstuvwxyz";
}
/// Templated radix based conversions. Only a small number of radix are supported
/// and providing a template minimizes cut and paste code while also enabling
/// compiler optimizations (e.g. for power of 2 radix the modulo / divide become
/// bit operations).
template <size_t RADIX>
size_t
BW_to_radix(uintmax_t n, char *buff, size_t width, char *digits)
{
  static_assert(1 < RADIX && RADIX <= 36);
  char *out = buff + width;
  if (n) {
    while (n) {
      *--out = digits[n % RADIX];
      n /= RADIX;
    }
  } else {
    *--out = '0';
  }
  return (buff + width) - out;
}

BufferWriter &
bw_integral_formatter(BufferWriter &w, BW_Spec const &spec, uintmax_t i, bool neg_p)
{
  size_t n  = 0;
  int width = static_cast<int>(spec._min); // amount left to fill.
  string_view prefix;
  string_view neg;
  char buff[std::numeric_limits<uintmax_t>::digits + 1];

  if (neg_p)
    neg = "-"_sv;
  else if (spec._sign != '-')
    neg = string_view{&spec._sign, 1};
  switch (spec._type) {
  case 'x':
    if (spec._radix_lead_p)
      prefix = "0x"_sv;
    n        = detail::BW_to_radix<16>(i, buff, sizeof(buff), detail::LOWER_DIGITS);
    break;
  case 'X':
    if (spec._radix_lead_p)
      prefix = "0X"_sv;
    n        = detail::BW_to_radix<16>(i, buff, sizeof(buff), detail::UPPER_DIGITS);
    break;
  case 'b':
    if (spec._radix_lead_p)
      prefix = "0b"_sv;
    n        = detail::BW_to_radix<2>(i, buff, sizeof(buff), detail::LOWER_DIGITS);
    break;
  case 'B':
    if (spec._radix_lead_p)
      prefix = "0B"_sv;
    n        = detail::BW_to_radix<2>(i, buff, sizeof(buff), detail::UPPER_DIGITS);
    break;
    break;
  case 'o':
    if (spec._radix_lead_p)
      prefix = "0"_sv;
    n        = detail::BW_to_radix<8>(i, buff, sizeof(buff), detail::LOWER_DIGITS);
    break;
  default:
    n = detail::BW_to_radix<10>(i, buff, sizeof(buff), detail::LOWER_DIGITS);
    break;
  }
  // Clip fill width by stuff that's already committed to be written.
  width -= static_cast<int>(neg.size());
  width -= static_cast<int>(prefix.size());
  width -= static_cast<int>(n);
  string_view digits{buff + sizeof(buff) - n, n};

  switch (spec._align) {
  case BW_Spec::Align::LEFT:
    w.write(neg);
    w.write(prefix);
    w.write(digits);
    while (width-- > 0)
      w.write(spec._fill);
    break;
  case BW_Spec::Align::RIGHT:
    while (width-- > 0)
      w.write(spec._fill);
    w.write(neg);
    w.write(prefix);
    w.write(digits);
    break;
  case BW_Spec::Align::CENTER:
    for (int i = width / 2; i > 0; --i)
      w.write(spec._fill);
    w.write(neg);
    w.write(prefix);
    w.write(digits);
    for (int i = (width + 1) / 2; i > 0; --i)
      w.write(spec._fill);
    break;
  case BW_Spec::Align::SIGN:
    w.write(neg);
    w.write(prefix);
    while (width-- > 0)
      w.write(spec._fill);
    w.write(digits);
    break;
  default:
    w.write(neg);
    w.write(prefix);
    w.write(digits);
    break;
  }
  return w;
}

} // ts::detail

ts::BWFormat::BWFormat(ts::TextView fmt)
{
  while (fmt) {
    ts::TextView lit = fmt.take_prefix_at('{');
    if (lit) {
      BW_Spec spec{""};
      spec._ext = lit;
      _items.emplace_back(spec, &LiteralFormatter);
    }
    if (fmt) {
      detail::BW_GlobalSignature gf = nullptr;
      // Need to be careful, because an empty format is OK and it's hard to tell if
      // take_prefix_at failed to find the delimiter or found it as the first byte.
      TextView::size_type off = fmt.find('}');
      if (off == TextView::npos) {
        throw std::invalid_argument("Unclosed {");
      }

      BW_Spec spec{fmt.take_prefix_at(off)};
      if (spec._idx < 0)
        gf = detail::BW_GlobalTableFind(spec._name);
      _items.emplace_back(spec, gf);
    }
  }
}

ts::BWFormat::~BWFormat()
{
}

void
ts::BWFormat::LiteralFormatter(BufferWriter &w, BW_Spec const &spec)
{
  w.write(spec._ext);
}

ts::detail::BW_GlobalSignature
ts::detail::BW_GlobalTableFind(string_view name)
{
  if (name.size()) {
    auto spot = detail::BW_FORMAT_GLOBAL_TABLE.find(name);
    if (spot != detail::BW_FORMAT_GLOBAL_TABLE.end())
      return spot->second;
  }
  return nullptr;
}

namespace
{
void
BW_Formatter_Now(ts::BufferWriter &w, ts::BW_Spec const &spec)
{
  std::time_t t = std::time(nullptr);
  w.fill(std::strftime(w.auxBuffer(), w.remaining(), "%Y%b%d:%H%M%S", std::localtime(&t)));
}

static bool BW_INITIALIZED = []() -> bool {
  ts::detail::BW_FORMAT_GLOBAL_TABLE.emplace("now", &BW_Formatter_Now);
  return true;
}();
}
