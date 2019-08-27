# SYNOPSIS
#
#   CHECK_LIBRDKAFKA_VERSION
#
# DESCRIPTION
#
#   This macro checks that librdkafka is present and new enough
#
# LICENSE
#
# Copyright (C) 2019 The Regents of the University of California.
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
# CHECK_LIBRDKAFKA_VERSION(1.0.0, 0x000b00ff)
AC_DEFUN([CHECK_LIBRDKAFA_VERSION],
[
  AC_CHECK_LIB([rdkafka], [rd_kafka_version], ,
    [AC_MSG_ERROR(
      [librdkafka >= $1 is required for kafka support (--without-kafka to disable)]
     )]
  )

  AC_MSG_CHECKING([for librdkafka >= $1])

  # there is some version of librdkafka installed, check that it is what we want
  AC_RUN_IFELSE([AC_LANG_PROGRAM([
    #include <librdkafka/rdkafka.h>
    #include <stdio.h>
    ],[
      int version = rd_kafka_version();
      if (version >= $2) {
         fprintf(stderr, "librdkafka version %s ok\n", rd_kafka_version_str());
         return 0;
      } else {
        fprintf(stderr, "librdkafka version %s too old (need >= $1)\n",
                rd_kafka_version_str());
        return rd_kafka_version();
      }
  ])],
  [rdkafka_version=$?],
  [AC_MSG_ERROR([installed librdkafka is too old. librdkafka >= $1 required. check config.log for more details (use --without-kafka to disable kafka support)])])

  AC_MSG_RESULT([ok])
])
