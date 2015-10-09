#!/usr/bin/env python
# This file is part of bgpstream
#
# Copyright (C) 2015 The Regents of the University of California.
# Authors: Alistair King, Chiara Orsini, Adriano Faggiani
#
# All rights reserved.
#
# This code has been developed by CAIDA at UC San Diego.
# For more information, contact bgpstream-info@caida.org
#
# This source code is proprietary to the CAIDA group at UC San Diego and may
# not be redistributed, published or disclosed without prior permission from
# CAIDA.
#
# Report any bugs, questions or comments to bgpstream-info@caida.org
#


"""Provides a function to extract a list of host IPs from a list of
possibly overlapping prefixes.
The list of prefixes is read from a file provided as input, containing
a prefix per line. The list of host IPs is provided in the output file
using the following format:
<pfx>  <host ip>
Prefixes that are fully covered are not considered.
"""


import argparse
import os, errno
import radix
import gzip
import bz2
import ipcalc



special_purpose_pfxs_v4 = (
    # https://tools.ietf.org/html/rfc6890
    "0.0.0.0/8",           # This host on this network RFC 1122
    "10.0.0.0/8",          # Private-Use RFC 1918
    "100.64.0.0/10",       # Shared Address Space RFC 6598
    "127.0.0.0/8",         # Loopback RFC 1122
    "169.254.0.0/16",      # Link Local RFC 3927
    "172.16.0.0/12",       # Private-Use RFC 1918
    "192.0.0.0/24",        # IETF Protocol Assignments RFC 6890
    "192.0.0.0/29",        # DS-Lite RFC 6333
    "192.0.2.0/24",        # Documentation (TEST-NET-1) RFC 5737
    "192.88.99.0/24",      # 6to4 Relay Anycast RFC 3068
    "192.168.0.0/16",      # Private-Use RFC 1918
    "198.18.0.0/15",       # Benchmarking RFC 2544
    "198.51.100.0/24",     # Documentation (TEST-NET-2) RFC 5737
    "203.0.113.0/24",      # Documentation (TEST-NET-3) RFC5737
    "240.0.0.0/4",         # Reserved RFC 1112
    "255.255.255.255/32",  # Limited Broadcast RFC 0919
    # IPv4 Multicast Address Assignments RFC 3171
    "224.0.0.0/4",         # Multicast   RFC 3171
    # https://conference.apnic.net/data/37/2014-02-27-prop-109_1393397866.pdf
    # https://www.apnic.net/policy/proposals/prop-109
    "1.0.0.0/24",          # APNIC Labs as Research Prefixes
    "1.1.1.0/24"          # APNIC Labs as Research Prefixes
)


def pfxs_to_ip(input_file, output_file):
    """Extract a list of IP host addresses from a lis of prefixes
    Reads the prefixes from the input file and outputs
    for each prefix, a corresponding host IP address
    (unless the prefix is fully covered by subprefixes)
    """
    # check that the input file and the output folder exist
    if not os.path.exists(input_file):
        print "Error: input file", input_file, "does not exist"
        return
    output_folder = os.path.dirname(os.path.abspath(output_file))
    if not os.path.exists(output_folder):
        print "Error: output folder", output_folder, "does not exist"
        return
    # load reserved prefixes in patricia trie
    reserved_pfxs = radix.Radix()
    for r in special_purpose_pfxs_v4:
        reserved_pfxs.add(r)
    fh = 0
    # open file based on extension
    ext = os.path.splitext(input_file)[-1]
    if ext == ".gz":
        fh = gzip.open(input_file, 'r')
    elif ext == ".bz2":
        fh = bz2.BZ2File(input_file, 'r')
    else:
        fh = open(input_file, 'r')
    # load prefixes in a patricia trie
    patricia_trie = radix.Radix()
    for line in fh:
        pfx = line.rstrip("\n")
        # ignore reserved prefixes
        if reserved_pfxs.search_best(pfx) is not None:
            continue
        # ignore prefixes with short or long mask
        net_length = int(pfx.split("/")[1])
        if net_length < 7 or net_length > 24:
            continue
        patricia_trie.add(pfx)
    fh.close()
    # debug: print patricia_trie.prefixes()
    pfx_ip = dict()
    for node in patricia_trie:
        pfx = node.prefix
        # if an entry exists already, then
        # an IP has been already assigned
        if pfx in pfx_ip:
            continue
        # debug: print pfx
        # get the first and last ip of the network
        network = ipcalc.Network(pfx)
        ip_first = long(network.host_first())
        ip_last = long(network.host_last())
        assigned_ip = ip_first
        while assigned_ip <= ip_last:
            assigned_ip_str = str(ipcalc.IP(assigned_ip))
            # Best-match search will return the longest matching prefix
            # for the current assigned_ip  (routing-style lookup)
            best_match_pfx = patricia_trie.search_best(assigned_ip_str).prefix
            # If the best match for the first IP is the same prefix, then
            # we have found an IP
            if best_match_pfx == pfx:
                pfx_ip[pfx] = assigned_ip_str
                # debug: print pfx, pfx_ip[pfx]
                break
            # if the best_match_pfx has not been assigned
            # an ip yet, then do it
            if best_match_pfx not in pfx_ip:
                pfx_ip[best_match_pfx] = assigned_ip_str
                # debug: print best_match_pfx, pfx_ip[best_match_pfx]
            # in any case move the assigned_ip further:
            # move to the next prefix (having the same length as the
            # more specific found): the next network begins with
            # broadcast_long()) + 1 and has the same netmask
            # (note tha the first host ip may not be broadcast + 1)
            matching_network = ipcalc.Network(best_match_pfx)
            net = best_match_pfx.split("/")[1]
            next_pfx = str(ipcalc.IP(long(matching_network.broadcast_long()) + 1)) + "/" + net
            next_network = ipcalc.Network(next_pfx)
            assigned_ip = long(next_network.host_first())
    ext = os.path.splitext(output_file)[-1]
    if ext == ".gz":
        fh = gzip.open(output_file, 'wb')
    elif ext == ".bz2":
        fh = bz2.BZ2File(output_file, 'wb')
    else:
        fh = open(output_file, 'wb')
    for pfx in pfx_ip:
        fh.write("\t".join([pfx, pfx_ip[pfx]]) + "\n")
    fh.close()



# code to execute if called from command-line
if __name__ == "__main__": 
    parser = argparse.ArgumentParser()
    parser.add_argument("-i","--input", help="input file (required)",
                    default=None, action='store',type=str)
    parser.add_argument("-o","--output", help="output file (required)",
                    default=None, action='store',type=str)
    args = parser.parse_args()
    if not args.input:
        print "Error: input file required"
        parser.print_help()
    elif not args.output:
        print "Error: output file required"
        parser.print_help()
    else:
        # run pfxs to ip function
        pfxs_to_ip(args.input, args.output)
        
