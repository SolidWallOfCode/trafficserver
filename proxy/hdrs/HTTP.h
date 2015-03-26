/** @file

    A brief file description

    @section license License

    Licensed to the Apache Software Foundation (ASF) under one
    or more contributor license agreements.  See the NOTICE file
    distributed with this work for additional information
    regarding copyright ownership.  The ASF licenses this file
    to you under the Apache License, Version 2.0 (the
    "License"); you may not use this file except in compliance
    with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#ifndef __HTTP_H__
#define __HTTP_H__

#include <assert.h>
#include <vector>
#include "ts/Arena.h"
#include "ts/INK_MD5.h"
#include "MIME.h"
#include "URL.h"

#include "ts/ink_apidefs.h"

#define HTTP_VERSION(a, b) ((((a)&0xFFFF) << 16) | ((b)&0xFFFF))
#define HTTP_MINOR(v) ((v)&0xFFFF)
#define HTTP_MAJOR(v) (((v) >> 16) & 0xFFFF)

class Http2HeaderTable;
class MIOBuffer;
namespace ts
{
struct ConstBuffer;
}

enum HTTPStatus {
  HTTP_STATUS_NONE = 0,

  HTTP_STATUS_CONTINUE           = 100,
  HTTP_STATUS_SWITCHING_PROTOCOL = 101,

  HTTP_STATUS_OK                            = 200,
  HTTP_STATUS_CREATED                       = 201,
  HTTP_STATUS_ACCEPTED                      = 202,
  HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
  HTTP_STATUS_NO_CONTENT                    = 204,
  HTTP_STATUS_RESET_CONTENT                 = 205,
  HTTP_STATUS_PARTIAL_CONTENT               = 206,

  HTTP_STATUS_MULTIPLE_CHOICES   = 300,
  HTTP_STATUS_MOVED_PERMANENTLY  = 301,
  HTTP_STATUS_MOVED_TEMPORARILY  = 302,
  HTTP_STATUS_SEE_OTHER          = 303,
  HTTP_STATUS_NOT_MODIFIED       = 304,
  HTTP_STATUS_USE_PROXY          = 305,
  HTTP_STATUS_TEMPORARY_REDIRECT = 307,

  HTTP_STATUS_BAD_REQUEST                   = 400,
  HTTP_STATUS_UNAUTHORIZED                  = 401,
  HTTP_STATUS_PAYMENT_REQUIRED              = 402,
  HTTP_STATUS_FORBIDDEN                     = 403,
  HTTP_STATUS_NOT_FOUND                     = 404,
  HTTP_STATUS_METHOD_NOT_ALLOWED            = 405,
  HTTP_STATUS_NOT_ACCEPTABLE                = 406,
  HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
  HTTP_STATUS_REQUEST_TIMEOUT               = 408,
  HTTP_STATUS_CONFLICT                      = 409,
  HTTP_STATUS_GONE                          = 410,
  HTTP_STATUS_LENGTH_REQUIRED               = 411,
  HTTP_STATUS_PRECONDITION_FAILED           = 412,
  HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE      = 413,
  HTTP_STATUS_REQUEST_URI_TOO_LONG          = 414,
  HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE        = 415,
  HTTP_STATUS_RANGE_NOT_SATISFIABLE         = 416,

  HTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
  HTTP_STATUS_NOT_IMPLEMENTED       = 501,
  HTTP_STATUS_BAD_GATEWAY           = 502,
  HTTP_STATUS_SERVICE_UNAVAILABLE   = 503,
  HTTP_STATUS_GATEWAY_TIMEOUT       = 504,
  HTTP_STATUS_HTTPVER_NOT_SUPPORTED = 505
};

enum HTTPKeepAlive {
  HTTP_KEEPALIVE_UNDEFINED = 0,
  HTTP_NO_KEEPALIVE,
  HTTP_KEEPALIVE,
};

enum HTTPWarningCode {
  HTTP_WARNING_CODE_NONE = 0,

  HTTP_WARNING_CODE_RESPONSE_STALE         = 110,
  HTTP_WARNING_CODE_REVALIDATION_FAILED    = 111,
  HTTP_WARNING_CODE_DISCONNECTED_OPERATION = 112,
  HTTP_WARNING_CODE_HERUISTIC_EXPIRATION   = 113,
  HTTP_WARNING_CODE_TRANSFORMATION_APPLIED = 114,
  HTTP_WARNING_CODE_MISC_WARNING           = 199
};

/* squild log codes */
enum SquidLogCode {
  SQUID_LOG_EMPTY                     = '0',
  SQUID_LOG_TCP_HIT                   = '1',
  SQUID_LOG_TCP_DISK_HIT              = '2',
  SQUID_LOG_TCP_MEM_HIT               = '.', // Don't want to change others codes
  SQUID_LOG_TCP_MISS                  = '3',
  SQUID_LOG_TCP_EXPIRED_MISS          = '4',
  SQUID_LOG_TCP_REFRESH_HIT           = '5',
  SQUID_LOG_TCP_REF_FAIL_HIT          = '6',
  SQUID_LOG_TCP_REFRESH_MISS          = '7',
  SQUID_LOG_TCP_CLIENT_REFRESH        = '8',
  SQUID_LOG_TCP_IMS_HIT               = '9',
  SQUID_LOG_TCP_IMS_MISS              = 'a',
  SQUID_LOG_TCP_SWAPFAIL              = 'b',
  SQUID_LOG_TCP_DENIED                = 'c',
  SQUID_LOG_TCP_WEBFETCH_MISS         = 'd',
  SQUID_LOG_TCP_FUTURE_2              = 'f',
  SQUID_LOG_TCP_HIT_REDIRECT          = '[', // standard redirect
  SQUID_LOG_TCP_MISS_REDIRECT         = ']', // standard redirect
  SQUID_LOG_TCP_HIT_X_REDIRECT        = '<', // extended redirect
  SQUID_LOG_TCP_MISS_X_REDIRECT       = '>', // extended redirect
  SQUID_LOG_UDP_HIT                   = 'g',
  SQUID_LOG_UDP_WEAK_HIT              = 'h',
  SQUID_LOG_UDP_HIT_OBJ               = 'i',
  SQUID_LOG_UDP_MISS                  = 'j',
  SQUID_LOG_UDP_DENIED                = 'k',
  SQUID_LOG_UDP_INVALID               = 'l',
  SQUID_LOG_UDP_RELOADING             = 'm',
  SQUID_LOG_UDP_FUTURE_1              = 'n',
  SQUID_LOG_UDP_FUTURE_2              = 'o',
  SQUID_LOG_ERR_READ_TIMEOUT          = 'p',
  SQUID_LOG_ERR_LIFETIME_EXP          = 'q',
  SQUID_LOG_ERR_POST_ENTITY_TOO_LARGE = 'L',
  SQUID_LOG_ERR_NO_CLIENTS_BIG_OBJ    = 'r',
  SQUID_LOG_ERR_READ_ERROR            = 's',
  SQUID_LOG_ERR_CLIENT_ABORT          = 't',
  SQUID_LOG_ERR_CONNECT_FAIL          = 'u',
  SQUID_LOG_ERR_INVALID_REQ           = 'v',
  SQUID_LOG_ERR_UNSUP_REQ             = 'w',
  SQUID_LOG_ERR_INVALID_URL           = 'x',
  SQUID_LOG_ERR_NO_FDS                = 'y',
  SQUID_LOG_ERR_DNS_FAIL              = 'z',
  SQUID_LOG_ERR_NOT_IMPLEMENTED       = 'A',
  SQUID_LOG_ERR_CANNOT_FETCH          = 'B',
  SQUID_LOG_ERR_NO_RELAY              = 'C',
  SQUID_LOG_ERR_DISK_IO               = 'D',
  SQUID_LOG_ERR_ZERO_SIZE_OBJECT      = 'E',
  SQUID_LOG_ERR_PROXY_DENIED          = 'G',
  SQUID_LOG_ERR_WEBFETCH_DETECTED     = 'H',
  SQUID_LOG_ERR_FUTURE_1              = 'I',
  SQUID_LOG_ERR_UNKNOWN               = 'Z'
};

