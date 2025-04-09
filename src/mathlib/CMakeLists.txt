# Code contained in mathlib directories is from musl-1.2.5
#
# musl as a whole is licensed under the following standard MIT license:
# ----------------------------------------------------------------------
# Copyright © 2005-2020 Rich Felker, et al.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# ----------------------------------------------------------------------

file(GLOB MATHLIB_SRC *.c)

list(REMOVE_ITEM MATHLIB_SRC fmaf.c)

# Skip some unneeded functions that depend on atomics or other unimplemented features
list(FILTER MATHLIB_SRC EXCLUDE REGEX ".*fma.*\.c")
list(FILTER MATHLIB_SRC EXCLUDE REGEX ".*lrint.*\.c")
list(FILTER MATHLIB_SRC EXCLUDE REGEX ".*nearbyint.*\.c")

target_sources(quake PRIVATE
        ${MATHLIB_SRC}
)
