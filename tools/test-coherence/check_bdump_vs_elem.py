#!/usr/bin/python
#
# bgpstream
#
# Chiara Orsini, CAIDA, UC San Diego
# chiara@caida.org
#
# Copyright (C) 2014 The Regents of the University of California.
#
# This file is part of bgpstream.
#
# bgpstream is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# bgpstream is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with bgpstream.  If not, see <http://www.gnu.org/licenses/>.
#


import fileinput

# http://stackoverflow.com/questions/354038/how-do-i-check-if-a-string-is-a-number-in-python
def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        return False

num_to_state = {"0": "unknown",
                "1": "idle",
                "2": "connect",
                "3": "active",
                "4": "opensent",
                "5": "openconfirm",
                "6": "established",
                "7": "null"}



def parse_bd_line(message_list, bd_list):
    entry = dict()
    entry['ts'] = message_list[1]
    entry['peer_ip'] = message_list[3]
    entry['peer_as'] = message_list[4]
    entry['type'] = message_list[2]
    if(entry['type'] == "B"):
        entry['type'] = "rib"
        entry['prefix'] = message_list[5]
        entry['aspath'] = message_list[6]
        #if(entry['aspath'] == "! Error !"):
        #    entry['aspath'] = ''
        entry['next_hop'] = message_list[8]
    if(entry['type'] == "A"):
        entry['type'] = "announcement"
        entry['prefix'] = message_list[5]
        entry['aspath'] = message_list[6]
        if(entry['aspath'] == "! Error !"):
            entry['aspath'] = ''
        entry['next_hop'] = message_list[8]
    if(entry['type'] == "W"):
        entry['type'] = "withdrawal"
        entry['prefix'] = message_list[5]
    if(entry['type'] == "STATE"):
        entry['type'] = "state"
        entry['old_state'] = num_to_state[message_list[5]]
        entry['new_state'] = num_to_state[message_list[6]]
    bd_list.append(entry)

def parse_bs_line(message_list, bs_list):
    entry = dict()
    entry['ts'] = message_list[0]
    entry['peer_ip'] = message_list[1]
    entry['peer_as'] = message_list[2]
    entry['type'] = message_list[3]
    if(entry['type'] == "rib" or entry['type'] == "announcement"):
        entry['prefix'] = message_list[4]
        entry['next_hop'] = message_list[5]
        entry['aspath'] = message_list[6]
    if(entry['type'] == "withdrawal"):
        entry['prefix'] = message_list[4]
    if(entry['type'] == "state"):
        entry['old_state'] = message_list[4]
        entry['new_state'] = message_list[5]
    bs_list.append(entry)


    # http://stackoverflow.com/questions/4527942/comparing-two-dictionaries-in-python
def equal_bgp_lists(bd_list,bs_list):
    if(len(bd_list) != len(bs_list)):
        return False
    # check all bs are in bd
    equal=0
    for x in bs_list:
        for y in bd_list:
            if(len(set(x.items()) ^ set(y.items())) == 0):
                equal+=1
                break
    if(equal == len(bs_list)):
        return True
    return False

        
def main():    
    bd_lines_cnt = 0 # number of lines counted in the last bgpdump section
    bd_section_read = 0 # 1 if we have read an entire bgpdump section
    bd_list = []
    bd_lines = []
    bd_headers = ["TABLE_DUMP","TABLE_DUMP2","BGP4MP"]
    print bd_headers
    bs_lines_cnt = 0 # number of lines counted in the last bgpstream section
    bs_section_read = 0 # 1 if we have read an entire bgpstream section
    bs_list = []
    bs_lines = []
    for line in fileinput.input():  
        # chomp line
        line = line.rstrip('\n')
        message_list = line.rstrip('\n').split('|')
        if(is_number(message_list[0])):
            # it is a bgpstream line
            if(bd_lines_cnt > 0):
                # we are reading the first bgpstream line after a bgpdump section
                bd_section_read = 1
            # in any case we update the bs_lines_cnt and process them
            bs_lines_cnt+=1
            bs_lines.append(line)
            parse_bs_line(message_list, bs_list)
        elif(message_list[0] in bd_headers):
            # it is a bgpdump line
            if(bs_lines_cnt > 0 and bd_section_read == 1):
                bs_section_read = 1
                if(equal_bgp_lists(bd_list,bs_list)):
                    #print "OK!"
                    pass
                else:
                    print "WRONG!"
                    print bd_list
                    print bs_list
                    print bd_lines
                    print bs_lines
                # then we reset values
                bs_section_read = 0
                bd_section_read = 0
                bs_lines_cnt = 0
                bd_lines_cnt = 0
                bs_list = []
                bs_lines = []
                bd_list = []
                bd_lines = []
            # in any case we process the bgpdump lines
            bd_lines_cnt+=1
            parse_bd_line(message_list, bd_list)
            bd_lines.append(line)
        #print line
        
    # last comparison (end of file
    if(equal_bgp_lists(bd_list,bs_list)):
        #print "OK!"
        pass
    else:
        print "Last WRONG!"
        print bd_list
        print bs_list
        print bd_lines
        print bs_lines
    



if __name__ == "__main__":
    main()