/* squid hieratchy codes */
enum SquidHierarchyCode {
  SQUID_HIER_EMPTY                           = '0',
  SQUID_HIER_NONE                            = '1',
  SQUID_HIER_DIRECT                          = '2',
  SQUID_HIER_SIBLING_HIT                     = '3',
  SQUID_HIER_PARENT_HIT                      = '4',
  SQUID_HIER_DEFAULT_PARENT                  = '5',
  SQUID_HIER_SINGLE_PARENT                   = '6',
  SQUID_HIER_FIRST_UP_PARENT                 = '7',
  SQUID_HIER_NO_PARENT_DIRECT                = '8',
  SQUID_HIER_FIRST_PARENT_MISS               = '9',
  SQUID_HIER_LOCAL_IP_DIRECT                 = 'a',
  SQUID_HIER_FIREWALL_IP_DIRECT              = 'b',
  SQUID_HIER_NO_DIRECT_FAIL                  = 'c',
  SQUID_HIER_SOURCE_FASTEST                  = 'd',
  SQUID_HIER_SIBLING_UDP_HIT_OBJ             = 'e',
  SQUID_HIER_PARENT_UDP_HIT_OBJ              = 'f',
  SQUID_HIER_PASSTHROUGH_PARENT              = 'g',
  SQUID_HIER_SSL_PARENT_MISS                 = 'h',
  SQUID_HIER_INVALID_CODE                    = 'i',
  SQUID_HIER_TIMEOUT_DIRECT                  = 'j',
  SQUID_HIER_TIMEOUT_SIBLING_HIT             = 'k',
  SQUID_HIER_TIMEOUT_PARENT_HIT              = 'l',
  SQUID_HIER_TIMEOUT_DEFAULT_PARENT          = 'm',
  SQUID_HIER_TIMEOUT_SINGLE_PARENT           = 'n',
  SQUID_HIER_TIMEOUT_FIRST_UP_PARENT         = 'o',
  SQUID_HIER_TIMEOUT_NO_PARENT_DIRECT        = 'p',
  SQUID_HIER_TIMEOUT_FIRST_PARENT_MISS       = 'q',
  SQUID_HIER_TIMEOUT_LOCAL_IP_DIRECT         = 'r',
  SQUID_HIER_TIMEOUT_FIREWALL_IP_DIRECT      = 's',
  SQUID_HIER_TIMEOUT_NO_DIRECT_FAIL          = 't',
  SQUID_HIER_TIMEOUT_SOURCE_FASTEST          = 'u',
  SQUID_HIER_TIMEOUT_SIBLING_UDP_HIT_OBJ     = 'v',
  SQUID_HIER_TIMEOUT_PARENT_UDP_HIT_OBJ      = 'w',
  SQUID_HIER_TIMEOUT_PASSTHROUGH_PARENT      = 'x',
  SQUID_HIER_TIMEOUT_TIMEOUT_SSL_PARENT_MISS = 'y',
  SQUID_HIER_INVALID_ASSIGNED_CODE           = 'z'
};

/* squid hit/miss codes */
enum SquidHitMissCode {
  SQUID_HIT_RESERVED                   = '0', // Kinda wonky that this is '0', so skipping 'A' for now
  SQUID_HIT_LEVEL_1                    = 'B',
  SQUID_HIT_LEVEL_2                    = 'C',
  SQUID_HIT_LEVEL_3                    = 'D',
  SQUID_HIT_LEVEL_4                    = 'E',
  SQUID_HIT_LEVEL_5                    = 'F',
  SQUID_HIT_LEVEL_6                    = 'G',
  SQUID_HIT_LEVEL_7                    = 'H',
  SQUID_HIT_LEVEL_8                    = 'I',
  SQUID_HIT_LEVEl_9                    = 'J',
  SQUID_MISS_NONE                      = '1',
  SQUID_MISS_ICP_AUTH                  = '2',
  SQUID_MISS_HTTP_NON_CACHE            = '3',
  SQUID_MISS_ICP_STOPLIST              = '4',
  SQUID_MISS_HTTP_NO_DLE               = '5',
  SQUID_MISS_HTTP_NO_LE                = '6',
  SQUID_MISS_HTTP_CONTENT              = '7',
  SQUID_MISS_PRAGMA_NOCACHE            = '8',
  SQUID_MISS_PASS                      = '9',
  SQUID_MISS_PRE_EXPIRED               = 'a',
  SQUID_MISS_ERROR                     = 'b',
  SQUID_MISS_CACHE_BYPASS              = 'c',
  SQUID_HIT_MISS_INVALID_ASSIGNED_CODE = 'z',
  // These are pre-allocated with special semantics, added here for convenience
  SQUID_HIT_RAM     = SQUID_HIT_LEVEL_1,
  SQUID_HIT_SSD     = SQUID_HIT_LEVEL_2,
  SQUID_HIT_DISK    = SQUID_HIT_LEVEL_3,
  SQUID_HIT_CLUSTER = SQUID_HIT_LEVEL_4,
  SQUID_HIT_NET     = SQUID_HIT_LEVEL_5
};

enum HTTPType {
  HTTP_TYPE_UNKNOWN,
  HTTP_TYPE_REQUEST,
  HTTP_TYPE_RESPONSE,
};

struct HTTPHdrImpl : public HdrHeapObjImpl {
  // HdrHeapObjImpl is 4 bytes
  HTTPType m_polarity; // request or response or unknown
  int32_t m_version;   // cooked version number
  // 12 bytes means 4 bytes padding here on 64-bit architectures
  union {
    struct {
      URLImpl *m_url_impl;
      const char *m_ptr_method;
      uint16_t m_len_method;
      int16_t m_method_wks_idx;
    } req;

    struct {
      const char *m_ptr_reason;
      uint16_t m_len_reason;
      int16_t m_status;
    } resp;
  } u;

  MIMEHdrImpl *m_fields_impl;

  // Marshaling Functions
  int marshal(MarshalXlate *ptr_xlate, int num_ptr, MarshalXlate *str_xlate, int num_str);
  void unmarshal(intptr_t offset);
  void move_strings(HdrStrHeap *new_heap);
  size_t strings_length();

  // Sanity Check Functions
  void check_strings(HeapCheck *heaps, int num_heaps);
};

struct HTTPValAccept {
  char *type;
  char *subtype;
  double qvalue;
};

struct HTTPValAcceptCharset {
  char *charset;
  double qvalue;
};

struct HTTPValAcceptEncoding {
  char *encoding;
  double qvalue;
};

struct HTTPValAcceptLanguage {
  char *language;
  double qvalue;
};

struct HTTPValFieldList {
  char *name;
  HTTPValFieldList *next;
};

struct HTTPValCacheControl {
  const char *directive;

  union {
    int delta_seconds;
    HTTPValFieldList *field_names;
  } u;
};

struct HTTPValRange {
  int start;
  int end;
  HTTPValRange *next;
};

struct HTTPValTE {
  char *encoding;
  double qvalue;
};

struct HTTPParser {
  bool m_parsing_http;
  bool m_allow_non_http;
  MIMEParser m_mime_parser;
};

extern const char *HTTP_METHOD_CONNECT;
extern const char *HTTP_METHOD_DELETE;
extern const char *HTTP_METHOD_GET;
extern const char *HTTP_METHOD_HEAD;
extern const char *HTTP_METHOD_ICP_QUERY;
extern const char *HTTP_METHOD_OPTIONS;
extern const char *HTTP_METHOD_POST;
extern const char *HTTP_METHOD_PURGE;
extern const char *HTTP_METHOD_PUT;
extern const char *HTTP_METHOD_TRACE;
extern const char *HTTP_METHOD_PUSH;

extern int HTTP_WKSIDX_CONNECT;
extern int HTTP_WKSIDX_DELETE;
extern int HTTP_WKSIDX_GET;
extern int HTTP_WKSIDX_HEAD;
extern int HTTP_WKSIDX_ICP_QUERY;
extern int HTTP_WKSIDX_OPTIONS;
extern int HTTP_WKSIDX_POST;
extern int HTTP_WKSIDX_PURGE;
extern int HTTP_WKSIDX_PUT;
extern int HTTP_WKSIDX_TRACE;
extern int HTTP_WKSIDX_PUSH;
extern int HTTP_WKSIDX_METHODS_CNT;

extern int HTTP_LEN_CONNECT;
extern int HTTP_LEN_DELETE;
extern int HTTP_LEN_GET;
extern int HTTP_LEN_HEAD;
extern int HTTP_LEN_ICP_QUERY;
extern int HTTP_LEN_OPTIONS;
extern int HTTP_LEN_POST;
extern int HTTP_LEN_PURGE;
extern int HTTP_LEN_PUT;
extern int HTTP_LEN_TRACE;
extern int HTTP_LEN_PUSH;

