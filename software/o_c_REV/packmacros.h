// Copyright (c) 2021, Bryan Head
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/**
 * A collection of handy X-macros for safe and convenient data packing.
 * Expects a macro that looks like the following:
 * #define PACK_SPEC(PACK_OP) \
 *   PACK_OP(foo, foo_bits) \
 *   PACK_OP(bar, bar_bits) \
 *   PACK_OP(baz, baz_bits)
 *
 * where foo, bar, baz are the variables you want to pack and unpack into and
 * *_bits is how many bits they should occupy. Good idea to #undef PACK_SPEC at
 * the end of the class.
**/

#define _PACKMACROS_PACK(var, bits) Pack(data, PackLocation { pos, bits }, var); pos += bits;
#define _PACKMACROS_UNPACK(var, bits) var = Unpack(data, PackLocation { pos, bits }); pos += bits;
#define _PACKMACROS_SUM_BITS(var, bits) bits +

// Define OnDataReceive based on variables and bit values in PACK_SPEC.
#define ON_DATA_RECEIVE(PACK_SPEC) \
uint32_t OnDataRequest() { \
  uint32_t data = 0; \
  int pos = 0; \
  PACK_SPEC(_PACKMACROS_PACK) \
  return data; \
}

// Define OnDataRequest based on variables and bit values in PACK_SPEC.
#define ON_DATA_REQUEST(PACK_SPEC) \
void OnDataReceive(uint32_t data) { \
  int pos = 0; \
  PACK_SPEC(_PACKMACROS_UNPACK) \
}

// Ensures at compile time that the total space declared by PACK_SPEC does not exceed 32.
#define CHECK_PACK_SIZE(PACK_SPEC) \
  static_assert(PACK_SPEC(_PACKMACROS_SUM_BITS) 0 <= 32, "Can only pack up to 32 bits");


