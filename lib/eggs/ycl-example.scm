(use ycl)

(define (send-data)
  (let*
      ((ctx (ycl-connect "foobar"))
       (msg (ycl-msgbuf)))
    (ycl-msgbuf-set msg (string->blob "trololo\n"))
    (ycl-sendmsg ctx msg)
    (ycl-recvmsg ctx msg)))

(condition-case (send-data)
  [c (ycl-exn) (printf "ycl exception: ~A~%"
      (get-condition-property c 'ycl-exn 'msg))])
(exit)