extern const char *HTTP_VALUE_BYTES;
extern const char *HTTP_VALUE_CHUNKED;
extern const char *HTTP_VALUE_CLOSE;
extern const char *HTTP_VALUE_COMPRESS;
extern const char *HTTP_VALUE_DEFLATE;
extern const char *HTTP_VALUE_GZIP;
extern const char *HTTP_VALUE_IDENTITY;
extern const char *HTTP_VALUE_KEEP_ALIVE;
extern const char *HTTP_VALUE_MAX_AGE;
extern const char *HTTP_VALUE_MAX_STALE;
extern const char *HTTP_VALUE_MIN_FRESH;
extern const char *HTTP_VALUE_MUST_REVALIDATE;
extern const char *HTTP_VALUE_NONE;
extern const char *HTTP_VALUE_NO_CACHE;
extern const char *HTTP_VALUE_NO_STORE;
extern const char *HTTP_VALUE_NO_TRANSFORM;
extern const char *HTTP_VALUE_ONLY_IF_CACHED;
extern const char *HTTP_VALUE_PRIVATE;
extern const char *HTTP_VALUE_PROXY_REVALIDATE;
extern const char *HTTP_VALUE_PUBLIC;
extern const char *HTTP_VALUE_S_MAXAGE;
extern const char *HTTP_VALUE_NEED_REVALIDATE_ONCE;
extern const char *HTTP_VALUE_100_CONTINUE;

extern int HTTP_LEN_BYTES;
extern int HTTP_LEN_CHUNKED;
extern int HTTP_LEN_CLOSE;
extern int HTTP_LEN_COMPRESS;
extern int HTTP_LEN_DEFLATE;
extern int HTTP_LEN_GZIP;
extern int HTTP_LEN_IDENTITY;
extern int HTTP_LEN_KEEP_ALIVE;
extern int HTTP_LEN_MAX_AGE;
extern int HTTP_LEN_MAX_STALE;
extern int HTTP_LEN_MIN_FRESH;
extern int HTTP_LEN_MUST_REVALIDATE;
extern int HTTP_LEN_NONE;
extern int HTTP_LEN_NO_CACHE;
extern int HTTP_LEN_NO_STORE;
extern int HTTP_LEN_NO_TRANSFORM;
extern int HTTP_LEN_ONLY_IF_CACHED;
extern int HTTP_LEN_PRIVATE;
extern int HTTP_LEN_PROXY_REVALIDATE;
extern int HTTP_LEN_PUBLIC;
extern int HTTP_LEN_S_MAXAGE;
extern int HTTP_LEN_NEED_REVALIDATE_ONCE;
extern int HTTP_LEN_100_CONTINUE;

static size_t const HTTP_RANGE_BOUNDARY_LEN = 32 + 2 + 16;

/* Private */
void http_hdr_adjust(HTTPHdrImpl *hdrp, int32_t offset, int32_t length, int32_t delta);

/* Public */
void http_init();

inkcoreapi HTTPHdrImpl *http_hdr_create(HdrHeap *heap, HTTPType polarity);
void http_hdr_init(HdrHeap *heap, HTTPHdrImpl *hh, HTTPType polarity);
HTTPHdrImpl *http_hdr_clone(HTTPHdrImpl *s_hh, HdrHeap *s_heap, HdrHeap *d_heap);
void http_hdr_copy_onto(HTTPHdrImpl *s_hh, HdrHeap *s_heap, HTTPHdrImpl *d_hh, HdrHeap *d_heap, bool inherit_strs);

inkcoreapi int http_hdr_print(HdrHeap *heap, HTTPHdrImpl *hh, char *buf, int bufsize, int *bufindex, int *dumpoffset);

void http_hdr_describe(HdrHeapObjImpl *obj, bool recurse = true);

int http_hdr_length_get(HTTPHdrImpl *hh);
// HTTPType               http_hdr_type_get (HTTPHdrImpl *hh);

// int32_t                  http_hdr_version_get (HTTPHdrImpl *hh);
inkcoreapi void http_hdr_version_set(HTTPHdrImpl *hh, int32_t ver);

const char *http_hdr_method_get(HTTPHdrImpl *hh, int *length);
inkcoreapi void http_hdr_method_set(HdrHeap *heap, HTTPHdrImpl *hh, const char *method, int16_t method_wks_idx, int method_length,
                                    bool must_copy);

void http_hdr_url_set(HdrHeap *heap, HTTPHdrImpl *hh, URLImpl *url);

// HTTPStatus             http_hdr_status_get (HTTPHdrImpl *hh);
void http_hdr_status_set(HTTPHdrImpl *hh, HTTPStatus status);
const char *http_hdr_reason_get(HTTPHdrImpl *hh, int *length);
void http_hdr_reason_set(HdrHeap *heap, HTTPHdrImpl *hh, const char *value, int length, bool must_copy);
const char *http_hdr_reason_lookup(unsigned status);

void http_parser_init(HTTPParser *parser);
void http_parser_clear(HTTPParser *parser);
ParseResult http_parser_parse_req(HTTPParser *parser, HdrHeap *heap, HTTPHdrImpl *hh, const char **start, const char *end,
                                  bool must_copy_strings, bool eof, bool strict_uri_parsing);
ParseResult validate_hdr_host(HTTPHdrImpl *hh);
ParseResult http_parser_parse_resp(HTTPParser *parser, HdrHeap *heap, HTTPHdrImpl *hh, const char **start, const char *end,
                                   bool must_copy_strings, bool eof);

HTTPStatus http_parse_status(const char *start, const char *end);
int32_t http_parse_version(const char *start, const char *end);

/*
  HTTPValAccept*         http_parse_accept (const char *buf, Arena *arena);
  HTTPValAcceptCharset*  http_parse_accept_charset (const char *buf, Arena *arena);
  HTTPValAcceptEncoding* http_parse_accept_encoding (const char *buf, Arena *arena);
  HTTPValAcceptLanguage* http_parse_accept_language (const char *buf, Arena *arena);
  HTTPValCacheControl*   http_parse_cache_control (const char *buf, Arena *arena);
  const char*            http_parse_cache_directive (const char **buf);
  HTTPValRange*          http_parse_range (const char *buf, Arena *arena);
*/
HTTPValTE *http_parse_te(const char *buf, int len, Arena *arena);

class HTTPVersion
{
public:
  HTTPVersion();
  explicit HTTPVersion(int32_t version);
  HTTPVersion(int ver_major, int ver_minor);

  void set(HTTPVersion ver);
  void set(int ver_major, int ver_minor);

  HTTPVersion &operator=(const HTTPVersion &hv);
  int operator==(const HTTPVersion &hv) const;
  int operator!=(const HTTPVersion &hv) const;
  int operator>(const HTTPVersion &hv) const;
  int operator<(const HTTPVersion &hv) const;
  int operator>=(const HTTPVersion &hv) const;
  int operator<=(const HTTPVersion &hv) const;

public:
  int32_t m_version;
};

/** A set of content ranges.

    This represents the data for an HTTP range specification.
    On a request this contains the request ranges. On a response it is the actual ranges in the
    response, which are the requested ranges modified by the actual content length.
*/
struct HTTPRangeSpec {
  typedef HTTPRangeSpec self;

  /** A range of bytes in an object.

      If @a _min > 0 and @a _max == 0 the range is backwards and counts from the
      end of the object. That is (100,0) means the last 100 bytes of content.

      A prefix range is represented by a @a _min >= 0 and @c _max == INT64_MAX.

      A prefix or suffix range is an @a open range as it is not fully specified and depends on the
      actual content length.
  */
  struct Range {
    typedef Range self; ///< Self reference type.

    int64_t _min; ///< Minimum offset of the range.
    int64_t _max; ///< Maximum offset of the range.

    /// Default constructor - invalid range.
    Range() : _min(INT64_MAX), _max(1) {}
    /// Construct as the range ( @a low .. @a high )
    Range(int64_t low, int64_t high) : _min(low), _max(high) {}
    /// Test if this range is a suffix range.
    bool isSuffix() const;
    /// Test if this range is a prefix range.
    bool isPrefix() const;
    /// Test if this range is a valid range.
    bool isValid() const;
    /// Get the size (in bytes) of the range.
    int64_t size() const;
    /** Convert range to absolute values for a content length of @a len.

        @return @c true if the range was valid for @a len and all resulting range are absolute.
    */
    bool apply(int64_t len);

    /// Make the range a suffix range.
    self &setSuffix(int64_t len);

    /// Force the range to an empty state.
    Range &invalidate();
  };

  /// Range iteration type.
  typedef Range *iterator;
  typedef Range const *const_iterator;

  /// Current state of the overall specification.
  /// @internal We can distinguish between @c SINGLE and @c MULTI by looking at the
  /// size of @a _ranges but we need this to mark @c EMPTY vs. not.
  enum State {
    EMPTY,         ///< No range.
    INVALID,       ///< Range parsing failed.
    UNSATISFIABLE, ///< Content length resolution failed.
    SINGLE,        ///< Single range.
    MULTI,         ///< Multiple ranges.
  } _state;

