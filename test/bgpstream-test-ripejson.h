/*
 * Copyright (C) 2015 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

const char *valid_output[] = {
"U|A|1533666470.700000|singlefile|rrc13|||28917|2001:7f8:20:101::208:147|2804:45d0::/32|2001:7f8:20:101::208:147|28917 6939 266951|266951|28917:4000 28917:4100 28917:4101||",
"U|W|1534175898.170000|singlefile|rrc21|||198290|37.49.236.123|185.124.96.0/22||||||",
"U|W|1534175898.170000|singlefile|rrc21|||198290|37.49.236.123|185.166.180.0/22||||||",
"U|S|1533938856.529999|singlefile|rrc00|||200358|89.163.209.32|||||||OPENSENT",
"U|S|1533938856.529999|singlefile|rrc00|||200358|89.163.209.32|||||||OPENCONFIRM",
"U|S|1534175211.490000|singlefile|rrc21|||49375|2001:7f8:54::201|||||||ESTABLISHED",
"U|S|1534175193.450000|singlefile|rrc21|||31122|37.49.237.31|||||||IDLE",
"",
"",
"",
};

// WARN: unsupported ris-stream message: {"timestamp":1534175211.49,"state":"connected","host":"rrc21","peer":"2001:7f8:54::201","type":"N","id":"2001_7f8_54__201-23b86597fd-2bbf6","peer_asn":"49375"}
// WARN: unsupported ris-stream message: {"timestamp:1534175211.49,"state":"connected","host":"rrc21","peer":"2001:7f8:54::201","type":"N","id":"2001_7f8_54__201-23b86597fd-2bbf6","peer_asn":"49375"}
// WARN: unsupported ris-stream message: {"body":"000816B97C6016B9A6B40000","host":"rrc21","peer":"37.49.236.123","timestamp":1534175898.17,"prefix":"185.124.96.0/22","type":"W","id":"JTHsew-23b866a439-14a9a93","peer_asn":"198290"} {"body":"000816B97C6016B9A6B40000","host":"rrc21","peer":"37.49.236.123","timestamp":1534175898.17,"prefix":"185.124.96.0/22","type":"W","id":"JTHsew-23b866a439-14a9a93","peer_asn":"198290"}
// U|S|0.000000|ris-stream|rrc21|||31122|37.49.237.31|||||||IDLE


#define SETUP                                                                  \
  do {                                                                         \
    bs = bgpstream_create();                                                   \
  } while (0)

#define CHECK_SET_INTERFACE(interface)                                         \
  do {                                                                         \
    di_id = bgpstream_get_data_interface_id_by_name(bs, STR(interface));       \
    bgpstream_set_data_interface(bs, di_id);                                   \
  } while (0)
