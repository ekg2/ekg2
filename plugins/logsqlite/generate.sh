#!/bin/sh

sqlite logsqlite.db "CREATE TABLE log_msg (
   session text,
   uid text,
   nick text,
   type text,
   sent boolean,
   ts timestamp,
   sentts timestamp,
   body text)"

sqlite logsqlite.db "CREATE TABLE log_status (
   session text,
   uid text,
   nick text,
   ts timestamp,
   status text,
   desc text)"

