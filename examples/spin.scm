(define list '())

(define (sphere)
	(push)
	(opacity 0.1)
	(colour (vector 0 0 0))
	(specular (vector 1 1 0))
	(shinyness 100)
	(set! list (cons (build-sphere 10 10) list))
	(apply (car list))
	;parent (car list))
	(pop))

(define (line n)
	(translate (vector 1 0 0))
	(sphere)
	(if (eq? 0 n)
		1
		(line (- n 1))))

(define (grid m n)
	(translate (vector 0 1 0))
	(push)
	(line n)
	(pop)
	(if (eq? 0 m)
		1
		(grid (- m 1) n)))

(define (update l c s)
	(if (null? l)
		'()
		(begin 
			(grab (car l))
			(emissive (vector (gh c) (gh (+ c 1)) (gh (+ c 2)) ))
			(specular (vector (*(gh s)100) (*(gh s)100) (*(gh s)100)))
			;(identity)
			(rotate (vector (*(gh s)10) (*(gh s)10) (*(gh s)10) ))
			;(scale (vector (gh s) (gh s) (gh s) ))
			(ungrab)
			(update (cdr l) (+ c 1) (+ s 1)))))

(show-axis 0)
(clear)
(grid 10 10)
(blur 0.001)
(display list)
(every-frame "(update list 1 1)")
