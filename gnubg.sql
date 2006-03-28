--
-- gnubg.sql
--
-- by Joern Thyssen <jth@gnubg.org>, 2004.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of version 2 of the GNU General Public License as
-- published by the Free Software Foundation.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
--
-- $Id$
--

-- Table: control
-- Holds the most current value of xxx_id for every table.

CREATE TABLE control (
    tablename     CHAR(80) NOT NULL
   ,next_id       INTEGER NOT NULL
   ,PRIMARY KEY (tablename)
);

CREATE UNIQUE INDEX icontrol ON control (
    tablename
);

-- Table: env (nobody can spell 'environment' :-)) 
-- Holds the places
-- where nicks might be found. This allows different players having
-- identical nicks on different servers.

CREATE TABLE env (
    env_id       INTEGER NOT NULL
   -- Textual description of the environment, e.g., "FIBS"
   ,place        CHAR(80) NOT NULL 
   ,PRIMARY KEY (env_id)
);

CREATE UNIQUE INDEX ienv ON env(
    env_id 
);

-- Table: person
-- One person, which can have one or more nicknames

CREATE TABLE person (
    person_id      INTEGER NOT NULL
   -- Name of person
   ,name          CHAR(80) NOT NULL
   -- Misc notes about this person
   ,notes         VARCHAR(1000) NOT NULL
   ,PRIMARY KEY (person_id)
);

CREATE UNIQUE INDEX iperson ON person (
    person_id
);

-- Table: nick
-- Nicknames, combining a player and an env

CREATE TABLE nick (
   nick_id INTEGER NOT NULL
   ,env_id INTEGER NOT NULL
   ,person_id INTEGER NOT NULL
   -- Nickname of player
   ,name          CHAR(80) NOT NULL
   ,PRIMARY KEY (nick_id)
   ,FOREIGN KEY (env_id) REFERENCES env (env_id)
      ON DELETE RESTRICT
   ,FOREIGN KEY (person_id) REFERENCES person (person_id)
      ON DELETE RESTRICT
);

CREATE UNIQUE INDEX inick ON nick (
    nick_id
);

-- Table: match

CREATE TABLE match (
    match_id        INTEGER NOT NULL
   ,checksum        CHAR(33) NOT NULL
   -- Player 0
   ,nick_id0      INTEGER NOT NULL 
   -- Player 1
   ,nick_id1      INTEGER NOT NULL 
   -- The result of the match/session:
   -- - the total number of points won or lost
   -- - +1/0/-1 for player 0 won the match, match not complete, and
   --   player 1 won match, respectively
   ,result          INTEGER NOT NULL
   -- Length of match
   -- 0=session, >0=match
   ,length          INTEGER NOT NULL
   -- Timestamp for insert into database
   ,added           TIMESTAMP NOT NULL
   -- Match info
   ,rating0         CHAR(80) NOT NULL
   ,rating1         CHAR(80) NOT NULL
   ,event           CHAR(80) NOT NULL
   ,round           CHAR(80) NOT NULL
   ,place           CHAR(80) NOT NULL
   ,annotator       CHAR(80) NOT NULL
   ,comment         CHAR(80) NOT NULL
   ,date            DATE 
   ,PRIMARY KEY (match_id)
   ,FOREIGN KEY (nick_id0) REFERENCES nick (nick_id)
      ON DELETE RESTRICT
   ,FOREIGN KEY (nick_id1) REFERENCES nick (nick_id)
      ON DELETE RESTRICT
);

CREATE UNIQUE INDEX imatch ON match (
    match_id
);



-- Table: statistics
-- Used from match and game tables to store match and game statistics,
-- respectively.