  /// The first range value.
  /// By separating this out we can avoid allocation in the case of a single
  /// range value, which is by far the most common ( > 99% in my experience).
  Range _single;
  /// Storage for range values.
  typedef std::vector<Range> RangeBox;
  /// The first range is copied here if there is more than one (to simplify).
  RangeBox _ranges;

  /// Default constructor - empty range
  HTTPRangeSpec();

  /// Reset to re-usable state.
  void clear();

  /** Parse a Range field @a value and update @a this with the results.
      @return @c true if @a value was a valid range specifier, @c false otherwise.
  */
  bool parseRangeFieldValue(char const *value, int len);

  /** Parse a Content-Range field @a value.

      @a r is set to the content range. If the content range is unsatisfied or a parse error the @a range is
      set to be invalid.

      @note The content length return is ambiguous on its own, the state of @a r must be checked.

      - Multipart: @a boundary is not empty
      - Parse error: @a CL == -1 and @a r is invalid
      - Unsatisfiable: @a CL >= 0 and @a r is invalid
      - Indeterminate: @c CL == -1 and @a r is valid

      @return The content length, or -1 if there is an error or the content length is indeterminate.
  */
  static int64_t parseContentRangeFieldValue(char const *value, int len, Range &r, ts::ConstBuffer &boundary);

  /// Print the range specification.
  /// @return The number of characters printed.
  int print(char *buff ///< Output buffer.
            ,
            size_t len ///< Size of output buffer.
            ) const;

  /// Print the range specification quantized.
  /// @return The number of characters printed.
  int print_quantized(char *buff ///< Output buffer.
                      ,
                      size_t len ///< Size of output buffer.
                      ,
                      int64_t quantum ///< Align ranges to multiples of this value.
                      ,
                      int64_t interstitial ///< Require gaps to be at least this large.
                      ,
                      int64_t limit ///< All requested bytes will be less than this if > 0
                      ) const;

  /// Print the @a ranges.
  /// @return The number of characters printed.
  static int print_array(char *buff ///< Output buffer.
                         ,
                         size_t len ///< Size of output buffer.
                         ,
                         Range const *ranges ///< Array of ranges
                         ,
                         int count ///< # of ranges
                         );

#if 0
  /** Copy ranges from @a while applying them to the content @a length.

      Ranges are copied if valid for @a length and converted to absolute offsets. The number of ranges
      after application may be less than the @a src number of ranges. In addition ranges will be clipped
      to @a length.

      @return @c true if the range spec is satisfiable, @c false otherwise.
      Note a range spec with no ranges is always satisfiable and that suffix ranges are also
      always satisfiable.
  */
  bool apply(self const& that, uint64_t length);
#endif

  /** Update ranges to be absolute based on content @a length.

      Invalid ranges are removed, ranges will be clipped as needed, and suffix ranges will be
      converted to absolute ranges.

      @return @c true if the range spec is satisfiable (there remains at least one valid range), @c false otherwise.
      Note a range spec with no ranges is always satisfiable and that suffix ranges are also
      always satisfiable.
  */
  bool apply(int64_t length);

  /** Number of distinct ranges.
      @return Number of ranges.
  */
  size_t count() const;

  /// Get the size (in bytes) of the ranges.
  uint64_t size() const;

  /// If this is a valid  single range specification.
  bool isSingle() const;

  /// If this is a valid multi range specification.
  bool isMulti() const;

  /// Test if this contains at least one valid range.
  bool hasRanges() const;

  /// Test if this is a well formed range (may be empty).
  bool isValid() const;

  /// Test if this is a valid but empty range spec.
  bool isEmpty() const;

  /// Test if this is an unsatisfied range.
  bool isUnsatisfied() const;

  /// Test if there is an open range (not absolute offsets)
  bool hasOpenRange() const;

  /// Access the range at index @a idx.
  Range &operator[](int n);

  /// Access the range at index @a idx.
  Range const &operator[](int n) const;

  /// Calculate the convex hull of the range spec.
  /// The convex hull is the smallest single range that contains all of the ranges in the range spec.
  /// @note This will return an invalid range if there are no ranges in the range spec.
  /// @see HttpRangeSpec::Range::isValid
  Range getConvexHull() const;

  /** Calculate the content length for this range specification.

      @note If a specific content length has not been @c apply 'd this will not produce
      a usable result.

      @return The content length for the ranges including the range separators.
  */
  uint64_t calcContentLength(uint64_t base_content_size, ///< Content size w/o ranges.
                             uint64_t ct_val_len         ///< Length of Content-Type field value.
                             ) const;

  /// Calculate the length of the range part boundary header.
  static uint64_t calcPartBoundarySize(uint64_t object_size ///< Base content size
                                       ,
                                       uint64_t ct_val_len ///< Length of the Content-Type value (0 if none).
                                       );

  /** Write the range part boundary to @a out.
   */
  static uint64_t writePartBoundary(MIOBuffer *out ///< Output IO Buffer
                                    ,
                                    char const *boundary_str ///< Boundary marker string.
                                    ,
                                    size_t boundary_len ///< Length of boundary marker string.
                                    ,
                                    uint64_t total_size ///< Base content size.
                                    ,
                                    uint64_t low ///< Low value for the range.
                                    ,
                                    uint64_t high ///< High value for the raNGE.
                                    ,
                                    MIMEField *ctf ///< Content-Type field (@c NULL if none)
                                    ,
                                    bool final ///< Is this the final part boundary?
                                    );

  /// Iterator for first range.
  iterator begin();
  const_iterator begin() const;
  /// Iterator past last range.
  iterator end();
  const_iterator end() const;

  self &add(uint64_t low, uint64_t high);
  self &add(Range const &r);
};

class IOBufferReader;

class HTTPHdr : public MIMEHdr
{
public:
  HTTPHdrImpl *m_http;
  // This is all cached data and so is mutable.
  mutable URL m_url_cached;
  mutable MIMEField *m_host_mime;
  mutable int m_host_length;    ///< Length of hostname.
  mutable int m_port;           ///< Target port.
  mutable bool m_target_cached; ///< Whether host name and port are cached.
  mutable bool m_target_in_url; ///< Whether host name and port are in the URL.
  /// Set if the port was effectively specified in the header.
  /// @c true if the target (in the URL or the HOST field) also specified
  /// a port. That is, @c true if whatever source had the target host
  /// also had a port, @c false otherwise.
  mutable bool m_port_in_header;

  HTTPHdr();
  ~HTTPHdr();

  int valid() const;

  void create(HTTPType polarity, HdrHeap *heap = NULL);
  void clear();
  void reset();
  void copy(const HTTPHdr *hdr);
  void copy_shallow(const HTTPHdr *hdr);

  int unmarshal(char *buf, int len, RefCountObj *block_ref);

  int print(char *buf, int bufsize, int *bufindex, int *dumpoffset);

  int length_get();

  HTTPType type_get() const;

  HTTPVersion version_get() const;
  void version_set(HTTPVersion version);

  const char *method_get(int *length);
  int method_get_wksidx();
  void method_set(const char *value, int length);

  URL *url_create(URL *url);

  URL *url_get() const;
  URL *url_get(URL *url);
  /** Get a string with the effective URL in it.
      If @a length is not @c NULL then the length of the string
      is stored in the int pointed to by @a length.

      Note that this can be different from getting the @c URL
      and invoking @c URL::string_get if the host is in a header
      field and not explicitly in the URL.
   */
  char *url_string_get(Arena *arena = 0, ///< Arena to use, or @c malloc if NULL.
                       int *length  = 0  ///< Store string length here.
                       );
  /** Get a string with the effective URL in it.
      This is automatically allocated if needed in the request heap.

      @see url_string_get
   */
  char *url_string_get_ref(int *length = 0 ///< Store string length here.
                           );

  /** Print the URL.
      Output is not null terminated.
      @return 0 on failure, non-zero on success.
   */
  int url_print(char *buff,  ///< Output buffer
                int length,  ///< Length of @a buffer
                int *offset, ///< [in,out] ???
                int *skip    ///< [in,out] ???
                );

  /** Get the URL path.
      This is a reference, not allocated.
      @return A pointer to the path or @c NULL if there is no valid URL.
  */
  const char *path_get(int *length ///< Storage for path length.
                       );

  /** Get the target host name.
      The length is returned in @a length if non-NULL.
      @note The results are cached so this is fast after the first call.
      @return A pointer to the host name.
  */
  const char *host_get(int *length = 0);

