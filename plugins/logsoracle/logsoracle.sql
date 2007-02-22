--
--  (C) Copyright 2006 Szymon Bilinski <ecimon(at)babel.pl>
--
--  This program is free software; you can redistribute it and/or modify
--  it under the terms of the GNU Lesser General Public License Version
--  2.1 as published by the Free Software Foundation.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU Lesser General Public License for more details.
--
--  You should have received a copy of the GNU Lesser General Public
--  License along with this program; if not, write to the Free Software
--  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
--


-- Table for status changes (session and protocol)
CREATE TABLE status_changes (
    id		NUMBER PRIMARY KEY,
    session_uid	VARCHAR2(4000) NOT NULL,
    changes_uid	VARCHAR2(4000) NOT NULL,
    status	VARCHAR2(4000),
    descr	VARCHAR2(4000),
    change_time	DATE NOT NULL
);


-- Table for logged messages
CREATE TABLE messages (
    id		NUMBER	PRIMARY KEY,
    session_uid VARCHAR2(4000) NOT NULL,
    sender_uid	VARCHAR2(4000) NOT NULL,
    content	VARCHAR2(4000),
    recv_time	DATE NOT NULL
);
 
-- Table with recipients (join this with messages to get full info)
CREATE TABLE recipients (
    id			NUMBER PRIMARY KEY,
    recipient_uid	VARCHAR2(4000) NOT NULL,
    msg_id		NUMBER NOT NULL REFERENCES messages
);

-- Indexes
CREATE INDEX recipients_msg_index ON recipients(msg_id);

-- Sequences
CREATE SEQUENCE status_changes_seq
    MINVALUE 1
    START WITH 1
    INCREMENT BY 1;

CREATE SEQUENCE messages_seq
    MINVALUE 1
    START WITH 1
    INCREMENT BY 1;

CREATE SEQUENCE recipients_seq
    MINVALUE 1
    START WITH 1
    INCREMENT BY 1;
    
-- Views
CREATE VIEW archive_msg AS
    SELECT m.session_uid, m.sender_uid, r.recipient_uid, TO_CHAR(m.recv_time, 'HH24:MI:SS, DD-MM-YYYY') as recv_time, m.content 
    FROM messages m, recipients r
    WHERE m.id = r.msg_id
    ORDER BY recv_time;
\