CREATE TABLE matchstat (
    matchstat_id        INTEGER NOT NULL
   -- match identification
   ,match_id                          INTEGER NOT NULL
   -- player identification
   ,nick_id                         INTEGER NOT NULL
   -- chequerplay statistics
   ,total_moves                       INTEGER NOT NULL 
   ,unforced_moves                    INTEGER NOT NULL
   ,unmarked_moves                    INTEGER NOT NULL
   ,good_moves                        INTEGER NOT NULL
   ,doubtful_moves                    INTEGER NOT NULL
   ,bad_moves                         INTEGER NOT NULL
   ,very_bad_moves                    INTEGER NOT NULL
   ,chequer_error_total_normalised    FLOAT   NOT NULL
   ,chequer_error_total               FLOAT   NOT NULL
   ,chequer_error_per_move_normalised FLOAT   NOT NULL
   ,chequer_error_per_move            FLOAT   NOT NULL
   ,chequer_rating                    INTEGER NOT NULL
   -- luck statistics
   ,very_lucky_rolls                  INTEGER NOT NULL
   ,lucky_rolls                       INTEGER NOT NULL
   ,unmarked_rolls                    INTEGER NOT NULL
   ,unlucky_rolls                     INTEGER NOT NULL
   ,very_unlucky_rolls                INTEGER NOT NULL
   ,luck_total_normalised             FLOAT   NOT NULL
   ,luck_total                        FLOAT   NOT NULL
   ,luck_per_move_normalised          FLOAT   NOT NULL
   ,luck_per_move                     FLOAT   NOT NULL
   ,luck_rating                       INTEGER NOT NULL
   -- cube statistics
   ,total_cube_decisions              INTEGER NOT NULL
   ,close_cube_decisions              INTEGER NOT NULL
   ,doubles                           INTEGER NOT NULL
   ,takes                             INTEGER NOT NULL
   ,passes                            INTEGER NOT NULL
   ,missed_doubles_below_cp           INTEGER NOT NULL
   ,missed_doubles_above_cp           INTEGER NOT NULL
   ,wrong_doubles_below_dp            INTEGER NOT NULL
   ,wrong_doubles_above_tg            INTEGER NOT NULL
   ,wrong_takes                       INTEGER NOT NULL
   ,wrong_passes                      INTEGER NOT NULL
   ,error_missed_doubles_below_cp_normalised     FLOAT   NOT NULL
   ,error_missed_doubles_above_cp_normalised     FLOAT   NOT NULL
   ,error_wrong_doubles_below_dp_normalised      FLOAT   NOT NULL
   ,error_wrong_doubles_above_tg_normalised      FLOAT   NOT NULL
   ,error_wrong_takes_normalised                 FLOAT   NOT NULL
   ,error_wrong_passes_normalised                FLOAT   NOT NULL
   ,error_missed_doubles_below_cp     FLOAT   NOT NULL
   ,error_missed_doubles_above_cp     FLOAT   NOT NULL
   ,error_wrong_doubles_below_dp      FLOAT   NOT NULL
   ,error_wrong_doubles_above_tg      FLOAT   NOT NULL
   ,error_wrong_takes                 FLOAT   NOT NULL
   ,error_wrong_passes                FLOAT   NOT NULL
   ,cube_error_total_normalised       FLOAT   NOT NULL
   ,cube_error_total                  FLOAT   NOT NULL
   ,cube_error_per_move_normalised    FLOAT   NOT NULL
   ,cube_error_per_move               FLOAT   NOT NULL
   ,cube_rating                       INTEGER NOT NULL
   -- overall statistics
   ,overall_error_total_normalised    FLOAT   NOT NULL
   ,overall_error_total               FLOAT   NOT NULL
   ,overall_error_per_move_normalised FLOAT   NOT NULL
   ,overall_error_per_move            FLOAT   NOT NULL
   ,overall_rating                    INTEGER NOT NULL
   ,actual_result                     FLOAT   NOT NULL
   ,luck_adjusted_result              FLOAT   NOT NULL
   ,snowie_error_rate_per_move        FLOAT   NOT NULL
   -- for matches only
   ,luck_based_fibs_rating_diff       FLOAT           
   ,error_based_fibs_rating           FLOAT           
   ,chequer_rating_loss               FLOAT           
   ,cube_rating_loss                  FLOAT           
   -- for money sesisons only
   ,actual_advantage                  FLOAT           
   ,actual_advantage_ci               FLOAT           
   ,luck_adjusted_advantage           FLOAT           
   ,luck_adjusted_advantage_ci        FLOAT           
   -- time penalties
   ,time_penalties                    INTEGER NOT NULL
   ,time_penalty_loss_normalised      FLOAT   NOT NULL
   ,time_penalty_loss                 FLOAT   NOT NULL
   -- 
   ,PRIMARY KEY (matchstat_id)
   ,FOREIGN KEY (nick_id) REFERENCES nick (nick_id)
      ON DELETE RESTRICT
   ,FOREIGN KEY (match_id) REFERENCES match (match_id)
      ON DELETE CASCADE
);

CREATE UNIQUE INDEX ismatchstat ON matchstat (
    matchstat_id
);

INSERT INTO env VALUES( 0, 'Default env.');
INSERT INTO control VALUES('env', 1);
