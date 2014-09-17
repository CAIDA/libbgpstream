#!/bin/sh

git --no-pager log --format="%ai %aN %n%n%x09* %s%d%n" > ChangeLog;
