#!/usr/bin/env python

import sqlite3
import argparse



def create_tables(db_conn):
    c = db_conn.cursor()    
    c.execute('''SELECT name FROM sqlite_master WHERE type='table' AND name='bgp_data' ''')
    if len(c.fetchall()) == 0:
        c.execute('''CREATE TABLE bgp_data
                 (collector_id integer,
                 type_id integer,
                 file_time timestamp,
                 file_path text,
                 ts timestamp default (strftime('%s', 'now')),
                 PRIMARY KEY(collector_id, type_id, file_time))''')
    c.execute('''SELECT name FROM sqlite_master WHERE type='table' AND name='collectors' ''')
    if len(c.fetchall()) == 0:
        c.execute('''CREATE TABLE collectors
                 (id integer PRIMARY KEY,
                 project text,
                 name text)''')
    c.execute('''SELECT name FROM sqlite_master WHERE type='table' AND name='bgp_types' ''')
    if len(c.fetchall()) == 0:
        c.execute('''CREATE TABLE bgp_types
                 (id integer PRIMARY KEY,
                 name text)''')
        c.execute("INSERT INTO bgp_types VALUES ('1','ribs')")
        c.execute("INSERT INTO bgp_types VALUES ('2','updates')")
    c.execute('''SELECT name FROM sqlite_master WHERE type='table' AND name='time_span' ''')
    if len(c.fetchall()) == 0:
        c.execute('''CREATE TABLE time_span
                 (collector_id integer,
                 bgp_type_id integer,
                 time_span integer,
                 PRIMARY KEY(collector_id, bgp_type_id))''')
    db_conn.commit()


def create_new_collector(db_conn, project_name, collector_name, ribs_span, updates_span):
    c = db_conn.cursor()
    col_id = 0
    # get the collector id (or create a new one if it doesn't exist)
    c.execute('''SELECT id FROM collectors WHERE project=? AND name=?''',
              [project_name, collector_name])
    res = c.fetchone()
    if res is None:
        c.execute('''SELECT count(*) FROM collectors''')
        col_id = c.fetchone()[0] + 1
        c.execute('''INSERT INTO collectors VALUES(?,?,?)''',
              [col_id, project_name, collector_name])
    else:
        col_id = res[0]
    # insert or replace time span information
    c.execute('''INSERT OR REPLACE INTO time_span VALUES(?,1,?)''',
        [col_id, ribs_span])
    c.execute('''INSERT OR REPLACE INTO time_span VALUES(?,2,?)''',
        [col_id, updates_span])
    db_conn.commit()


def add_new_bgp_data(db_conn, project_name, collector_name, bgp_type, file_time, file_path):
    c = db_conn.cursor()
    # check collector exists
    c.execute('''SELECT id FROM collectors WHERE project=? AND name=?''',
              [project_name, collector_name])
    res = c.fetchone()
    if res is None:
        print "Collector " + project_name + "." + collector_name + " does not  exist!"
        return 0
    else:
        col_id = res[0]
    # if bgp_type != "ribs" and bgp_type != "updates":
    #     print "Type " + bgp_type + " does not exist!"
    #     return 0
    c.execute('''SELECT id FROM bgp_types WHERE name=? ''', [bgp_type])
    res = c.fetchone()
    if not res[0]:
        print "bgp type " + bgp_type + " not supported!"
    else:
        type_id = res[0]
    # insert or replace bgp_data information
    c.execute('''INSERT OR REPLACE INTO bgp_data
             (collector_id, type_id, file_time, file_path)
             VALUES(?,?,?,?)''', [col_id, type_id, file_time, file_path])
    db_conn.commit()
    return 1


def all_files_count(db_conn):
    c = db_conn.cursor()
    c.execute('''SELECT collectors.project, collectors.name,
                        bgp_types.name, time_span.time_span,
                        bgp_data.file_time, bgp_data.file_path, bgp_data.ts
              FROM  collectors JOIN bgp_data JOIN bgp_types JOIN time_span
              WHERE bgp_data.collector_id = collectors.id  AND
                    bgp_data.collector_id = time_span.collector_id AND
                    bgp_data.type_id = bgp_types.id AND
                    bgp_data.type_id = time_span.bgp_type_id
                    ''')
    res = c.fetchall()
    return len(res)


