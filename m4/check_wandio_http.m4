 SYNOPSIS
#
#   CHECK_WANDIO_HTTP
#
# DESCRIPTION
#
#   This macro checks that WANDIO HTTP read support actually works.
#
# LICENSE
#
# Copyright (C) 2015 The Regents of the University of California.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

AC_DEFUN([CHECK_WANDIO_HTTP],
[
  AC_MSG_CHECKING([for wandio HTTP support])

  LIBS_STASH=$LIBS
  LIBS="-lwandio"
  # here we have some version of wandio, check that it has http support
  AC_TRY_RUN([
    #include <wandio.h>
    int main() {
      io_t *file = wandio_create($1);
      return (file == NULL);
    }
  ],
  [with_wandio_http=yes],
  [AC_MSG_ERROR(
      [wandio HTTP support required. Ensure you have a working Internet connection and that libcurl is installed before building wandio.]
    )
  ])
  LIBS=$LIBS_STASH

  AC_MSG_RESULT([$with_wandio_http])
])
