#include <ts/BufferFormat.h>
#include <ctype.h>

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
        _base = 1;
        if (!++sz)
          return;
      }
      if ('0' == *sz) {
        if (Align::NONE == _align) {
          _align = Align::SIGN;
          _fill  = '0';
        }
        ++sz;
        _min = 0;
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
      }
    }
  }
}

void
ts::detail::bw_aligner(BW_Spec const &spec, BufferWriter &w, BufferWriter &lw)
{
  size_t size = lw.size();
  size_t min;
  if (spec._min >= 0 && size < (min = static_cast<size_t>(spec._min))) {
    size_t delta = min - size; // note - size <= extent -> size < min
    switch (spec._align) {
    case BW_Spec::Align::NONE:
      break;
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

BW_::BW_(ts::TextView fmt)
{
  while (fmt) {
    ts::TextView lit = fmt.take_prefix_at('{');
    if (lit) {
      items.emplace_back(lit);
    }
    if (fmt) {
      ts::TextView spec = fmt.take_prefix_at('}');
      if (!spec) {
        throw std::invalid_argument("Unclosed {");
      }
      items.emplace_back(ts::BW_Spec(spec));
    }
  }
}

BW_::~BW_()
{
}

void
test(ts::BufferWriter &w)
{
  bwprint(w, "This {} arg", 27);
}