def list_files(db_conn, time_start, time_end, projects, collectors, bgp_types):
    all_n = all_files_count(db_conn)
    c = db_conn.cursor()
    query = '''SELECT collectors.project, collectors.name,
                        bgp_types.name, time_span.time_span,
                        bgp_data.file_time, bgp_data.file_path, bgp_data.ts
              FROM  collectors JOIN bgp_data JOIN bgp_types JOIN time_span
              WHERE bgp_data.collector_id = collectors.id  AND
                    bgp_data.collector_id = time_span.collector_id AND
                    bgp_data.type_id = bgp_types.id AND
                    bgp_data.type_id = time_span.bgp_type_id AND
                    bgp_data.file_time >= ? AND bgp_data.file_time <= ?'''
    if projects[0] and len(projects) > 0:
        query += " \n AND collectors.project in(" + ",".join(["'%s'" % x for x in projects.split(",")]) + ")"
    if collectors[0] and len(collectors) > 0:
        query += " \n AND collectors.name in(" + ",".join(["'%s'" % x for x in collectors.split(",")]) + ")"
    if bgp_types[0] and len(bgp_types) > 0:
        query += " \n AND bgp_types.name in(" + ",".join(["'%s'" % x for x in bgp_types.split(",")]) + ")"
    c.execute(query, (time_start, time_end))
    res = c.fetchall()
    print "Matching files: " + str(len(res)) + "/" + str(all_n)
    for line in res:
        print line



parser = argparse.ArgumentParser()
parser.add_argument("sqlite_db", help="file containing the sqlite database",
                   type=str)
parser.add_argument("-l","--list_files", help="list the mrt files that match the filters",
                     action="store_true")
parser.add_argument("-C","--add_collector", help="add a new collector to the database",
                     action="store_true")
parser.add_argument("-M","--add_mrt_file", help="add this mrt file to the database",
                     default=[None], action="store",type=str)
parser.add_argument("-p","--proj", help="bgp project(s) to select (provide a comma separated list to select multiple)",
                    default=[None], action='store',type=str)
parser.add_argument("-c","--coll", help="bgp collector(s) to select (provide a comma separated list to select multiple)",
                    default=[None], action='store',type=str)
parser.add_argument("-R","--ribs_time_span", help="rib time span",
                    default=None, action='store',type=int)
parser.add_argument("-U","--updates_time_span", help="updates time span",
                    default=None, action='store',type=int)
parser.add_argument("-t","--bgp_type", help="bgp type(s) to select (provide a comma separated list to select multiple)",
                    default=[None], action='store',type=str)
parser.add_argument("-T","--file_time", help="time associated with the mrt file",
                    default=-1, action='store',type=int)
parser.add_argument("-b","--begin", help="bgp time begin",
                    default=0, action='store',type=int)
parser.add_argument("-e","--end", help="bgp time end",
                    default=2147483647, action='store',type=int)

args = parser.parse_args()

# connect to the database
conn = sqlite3.connect(args.sqlite_db)

# create tables (if they do not exist)
create_tables(conn)

action_result = 1

if args.list_files:
    # output the list of files that match the filters
    list_files(conn, args.begin, args.end, (args.proj), (args.coll), (args.bgp_type))

# Add a new collector to the database    
if args.add_collector:
    action_result = 0
    if args.proj[0] and len((args.proj).split(",")) == 1:
        if args.coll[0] and len((args.coll).split(",")) == 1:
            if args.ribs_time_span and args.updates_time_span:
                create_new_collector(conn, args.proj, args.coll, args.ribs_time_span, args.updates_time_span)
                action_result = 1
    if action_result == 0:
        print "Could not add collector"

# Add a new mrt file to the database
if args.add_mrt_file[0]:
    action_result = 0
    if args.proj[0] and len(args.proj.split(",")) == 1:
            if args.coll[0] and len((args.coll).split(",")) == 1:
                if args.bgp_type[0] and len((args.bgp_type).split(",")) == 1:
                    if args.file_time >= 0:
                        action_result = add_new_bgp_data(conn, args.proj, args.coll,
                                        args.bgp_type, args.file_time, args.add_mrt_file)
    if action_result == 0:
        print "Could not add mrt file"


# # We can also close the connection if we are done with it.
# # Just be sure any changes have been committed or they will be lost.
conn.close()

