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

#include <string.h>
#include <stdio.h>
#include "ink_code.h"
#include "INK_MD5.h"
#include "ink_assert.h"
#include "INK_MD5.h"

ats::CryptoHash const ats::CRYPTO_HASH_ZERO; // default constructed is correct.

MD5Context::MD5Context() {
  MD5_Init(&_ctx);
}

bool
MD5Context::update(void const* data, int length) {
  return 0 != MD5_Update(&_ctx, data, length);
}

bool
MD5Context::finalize(CryptoHash& hash) {
  return 0 != MD5_Final(hash.u8, &_ctx);
}

/**
  @brief Wrapper around MD5_Init
*/
int
ink_code_incr_md5_init(INK_DIGEST_CTX * context) {
  return MD5_Init(context);
}

/**
  @brief Wrapper around MD5_Update
*/
int
ink_code_incr_md5_update(INK_DIGEST_CTX * context, const char *input, int input_length) {
  return MD5_Update(context, input, input_length);
}

/**
  @brief Wrapper around MD5_Final
*/
int
ink_code_incr_md5_final(char *sixteen_byte_hash_pointer, INK_DIGEST_CTX * context) {
  return MD5_Final((unsigned char*)sixteen_byte_hash_pointer, context);
}

/**
  @brief Helper that will init, update, and create a final MD5

  @return always returns 0, maybe some error checking should be done
*/
int
ink_code_md5(unsigned char const* input, int input_length, unsigned char *sixteen_byte_hash_pointer)
{
  MD5_CTX context;

  MD5_Init(&context);
  MD5_Update(&context, input, input_length);
  MD5_Final(sixteen_byte_hash_pointer, &context);

  return (0);
}

/**
  @brief Converts a MD5 to a null-terminated string

  Externalizes an INK_MD5 as a null-terminated string into the first argument.
  Side Effects: none
  Reentrancy:     n/a.
  Thread Safety:  safe.
  Mem Management: stomps the passed dest char*.

  @return returns the passed destination string ptr.
*/
/* reentrant version */
char *
ink_code_md5_stringify(char *dest33, const size_t destSize, const char *md5)
{
  ink_assert(destSize >= 33);

  int i;
  for (i = 0; i < 16; i++) {
    // we check the size of the destination buffer above
    // coverity[secure_coding]
    sprintf(&(dest33[i * 2]), "%02X", md5[i]);
  }
  ink_assert(dest33[32] == '\0');
  return (dest33);
}                               /* End ink_code_stringify_md5(const char *md5) */

/**
  @brief Converts a MD5 to a null-terminated string

  Externalizes an INK_MD5 as a null-terminated string into the first argument.
  Does so without intenal procedure calls.
  Side Effects: none.
  Reentrancy:     n/a.
  Thread Safety:  safe.
  Mem Management: stomps the passed dest char*.

  @return returns the passed destination string ptr.
*/
/* reentrant version */
char *
ink_code_to_hex_str(char *dest33, uint8_t const* hash)
{
  int i;
  char *d;

  static char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

  d = dest33;
  for (i = 0; i < 16; i += 4) {
    *(d + 0) = hex_digits[hash[i + 0] >> 4];
    *(d + 1) = hex_digits[hash[i + 0] & 15];
    *(d + 2) = hex_digits[hash[i + 1] >> 4];
    *(d + 3) = hex_digits[hash[i + 1] & 15];
    *(d + 4) = hex_digits[hash[i + 2] >> 4];
    *(d + 5) = hex_digits[hash[i + 2] & 15];
    *(d + 6) = hex_digits[hash[i + 3] >> 4];
    *(d + 7) = hex_digits[hash[i + 3] & 15];
    d += 8;
  }
  *d = '\0';
  return (dest33);
}

namespace ats {

