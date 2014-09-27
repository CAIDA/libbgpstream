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

def parse_bd_line(line, bd_list):
    bd_list.append(line)
    pass    

def parse_bs_line(line, bs_list):
    bs_list.append(line)
    pass    


def main():    
    bd_lines_cnt = 0 # number of lines counted in the last bgpdump section
    bd_section_read = 0 # 1 if we have read an entire bgpdump section
    bd_list = []
    bs_lines_cnt = 0 # number of lines counted in the last bgpstream section
    bs_section_read = 0 # 1 if we have read an entire bgpstream section
    bs_list = []
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
            parse_bs_line(line, bs_list)
        else:
            # it is a bgpdump line
            if(bs_lines_cnt > 0 and bd_section_read == 1):
                bs_section_read = 1
                print "comparison here: " + str(bd_lines_cnt) + " " + str(bs_lines_cnt)
                print "comparison here: " + str(len(bd_list)) + " " + str(len(bs_list))
                # then we reset values
                bs_section_read = bd_section_read = 0
                bs_lines_cnt = bd_lines_cnt = 0
                bs_list = bd_list = []
            # in any case we process the bgpdump lines
            bd_lines_cnt+=1
            parse_bd_line(line, bd_list)
        print line
    



if __name__ == "__main__":
    main()