  /** Get the target port.
      If the target port is not found then it is adjusted to the
      default port for the URL type.
      @note The results are cached so this is fast after the first call.
      @return The canonicalized target port.
  */
  int port_get();

  /** Get the URL scheme.
      This is a reference, not allocated.
      @return A pointer to the scheme or @c NULL if there is no valid URL.
  */
  const char *scheme_get(int *length ///< Storage for path length.
                         );
  void url_set(URL *url);
  void url_set_as_server_url(URL *url);
  void url_set(const char *str, int length);

  /// Check location of target host.
  /// @return @c true if the host was in the URL, @c false otherwise.
  /// @note This returns @c false if the host is missing.
  bool is_target_in_url() const;

  /// Check if a port was specified in the target.
  /// @return @c true if the port was part of the target.
  bool is_port_in_header() const;

  /// If the target is in the fields and not the URL, copy it to the @a url.
  /// If @a url is @c NULL the cached URL in this header is used.
  /// @note In the default case the copy is avoided if the cached URL already
  /// has the target. If @a url is non @c NULL the copy is always performed.
  void set_url_target_from_host_field(URL *url = 0);

  /// Mark the target cache as invalid.
  /// @internal Ugly but too many places currently that touch the
  /// header internals, they must be able to do this.
  void mark_target_dirty() const;

  HTTPStatus status_get();
  void status_set(HTTPStatus status);

  const char *reason_get(int *length);
  void reason_set(const char *value, int length);
  void reason_set(HTTPStatus status);

  ParseResult parse_req(HTTPParser *parser, const char **start, const char *end, bool eof, bool strict_uri_parsing = false);
  ParseResult parse_resp(HTTPParser *parser, const char **start, const char *end, bool eof);

  ParseResult parse_req(HTTPParser *parser, IOBufferReader *r, int *bytes_used, bool eof, bool strict_uri_parsing = false);
  ParseResult parse_resp(HTTPParser *parser, IOBufferReader *r, int *bytes_used, bool eof);

public:
  // Utility routines
  bool is_cache_control_set(const char *cc_directive_wks);
  bool is_pragma_no_cache_set();
  bool is_keep_alive_set() const;
  HTTPKeepAlive keep_alive_get() const;

protected:
  /** Load the target cache.
      @see m_host, m_port, m_target_in_url
  */
  void _fill_target_cache() const;
  /** Test the cache and fill it if necessary.
      @internal In contrast to @c _fill_target_cache, this method
      is inline and checks whether the cache is already filled.
      @ _fill_target_cache @b always does a cache fill.
  */
  void _test_and_fill_target_cache() const;

  static Arena *const USE_HDR_HEAP_MAGIC;

private:
  // No gratuitous copies!
  HTTPHdr(const HTTPHdr &m);
  HTTPHdr &operator=(const HTTPHdr &m);