  uint8_t const CRYPTO_HASH_NEXT_TABLE[256] = {
    21, 53, 167, 51, 255, 126, 241, 151,
    115, 66, 155, 174, 226, 215, 80, 188,
    12, 95, 8, 24, 162, 201, 46, 104,
    79, 172, 39, 68, 56, 144, 142, 217,
    101, 62, 14, 108, 120, 90, 61, 47,
    132, 199, 110, 166, 83, 125, 57, 65,
    19, 130, 148, 116, 228, 189, 170, 1,
    71, 0, 252, 184, 168, 177, 88, 229,
    242, 237, 183, 55, 13, 212, 240, 81,
    211, 74, 195, 205, 147, 93, 30, 87,
    86, 63, 135, 102, 233, 106, 118, 163,
    107, 10, 243, 136, 160, 119, 43, 161,
    206, 141, 203, 78, 175, 36, 37, 140,
    224, 197, 185, 196, 248, 84, 122, 73,
    152, 157, 18, 225, 219, 145, 45, 2,
    171, 249, 173, 32, 143, 137, 69, 41,
    35, 89, 33, 98, 179, 214, 114, 231,
    251, 123, 180, 194, 29, 3, 178, 31,
    192, 164, 15, 234, 26, 230, 91, 156,
    5, 16, 23, 244, 58, 50, 4, 67,
    134, 165, 60, 235, 250, 7, 138, 216,
    49, 139, 191, 154, 11, 52, 239, 59,
    111, 245, 9, 64, 25, 129, 247, 232,
    190, 246, 109, 22, 112, 210, 221, 181,
    92, 169, 48, 100, 193, 77, 103, 133,
    70, 220, 207, 223, 176, 204, 76, 186,
    200, 208, 158, 182, 227, 222, 131, 38,
    187, 238, 6, 34, 253, 128, 146, 44,
    94, 127, 105, 153, 113, 20, 27, 124,
    159, 17, 72, 218, 96, 149, 213, 42,
    28, 254, 202, 40, 117, 82, 97, 209,
    54, 236, 121, 75, 85, 150, 99, 198,
  };

  uint8_t const CRYPTO_HASH_PREV_TABLE[256] = {
    57, 55, 119, 141, 158, 152, 218, 165,
    18, 178, 89, 172, 16, 68, 34, 146,
    153, 233, 114, 48, 229, 0, 187, 154,
    19, 180, 148, 230, 240, 140, 78, 143,
    123, 130, 219, 128, 101, 102, 215, 26,
    243, 127, 239, 94, 223, 118, 22, 39,
    194, 168, 157, 3, 173, 1, 248, 67,
    28, 46, 156, 175, 162, 38, 33, 81,
    179, 47, 9, 159, 27, 126, 200, 56,
    234, 111, 73, 251, 206, 197, 99, 24,
    14, 71, 245, 44, 109, 252, 80, 79,
    62, 129, 37, 150, 192, 77, 224, 17,
    236, 246, 131, 254, 195, 32, 83, 198,
    23, 226, 85, 88, 35, 186, 42, 176,
    188, 228, 134, 8, 51, 244, 86, 93,
    36, 250, 110, 137, 231, 45, 5, 225,
    221, 181, 49, 214, 40, 199, 160, 82,
    91, 125, 166, 169, 103, 97, 30, 124,
    29, 117, 222, 76, 50, 237, 253, 7,
    112, 227, 171, 10, 151, 113, 210, 232,
    92, 95, 20, 87, 145, 161, 43, 2,
    60, 193, 54, 120, 25, 122, 11, 100,
    204, 61, 142, 132, 138, 191, 211, 66,
    59, 106, 207, 216, 15, 53, 184, 170,
    144, 196, 139, 74, 107, 105, 255, 41,
    208, 21, 242, 98, 205, 75, 96, 202,
    209, 247, 189, 72, 69, 238, 133, 13,
    167, 31, 235, 116, 201, 190, 213, 203,
    104, 115, 12, 212, 52, 63, 149, 135,
    183, 84, 147, 163, 249, 65, 217, 174,
    70, 6, 64, 90, 155, 177, 185, 182,
    108, 121, 164, 136, 58, 220, 241, 4,
  };

}
