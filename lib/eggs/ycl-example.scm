(declare (uses ycl))
(use ycl)
(condition-case (print (ycl-connect "foobar"))
  [c (ycl-exn) (printf "ycl exception: ~A~%"
      (get-condition-property c 'ycl-exn 'msg))])
(exit)

