;;; 
;;; gnubg.scm
;;; 
;;; by Gary Wong <gtw@gnu.org>, 2000
;;; 
;;; This program is free software; you can redistribute it and/or modify
;;; it under the terms of version 2 of the GNU General Public License as
;;; published by the Free Software Foundation.
;;; 
;;; This program is distributed in the hope that it will be useful,
;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.
;;; 
;;; You should have received a copy of the GNU General Public License
;;; along with this program; if not, write to the Free Software
;;; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
;;; 
;;; $Id$
;;; 

;;;
;;; GNU Backgammon Scheme definitions
;;;

;;; A _board_ is a pair of vectors each containing 25 integers; the cdr
;;; represents the number of chequers for the player on roll (moving from
;;; 24 -> 0, where 24 is the bar and 23 to 0 are the 24 to 1 points).
;;; The car represents the same for the opponent.

;; It's not worth defining a Guile interface to the C SwapSides() when
;; we can do the same thing more easily in Scheme.
(define (swap-sides b)
  "Return a specified position with the opposite player on roll."
  (cons (cdr b) (car b)))

;; Similarly for counting pips.
(define (pip-count b)
  "Give the pip count of the specified position.  The cdr is the pip count
of the player on roll, and the car is the pip count of the opponent."
  (define (count v i c)
    (if (>= i 25)
	c
	(count v (+ i 1) (+ c (* (vector-ref v i) (+ i 1))))))
  (cons (count (car b) 0 0) (count (cdr b) 0 0)))
