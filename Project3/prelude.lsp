;;;
;;;   Lispy Standard Prelude
;;;

;;; list = quote (q-expr)

;;; Atoms
(def {nil} {})
(def {true} 1)
(def {false} 0)

;;; defunctional defunctions

; defunction Definitions
(def {defun} (\ {f b} {
  def (head f) (\ (tail f) b)
}))

; Open new scope
(defun {let b} {
  ((\ {_} b) ())
})

; Unpack List to defunction
(defun {unpack f l} {
  eval (join (list f) l)
})

; Unapply List to defunction
(defun {pack f & xs} {f xs})

; Curried and Uncurried calling
(def {curry} unpack)
(def {uncurry} pack)

; Perform Several things in Sequence
(defun {do & l} {
  if (== l nil)
    {nil}
    {last l}
})

;;; Logical functions

; Logical functions
(defun {not x}   {- 1 x})
(defun {or x y}  {+ x y})
(defun {and x y} {* x y})

;;; Numeric functions

; Minimum of Arguments
(defun {min & xs} {
  if (== (tail xs) nil) {fst xs}
    {do 
      (= {rest} (unpack min (tail xs)))
      (= {item} (fst xs))
      (if (< item rest) {item} {rest})
    }
})

; Maximum of Arguments
(defun {max & xs} {
  if (== (tail xs) nil) {fst xs}
    {do 
      (= {rest} (unpack max (tail xs)))
      (= {item} (fst xs))
      (if (> item rest) {item} {rest})
    }  
})

;;; Conditional functions

(defun {select & cs} {
  if (== cs nil)
    {error "No Selection Found"}
    {if (fst (fst cs)) {snd (fst cs)} {unpack select (tail cs)}}
})

(defun {case x & cs} {
  if (== cs nil)
    {error "No Case Found"}
    {if (== x (fst (fst cs))) {snd (fst cs)} {
	  unpack case (join (list x) (tail cs))}}
})

(def {otherwise} true)

;;; Misc functions

(defun {flip f a b} {f b a})
(defun {ghost & xs} {eval xs})
(defun {comp f g x} {f (g x)})

;;; List functions

; First, Second, or Third Item in List
(defun {fst l} { eval (head l) })
(defun {snd l} { eval (head (tail l)) })
(defun {trd l} { eval (head (tail (tail l))) })

; List Length
(defun {len l} {
  if (== l nil)
    {0}
    {+ 1 (len (tail l))}
})

; Nth item in List
(defun {nth n l} {
  if (== n 0)
    {fst l}
    {nth (- n 1) (tail l)}
})

; Last item in List
(defun {last l} {nth (- (len l) 1) l})

; Apply function to List
(defun {map f l} {
  if (== l nil)
    {nil}
    {join (list (f (fst l))) (map f (tail l))}
})

; Apply Filter to List
(defun {filter f l} {
  if (== l nil)
    {nil}
    {join (if (f (fst l)) {head l} {nil}) (filter f (tail l))}
})

; Return all of list but last element
(defun {init l} {
  if (== (tail l) nil)
    {nil}
    {join (head l) (init (tail l))}
})

; Reverse List
(defun {reverse l} {
  if (== l nil)
    {nil}
    {join (reverse (tail l)) (head l)}
})

; Fold Left
(defun {foldl f z l} {
  if (== l nil) 
    {z}
    {foldl f (f z (fst l)) (tail l)}
})

; Fold Right
(defun {foldr f z l} {
  if (== l nil) 
    {z}
    {f (fst l) (foldr f z (tail l))}
})

(defun {sum l} {foldl + 0 l})
(defun {product l} {foldl * 1 l})

; Take N items
(defun {take n l} {
  if (== n 0)
    {nil}
    {join (head l) (take (- n 1) (tail l))}
})

; Drop N items
(defun {drop n l} {
  if (== n 0)
    {l}
    {drop (- n 1) (tail l)}
})

; Split at N
(defun {split n l} {list (take n l) (drop n l)})

; Take While
(defun {take-while f l} {
  if (not (unpack f (head l)))
    {nil}
    {join (head l) (take-while f (tail l))}
})

; Drop While
(defun {drop-while f l} {
  if (not (unpack f (head l)))
    {l}
    {drop-while f (tail l)}
})

; Element of List
(defun {elem x l} {
  if (== l nil)
    {false}
    {if (== x (fst l)) {true} {elem x (tail l)}}
})

; Find element in list of pairs
(defun {lookup x l} {
  if (== l nil)
    {error "No Element Found"}
    {do
      (= {key} (fst (fst l)))
      (= {val} (snd (fst l)))
      (if (== key x) {val} {lookup x (tail l)})
    }
})

; Zip two lists together into a list of pairs
(defun {zip x y} {
  if (or (== x nil) (== y nil))
    {nil}
    {join (list (join (head x) (head y))) (zip (tail x) (tail y))}
})

; Unzip a list of pairs into two lists
(defun {unzip l} {
  if (== l nil)
    {{nil nil}}
    {do
      (= {x} (fst l))
      (= {xs} (unzip (tail l)))
      (list (join (head x) (fst xs)) (join (tail x) (snd xs)))
    }
})

; WHEN macro
(defun {when c d} {if (eval c) (eval d) {}})

;;; Other defun

; numberp
(defun {numberp n} {
  (or (== (ldb n 0) 1) 
      (== (ldb n 0) 2))
})
      
; symbolp
(defun {symbolp n} {
  (== (ldb n 0) 7)
})

(defun {dec n} {- n 1})
(defun {inc n} {+ n 1})

(defun {plusp n} {> n 0})
(defun {minusp n} {< n 0})
(defun {zerop n} {== n 0})
(defun {oddp n} {% n 2})
(defun {evenp n} {not (oddp n)})

(defun {const n} {\ {_} (list n)})

; Fibonacci
(defun {fib n} {
  select
    { (== n 0) 0 }
    { (== n 1) 1 }
    { otherwise (+ (fib (- n 1)) (fib (- n 2))) }
})

(defun {bfib n} {
  select
    { (== (cmp-bnum n (to-bnum 0)) 0) 0 }
    { (== (cmp-bnum n (to-bnum 1)) 0) 1 }
    { otherwise (addb (bfib (subb n 1)) (bfib (subb n 2))) }
})

; Factorial
(defun {fac n} {
  product (range 2 (inc n))
})