  friend class UrlPrintHack; // don't ask.
};

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPVersion::HTTPVersion() : m_version(HTTP_VERSION(1, 0))
{
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPVersion::HTTPVersion(int32_t version) : m_version(version)
{
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPVersion::HTTPVersion(int ver_major, int ver_minor) : m_version(HTTP_VERSION(ver_major, ver_minor))
{
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPVersion::set(HTTPVersion ver)
{
  m_version = ver.m_version;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPVersion::set(int ver_major, int ver_minor)
{
  m_version = HTTP_VERSION(ver_major, ver_minor);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPVersion &
HTTPVersion::operator=(const HTTPVersion &hv)
{
  m_version = hv.m_version;

  return *this;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPVersion::operator==(const HTTPVersion &hv) const
{
  return (m_version == hv.m_version);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPVersion::operator!=(const HTTPVersion &hv) const
{
  return (m_version != hv.m_version);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPVersion::operator>(const HTTPVersion &hv) const
{
  return (m_version > hv.m_version);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPVersion::operator<(const HTTPVersion &hv) const
{
  return (m_version < hv.m_version);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPVersion::operator>=(const HTTPVersion &hv) const
{
  return (m_version >= hv.m_version);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPVersion::operator<=(const HTTPVersion &hv) const
{
  return (m_version <= hv.m_version);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPHdr::HTTPHdr() : MIMEHdr(), m_http(NULL), m_url_cached(), m_target_cached(false)
{
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/
inline HTTPHdr::~HTTPHdr()
{ /* nop */
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPHdr::valid() const
{
  return (m_http && m_mime && m_heap);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::create(HTTPType polarity, HdrHeap *heap)
{
  if (heap) {
    m_heap = heap;
  } else if (!m_heap) {
    m_heap = new_HdrHeap();
  }

  m_http = http_hdr_create(m_heap, polarity);
  m_mime = m_http->m_fields_impl;
}

inline void
HTTPHdr::clear()
{
  if (m_http && m_http->m_polarity == HTTP_TYPE_REQUEST) {
    m_url_cached.clear();
  }
  this->HdrHeapSDKHandle::clear();
  m_http = NULL;
  m_mime = NULL;
}

inline void
HTTPHdr::reset()
{
  m_heap = NULL;
  m_http = NULL;
  m_mime = NULL;
  m_url_cached.reset();
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::copy(const HTTPHdr *hdr)
{
  ink_assert(hdr->valid());

  if (valid()) {
    http_hdr_copy_onto(hdr->m_http, hdr->m_heap, m_http, m_heap, (m_heap != hdr->m_heap) ? true : false);
  } else {
    m_heap = new_HdrHeap();
    m_http = http_hdr_clone(hdr->m_http, hdr->m_heap, m_heap);
    m_mime = m_http->m_fields_impl;
  }
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::copy_shallow(const HTTPHdr *hdr)
{
  ink_assert(hdr->valid());

  m_heap = hdr->m_heap;
  m_http = hdr->m_http;
  m_mime = hdr->m_mime;

  if (hdr->type_get() == HTTP_TYPE_REQUEST && m_url_cached.valid())
    m_url_cached.copy_shallow(&hdr->m_url_cached);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPHdr::print(char *buf, int bufsize, int *bufindex, int *dumpoffset)
{
  ink_assert(valid());
  return http_hdr_print(m_heap, m_http, buf, bufsize, bufindex, dumpoffset);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPHdr::length_get()
{
  ink_assert(valid());
  return http_hdr_length_get(m_http);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::_test_and_fill_target_cache() const
{
  if (!m_target_cached)
    this->_fill_target_cache();
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline const char *
HTTPHdr::host_get(int *length)
{
  this->_test_and_fill_target_cache();
  if (m_target_in_url) {
    return url_get()->host_get(length);
  } else if (m_host_mime) {
    if (length)
      *length = m_host_length;
    return m_host_mime->m_ptr_value;
  }

  if (length)
    *length = 0;
  return NULL;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int
HTTPHdr::port_get()
{
  this->_test_and_fill_target_cache();
  return m_port;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline bool
HTTPHdr::is_target_in_url() const
{
  this->_test_and_fill_target_cache();
  return m_target_in_url;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline bool
HTTPHdr::is_port_in_header() const
{
  this->_test_and_fill_target_cache();
  return m_port_in_header;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::mark_target_dirty() const
{
  m_target_cached = false;
}
/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPType
http_hdr_type_get(HTTPHdrImpl *hh)
{
  return (hh->m_polarity);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPType
HTTPHdr::type_get() const
{
  ink_assert(valid());
  return http_hdr_type_get(m_http);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline int32_t
http_hdr_version_get(HTTPHdrImpl *hh)
{
  return (hh->m_version);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPVersion
HTTPHdr::version_get() const
{
  ink_assert(valid());
  return HTTPVersion(http_hdr_version_get(m_http));
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline static HTTPKeepAlive
is_header_keep_alive(const HTTPVersion &http_version, const MIMEField *con_hdr)
{
  enum {
    CON_TOKEN_NONE = 0,
    CON_TOKEN_KEEP_ALIVE,
    CON_TOKEN_CLOSE,
  };

  int con_token            = CON_TOKEN_NONE;
  HTTPKeepAlive keep_alive = HTTP_NO_KEEPALIVE;
  //    *unknown_tokens = false;

  if (con_hdr) {
    if (con_hdr->value_get_index("keep-alive", 10) >= 0)
      con_token = CON_TOKEN_KEEP_ALIVE;
    else if (con_hdr->value_get_index("close", 5) >= 0)
      con_token = CON_TOKEN_CLOSE;
  }

  if (HTTPVersion(1, 0) == http_version) {
    keep_alive = (con_token == CON_TOKEN_KEEP_ALIVE) ? (HTTP_KEEPALIVE) : (HTTP_NO_KEEPALIVE);
  } else if (HTTPVersion(1, 1) == http_version) {
    // We deviate from the spec here.  If the we got a response where
    //   where there is no Connection header and the request 1.0 was
    //   1.0 don't treat this as keep-alive since Netscape-Enterprise/3.6 SP1
    //   server doesn't
    keep_alive = ((con_token == CON_TOKEN_KEEP_ALIVE) || (con_token == CON_TOKEN_NONE && HTTPVersion(1, 1) == http_version)) ?
                   (HTTP_KEEPALIVE) :
                   (HTTP_NO_KEEPALIVE);
  } else {
    keep_alive = HTTP_NO_KEEPALIVE;
  }
  return (keep_alive);
}

inline HTTPKeepAlive
HTTPHdr::keep_alive_get() const
{
  HTTPKeepAlive retval = HTTP_NO_KEEPALIVE;
  const MIMEField *pc  = this->field_find(MIME_FIELD_PROXY_CONNECTION, MIME_LEN_PROXY_CONNECTION);
  if (pc != NULL) {
    retval = is_header_keep_alive(this->version_get(), pc);
  } else {
    const MIMEField *c = this->field_find(MIME_FIELD_CONNECTION, MIME_LEN_CONNECTION);
    retval             = is_header_keep_alive(this->version_get(), c);
  }
  return retval;
}

inline bool
HTTPHdr::is_keep_alive_set() const
{
  return this->keep_alive_get() == HTTP_KEEPALIVE;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::version_set(HTTPVersion version)
{
  ink_assert(valid());
  http_hdr_version_set(m_http, version.m_version);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline const char *
HTTPHdr::method_get(int *length)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  return http_hdr_method_get(m_http, length);
}

inline int
HTTPHdr::method_get_wksidx()
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  return (m_http->u.req.m_method_wks_idx);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::method_set(const char *value, int length)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  int method_wks_idx = hdrtoken_tokenize(value, length);
  http_hdr_method_set(m_heap, m_http, value, method_wks_idx, length, true);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline URL *
HTTPHdr::url_create(URL *u)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  u->set(this);
  u->create(m_heap);
  return (u);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline URL *
HTTPHdr::url_get() const
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  // It's entirely possible that someone changed URL in our impl
  // without updating the cached copy in the C++ layer.  Check
  // to see if this happened before handing back the url

  URLImpl *real_impl = m_http->u.req.m_url_impl;
  if (m_url_cached.m_url_impl != real_impl) {
    m_url_cached.set(this);
    m_url_cached.m_url_impl = real_impl;
    this->mark_target_dirty();
  }
  return (&m_url_cached);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline URL *
HTTPHdr::url_get(URL *url)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  url->set(this); // attach refcount
  url->m_url_impl = m_http->u.req.m_url_impl;
  return (url);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::url_set(URL *url)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  URLImpl *url_impl = m_http->u.req.m_url_impl;
  ::url_copy_onto(url->m_url_impl, url->m_heap, url_impl, m_heap, true);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::url_set_as_server_url(URL *url)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  URLImpl *url_impl = m_http->u.req.m_url_impl;
  ::url_copy_onto_as_server_url(url->m_url_impl, url->m_heap, url_impl, m_heap, true);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::url_set(const char *str, int length)
{
  URLImpl *url_impl;

  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  url_impl = m_http->u.req.m_url_impl;
  ::url_clear(url_impl);
  ::url_parse(m_heap, url_impl, &str, str + length, true);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPStatus
http_hdr_status_get(HTTPHdrImpl *hh)
{
  ink_assert(hh->m_polarity == HTTP_TYPE_RESPONSE);
  return (HTTPStatus)hh->u.resp.m_status;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline HTTPStatus
HTTPHdr::status_get()
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_RESPONSE);

  return (NULL == m_http) ? HTTP_STATUS_NONE : http_hdr_status_get(m_http);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::status_set(HTTPStatus status)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_RESPONSE);

  http_hdr_status_set(m_http, status);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline const char *
HTTPHdr::reason_get(int *length)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_RESPONSE);

  return http_hdr_reason_get(m_http, length);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::reason_set(const char *value, int length)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_RESPONSE);

  http_hdr_reason_set(m_heap, m_http, value, length, true);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline void
HTTPHdr::reason_set(HTTPStatus status)
{
  char const *phrase = http_hdr_reason_lookup(status);
  this->reason_set(phrase, strlen(phrase));
}
/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline MIMEParseResult
HTTPHdr::parse_req(HTTPParser *parser, const char **start, const char *end, bool eof, bool strict_uri_parsing)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_REQUEST);

  return http_parser_parse_req(parser, m_heap, m_http, start, end, true, eof, strict_uri_parsing);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline ParseResult
HTTPHdr::parse_resp(HTTPParser *parser, const char **start, const char *end, bool eof)
{
  ink_assert(valid());
  ink_assert(m_http->m_polarity == HTTP_TYPE_RESPONSE);

  return http_parser_parse_resp(parser, m_heap, m_http, start, end, true, eof);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline bool
HTTPHdr::is_cache_control_set(const char *cc_directive_wks)
{
  ink_assert(valid());
  ink_assert(hdrtoken_is_wks(cc_directive_wks));

  HdrTokenHeapPrefix *prefix = hdrtoken_wks_to_prefix(cc_directive_wks);
  ink_assert(prefix->wks_token_type == HDRTOKEN_TYPE_CACHE_CONTROL);

  uint32_t cc_mask = prefix->wks_type_specific.u.cache_control.cc_mask;
  if (get_cooked_cc_mask() & cc_mask)
    return (true);
  else
    return (false);
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

inline bool
HTTPHdr::is_pragma_no_cache_set()
{
  ink_assert(valid());
  return (get_cooked_pragma_no_cache());
}

inline char *
HTTPHdr::url_string_get_ref(int *length)
{
  return this->url_string_get(USE_HDR_HEAP_MAGIC, length);
}

inline const char *
HTTPHdr::path_get(int *length)
{
  URL *url = this->url_get();
  return url ? url->path_get(length) : 0;
}

inline const char *
HTTPHdr::scheme_get(int *length)
{
  URL *url = this->url_get();
  return url ? url->scheme_get(length) : 0;
}

/*-------------------------------------------------------------------------
  -------------------------------------------------------------------------*/

enum {
  CACHE_ALT_MAGIC_ALIVE     = 0xabcddeed,
  CACHE_ALT_MAGIC_MARSHALED = 0xdcbadeed,
  CACHE_ALT_MAGIC_DEAD      = 0xdeadeed,
};

/// Header for an alternate of an object.
/// This is close to a POD, all the real API is in the @c HTTPInfo class.
/// @note THIS IS DIRECTLY SERIALIZED TO DISK
/// (after some tweaks, but any member in this struct will be written to disk)
struct HTTPCacheAlt {
  /// Information about a fragment in this alternate.
  /// @internal Currently @c Dir has only 40 bits for the disk offset of a fragment,
  /// and since no object (or alternate) is split across stripes (and thence disks)
  /// no fragment can have an internal offset more than 40 bits long, so 48 bits
  /// should suffice here.
  struct FragmentDescriptor {
    CryptoHash m_key;       ///< Key for fragment.
    uint64_t m_offset : 48; ///< Starting offset of fragment in object.
    union {
      uint16_t m_flags;
      struct {
        unsigned int cached_p : 1; ///< Presence bit (is fragment in cache?)
        unsigned int zero : 15;    ///< Zero fill for future use.
      } m_flag;
    };
  };

  /** Holds the table of fragment descriptors.

      @internal To avoid allocating 2 chunks of memory we hang the descriptors off the end of this structure and provide
      a method to do the calculations. The @a m_size contains the number of descriptors, the actual byte size must be
      computed from that. The count of valid entries is held in this structure, not in the table, because it makes
      serialization easier.  We don't serialize the explicit contents of the table struct (e.g., the capacity / @a
      m_size value) only the descriptors.
  */
  struct FragmentDescriptorTable {
    /** The number of entries in the table.
        Because this is a 1 based array, this is also the largest valid index.
        @note It is 1 less than the total number of fragment descriptors because earliest is stored
        directly and not in this table.
        @note This is never zero because in that case the table isn't created at all.
     */
    uint32_t m_n;

    /** Fragment index of last initial segment cached.

        All fragments from the earliest to this are in cache.

        @note A simple effort to minimize the cost of detecting a complete object.
        In the normal case we'll get all the fragments in order so this will roll along nicely.
        Otherwise we may have to do a lot of work on a single fragment, but that is better
        than doing it every time for every fragment.
    */
    uint32_t m_cached_idx;

    /** Array operator for fragments in the table (1-based).
        This is a bit tricky. The earliest fragment is special and so is @b not stored in this table.
        To make that easier to deal with this array is one based so the containing object can simply
        pass the index on if it's not 0 (earliest). From an external point of view the array of fragments
        is zero based.
     */
    FragmentDescriptor &operator[](int idx);
    /// Calculate the allocation size needed for a maximum array index of @a n.
    static size_t calc_size(uint32_t n);
  };

  HTTPCacheAlt();

  void copy(HTTPCacheAlt *to_copy);
  void destroy();

  uint32_t m_magic;

  union {
    uint32_t m_flags;
    struct {
      /** Do we own our own buffer?
          @c true if the buffer containing this data is owned by this object.
          INVARIANT: if we own this buffer then we also own the buffers for
          @a m_request_hdr and @a m_response_hdr.
      */
      uint32_t writeable_p : 1;
      /// Was this alternate originally stored as a partial object?
      uint32_t composite_p : 1;
      /// Did the origin tell us the actual length of the object?
      uint32_t content_length_p : 1;
      /// Are all fragments in cache?
      uint32_t complete_p : 1;
      /// Is the fragment table independently allocated?
      uint32_t table_allocated_p : 1;
      // Note - !composite_p => complete_p
      //      - complete_p => content_length_p
    } m_flag;
  };

  int32_t m_unmarshal_len;

  int32_t m_id;
  int32_t m_rid;

  /// # of fragments in the cached version of the alternate, including the earliest fragment.
  /// This can be zero for a resident alternate.
  /// @internal In practice this is the high water mark for cached fragments, the maximum index of cached fragments.
  /// This tracks in effect the minimum content size of the actual object - it must be at least this many fragments.
  /// Contrast with the @a m_cached_idx in the fragment table - that marks the high
  /// water of cached initial fragments.
  uint32_t m_frag_count;

  /** The target size for fragments in this alternate.
      This is @b mandatory if the object is being partially cached.
      During read it should be used as a guideline but not considered definitive.
  */
  uint32_t m_fixed_fragment_size;

  HTTPHdr m_request_hdr;
  HTTPHdr m_response_hdr;

  time_t m_request_sent_time;
  time_t m_response_received_time;

  /** Special case the first (earliest, non-resident) fragment.
      This holds the key for the earliest fragment and the object size
      by overloading the offset in this specific instance.
  */
  FragmentDescriptor m_earliest;

  /** Descriptors for the rest of the fragments.
      Because of this, index 0 in this array is really the next fragment after the
      earliest fragment. We should have the invariant
      ( @a m_fragments != 0) == ( @a m_frag_count > 1 )

      @internal I thought of using @c std::vector here, but then we end up with either
      doing 2 allocations (one for the @c std::vector and another for its contents) or
      writing the @c std::vector container to disk (because this struct is directly
      serialized). Instead we do our own memory management, which doesn't make me happy either.
  */
  FragmentDescriptorTable *m_fragments;

  // With clustering, our alt may be in cluster
  //  incoming channel buffer, when we are
  //  destroyed we decrement the refcount
  //  on that buffer so that it gets destroyed
  // We don't want to use a ref count ptr (Ptr<>)
  //  since our ownership model requires explicit
  //  destroys and ref count pointers defeat this
  RefCountObj *m_ext_buffer;
};

class HTTPInfo
{
public:
  typedef HTTPCacheAlt::FragmentDescriptor FragmentDescriptor;           ///< Import type.
  typedef HTTPCacheAlt::FragmentDescriptorTable FragmentDescriptorTable; ///< Import type.

  HTTPCacheAlt *m_alt;

  HTTPInfo() : m_alt(NULL) {}
  ~HTTPInfo() { clear(); }
  void
  clear()
  {
    m_alt = NULL;
  }
  bool
  valid() const
  {
    return m_alt != NULL;
  }

  void create();
  void destroy();

  void copy(HTTPInfo *to_copy);
  void
  copy_shallow(HTTPInfo *info)
  {
    m_alt = info->m_alt;
  }
  HTTPInfo &operator=(const HTTPInfo &m);

  inkcoreapi int marshal_length();
  inkcoreapi int marshal(char *buf, int len);
  static int unmarshal(char *buf, int len, RefCountObj *block_ref);
  void set_buffer_reference(RefCountObj *block_ref);
  int get_handle(char *buf, int len);

  int32_t
  id_get() const
  {
    return m_alt->m_id;
  }
  int32_t
  rid_get()
  {
    return m_alt->m_rid;
  }

  void
  id_set(int32_t id)
  {
    m_alt->m_id = id;
  }
  void
  rid_set(int32_t id)
  {
    m_alt->m_rid = id;
  }

  CryptoHash const &object_key_get();
  void object_key_get(CryptoHash *);
  bool compare_object_key(const CryptoHash *);
  int64_t object_size_get();

  void
  request_get(HTTPHdr *hdr)
  {
    hdr->copy_shallow(&m_alt->m_request_hdr);
  }
  void
  response_get(HTTPHdr *hdr)
  {
    hdr->copy_shallow(&m_alt->m_response_hdr);
  }

  HTTPHdr *
  request_get()
  {
    return &m_alt->m_request_hdr;
  }
  HTTPHdr *
  response_get()
  {
    return &m_alt->m_response_hdr;
  }

  URL *
  request_url_get(URL *url = NULL)
  {
    return m_alt->m_request_hdr.url_get(url);
  }

  time_t
  request_sent_time_get()
  {
    return m_alt->m_request_sent_time;
  }
  time_t
  response_received_time_get()
  {
    return m_alt->m_response_received_time;
  }

  void object_key_set(CryptoHash const &md5);
  void object_size_set(int64_t size);

  void
  request_set(const HTTPHdr *req)
  {
    m_alt->m_request_hdr.copy(req);
  }
  void
  response_set(const HTTPHdr *resp)
  {
    m_alt->m_response_hdr.copy(resp);
  }

  void
  request_sent_time_set(time_t t)
  {
    m_alt->m_request_sent_time = t;
  }
  void
  response_received_time_set(time_t t)
  {
    m_alt->m_response_received_time = t;
  }

  bool
  is_composite() const
  {
    return m_alt->m_flag.composite_p;
  }
  bool
  is_complete() const
  {
    return m_alt->m_flag.complete_p;
  }
  bool
  is_writeable() const
  {
    return m_alt->m_flag.writeable_p;
  }
  int64_t
  get_fixed_fragment_size() const
  {
    return m_alt->m_fixed_fragment_size;
  }

  /** Compute the convex hull of uncached ranges.

      If the resulting range has a minimum that is less than @a initial @b and the earliest fragment
      is not cached then the minimum will be changed to zero. Alternatively, the initial uncached
      segment must be at least @a initial bytes long.

      @return An invalid range if all of the request is available in cache.
  */
  HTTPRangeSpec::Range get_uncached_hull(HTTPRangeSpec const &req ///< [in] UA request with content length applied
                                         ,
                                         int64_t initial ///< Minimize size for uncached initial data
                                         );

  /// Get the fragment table.
  /// @note There is a fragment table only for multi-fragment alternates @b and
  /// the indexing starts with the second (non-earliest) fragment.
  /// @deprecated - use specialized methods.
  FragmentDescriptorTable *get_frag_table();

  /// Force a descriptor at index @a idx.
  FragmentDescriptor *force_frag_at(unsigned int idx);

  /// Get the fragment index for @a offset.
  int get_frag_index_of(int64_t offset);
  /// Get the fragment key for an @a offset.
  /// @note Forces fragment.
  CryptoHash const &get_frag_key_of(int64_t offset);
  /// Get the fragment key of the @a idx fragment.
  /// @note Forces fragment.
  CryptoHash const &get_frag_key(unsigned int idx);
  /// Get the starting offset of a fragment.
  int64_t get_frag_offset(unsigned int idx);
  /// Given a @a position and @a length return the maximum length that does not cross a fragment boundary.
  int64_t clip_to_frag_boundary(int64_t position, int64_t length);
  /// Get the number of fragments.
  /// 0 means resident alternate, 1 means single fragment, > 1 means multi-fragment.
  int get_frag_count() const;
  /// Get the target fragment size.
  uint32_t get_frag_fixed_size() const;
  /// Mark a fragment at index @a idx as written to cache.
  void mark_frag_write(unsigned int idx);
  /// Check if a fragment is cached.
  bool is_frag_cached(unsigned int idx) const;
  /// Get the range of bytes for the fragments from @a low to @a high.
  HTTPRangeSpec::Range get_range_for_frags(int low, int high);

  // Sanity check functions
  static bool check_marshalled(char *buf, int len);

private:
  HTTPInfo(const HTTPInfo &h);
};

inline void
HTTPInfo::destroy()
{
  if (m_alt) {
    if (m_alt->m_flag.writeable_p) {
      m_alt->destroy();
    } else if (m_alt->m_ext_buffer) {
      if (m_alt->m_ext_buffer->refcount_dec() == 0) {
        m_alt->m_ext_buffer->free();
      }
    }
  }
  clear();
}

inline HTTPInfo &
HTTPInfo::operator=(const HTTPInfo &m)
{
  m_alt = m.m_alt;
  return *this;
}

inline CryptoHash const &
HTTPInfo::object_key_get()
{
  return m_alt->m_earliest.m_key;
}

inline void
HTTPInfo::object_key_get(CryptoHash *key)
{
  memcpy(key, &(m_alt->m_earliest.m_key), sizeof(*key));
}

inline bool
HTTPInfo::compare_object_key(const CryptoHash *key)
{
  return *key == m_alt->m_earliest.m_key;
}

inline int64_t
HTTPInfo::object_size_get()
{
  return m_alt->m_earliest.m_offset;
}

inline void
HTTPInfo::object_key_set(CryptoHash const &md5)
{
  m_alt->m_earliest.m_key = md5;
}

inline void
HTTPInfo::object_size_set(int64_t size)
{
  m_alt->m_earliest.m_offset     = size;
  m_alt->m_flag.content_length_p = true;
  // Invariant - if a fragment is cached, all of that fragment is cached.
  // Therefore if the last byte is in the initial cached fragments all of the data is cached.
  if (!m_alt->m_flag.complete_p) {
    int64_t mco = 0; // maximum cached offset + 1
    if (m_alt->m_fragments) {
      if (m_alt->m_fragments->m_cached_idx >= 0)
        mco = this->get_frag_offset(m_alt->m_fragments->m_cached_idx) + this->get_frag_fixed_size();
    } else if (m_alt->m_earliest.m_flag.cached_p) {
      mco = this->get_frag_fixed_size();
    }
    if (mco > size)
      m_alt->m_flag.complete_p = true;
  }
}

inline HTTPInfo::FragmentDescriptorTable *
HTTPInfo::get_frag_table()
{
  return m_alt ? m_alt->m_fragments : 0;
}

inline int
HTTPInfo::get_frag_count() const
{
  return m_alt ? m_alt->m_frag_count : 0;
}

inline uint32_t
HTTPInfo::get_frag_fixed_size() const
{
  return m_alt ? m_alt->m_fixed_fragment_size : 0;
}

inline CryptoHash const &
HTTPInfo::get_frag_key_of(int64_t offset)
{
  return this->get_frag_key(this->get_frag_index_of(offset));
}

inline CryptoHash const &
HTTPInfo::get_frag_key(unsigned int idx)
{
  return 0 == idx ? m_alt->m_earliest.m_key : this->force_frag_at(idx)->m_key;
}

inline bool
HTTPInfo::is_frag_cached(unsigned int idx) const
{
  return m_alt && ((0 == idx && m_alt->m_earliest.m_flag.cached_p) ||
                   (m_alt->m_fragments && idx <= m_alt->m_fragments->m_n && (*m_alt->m_fragments)[idx].m_flag.cached_p));
}

inline HTTPRangeSpec::HTTPRangeSpec() : _state(EMPTY)
{
}

inline void
HTTPRangeSpec::clear()
{
  _state = EMPTY;
  RangeBox().swap(_ranges); // force memory drop.
}

inline bool
HTTPRangeSpec::isSingle() const
{
  return SINGLE == _state;
}

inline bool
HTTPRangeSpec::isMulti() const
{
  return MULTI == _state;
}

inline bool
HTTPRangeSpec::isEmpty() const
{
  return EMPTY == _state;
}

inline bool
HTTPRangeSpec::isUnsatisfied() const
{
  return UNSATISFIABLE == _state;
}

inline size_t
HTTPRangeSpec::count() const
{
  return SINGLE == _state ? 1 : _ranges.size();
}

inline bool
HTTPRangeSpec::hasRanges() const
{
  return SINGLE == _state || MULTI == _state;
}

inline bool
HTTPRangeSpec::isValid() const
{
  return SINGLE == _state || MULTI == _state || EMPTY == _state;
}

inline HTTPRangeSpec::Range &
HTTPRangeSpec::Range::invalidate()
{
  _min = INT64_MAX;
  _max = 1;
  return *this;
}

inline bool
HTTPRangeSpec::Range::isSuffix() const
{
  return 0 == _max && _min > 0;
}

inline HTTPRangeSpec::Range &
HTTPRangeSpec::Range::setSuffix(int64_t size)
{
  _max = 0;
  _min = size;
  return *this;
}

inline bool
HTTPRangeSpec::Range::isPrefix() const
{
  return _min >= 0 && _max == INT64_MAX;
}

inline bool
HTTPRangeSpec::Range::isValid() const
{
  return _min <= _max || this->isSuffix();
}

inline int64_t
HTTPRangeSpec::Range::size() const
{
  return 1 + (_max - _min);
}

inline uint64_t
HTTPRangeSpec::size() const
{
  uint64_t size = 0;
  if (this->isSingle())
    size = _single.size();
  else if (this->isMulti()) {
    for (RangeBox::const_iterator spot = _ranges.begin(), limit = _ranges.end(); spot != limit; ++spot)
      size += spot->size();
  }
  return size;
}

inline bool
HTTPRangeSpec::Range::apply(int64_t len)
{
  ink_assert(len > 0);
  bool zret = true; // is this range satisfiable for @a len?
  if (this->isSuffix()) {
    _max = len - 1;
    _min = _min > len ? 0 : len - _min;
  } else if (_min < len) {
    _max = MIN(_max, len - 1);
  } else {
    this->invalidate();
    zret = false;
  }
  return zret;
}

inline HTTPRangeSpec &
HTTPRangeSpec::add(uint64_t low, uint64_t high)
{
  return this->add(Range(low, high));
}

inline HTTPRangeSpec::Range &HTTPRangeSpec::operator[](int n)
{
  return SINGLE == _state ? _single : _ranges[n];
}

inline HTTPRangeSpec::Range const &HTTPRangeSpec::operator[](int n) const
{
  return SINGLE == _state ? _single : _ranges[n];
}

inline HTTPRangeSpec::iterator
HTTPRangeSpec::begin()
{
  switch (_state) {
  case SINGLE:
    return &_single;
  case MULTI:
    return &(*(_ranges.begin()));
  default:
    return NULL;
  }
}

inline HTTPRangeSpec::iterator
HTTPRangeSpec::end()
{
  switch (_state) {
  case SINGLE:
    return (&_single) + 1;
  case MULTI:
    return &(*(_ranges.end()));
  default:
    return NULL;
  }
}

inline HTTPRangeSpec::const_iterator
HTTPRangeSpec::begin() const
{
  return const_cast<self *>(this)->begin();
}

inline HTTPRangeSpec::const_iterator
HTTPRangeSpec::end() const
{
  return const_cast<self *>(this)->end();
}

inline HTTPRangeSpec::Range
HTTPRangeSpec::getConvexHull() const
{
  Range zret;
  // Compute the convex hull of the original in fragment indices.
  for (const_iterator spot = this->begin(), limit = this->end(); spot != limit; ++spot) {
    if (spot->_min < zret._min)
      zret._min = spot->_min;
    if (spot->_max > zret._max)
      zret._max = spot->_max;
  }
  return zret;
}

inline HTTPCacheAlt::FragmentDescriptor &HTTPCacheAlt::FragmentDescriptorTable::operator[](int idx)
{
  ink_assert(idx > 0);
  return *(reinterpret_cast<FragmentDescriptor *>(reinterpret_cast<char *>(this + 1) + sizeof(FragmentDescriptor) * (idx - 1)));
}

inline size_t
HTTPCacheAlt::FragmentDescriptorTable::calc_size(uint32_t n)
{
  return n < 1 ? 0 : sizeof(FragmentDescriptorTable) + n * sizeof(FragmentDescriptor);
}

#if 0
inline
HTTPCacheAlt::FragmentAccessor::FragmentAccessor(HTTPCacheAlt* alt)
             : _alt(alt), _table(alt->m_fragments)
{
}

inline HTTPCacheAlt::FragmentDescriptor&
HTTPCacheAlt::FragmentAccessor::operator [] (int idx)
{
  ink_assert(idx >= 0);
  return idx == 0 ? _alt->m_earliest : (*_table)[idx];
}

inline uint32_t
HTTPCacheAlt::FragmentAccessor::get_initial_cached_index() const
{
  return _table ? _table->m_cached_idx : 0;
}
#endif

#endif /* __HTTP_H__ */
