;;
;; GNU Backgammon Scheme definitions
;;

;; A _board_ is a pair of vectors each containing 25 integers; the cdr
;; represents the number of chequers for the player on roll (moving from
;; 24 -> 0, where 24 is the bar and 23 to 0 are the 24 to 1 points).
;; The car represents the same for the opponent.

;; It's not worth defining a Guile interface to the C SwapSides() when
;; we can do the same thing more easily in Scheme.
(define (swap-sides b)
  (cons (cdr b) (car b)))
