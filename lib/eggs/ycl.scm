(module ycl (ycl-connect ycl-close ycl-msgbuf ycl-msgbuf-reset
    ycl-msgbuf-set)
  (import chicken scheme foreign srfi-13)
  (foreign-declare "#include <lib/ycl/ycl.h>")

  (define sizeof-ycl_ctx (foreign-value "sizeof(struct ycl_ctx)" size_t))
  (define-record ycl-ctx buffer)
  (define-foreign-type ycl-ctx scheme-pointer ycl-ctx-buffer)

  (define sizeof-ycl_msg (foreign-value "sizeof(struct ycl_msg)" size_t))
  (define-record ycl-msg buffer)
  (define-foreign-type ycl-msg scheme-pointer ycl-msg-buffer)

  (define ycl-error (lambda (ctx)
    (string-copy ((foreign-lambda c-string "ycl_strerror" ycl-ctx) ctx))))

  (define ycl-exn (lambda (ctx)
    (signal (make-property-condition 'ycl-exn 'msg (ycl-error ctx)))))

  (define ycl-msg-exn (lambda ()
    (signal (make-property-condition 'ycl-msg-exn))))

  (define ycl-close (lambda (ctx)
    ((foreign-lambda void "ycl_close" ycl-ctx) ctx)))

  (define ycl-connect (lambda (dst)
    (let*
        ((ctx (make-ycl-ctx (make-blob sizeof-ycl_ctx)))
         (ret ((foreign-lambda int "ycl_connect" ycl-ctx
             (const nonnull-c-string)) ctx dst)))
      (if (< ret 0)
        (ycl-exn ctx)
        (set-finalizer! ctx ycl-close)))))

  (define ycl-msgbuf-cleanup (lambda (msg)
    ((foreign-lambda void "ycl_msg_cleanup" ycl-msg) msg)))

  (define ycl-msgbuf-reset (lambda (msg)
    ((foreign-lambda void "ycl_msg_reset" ycl-msg) msg)))

  (define ycl-msgbuf (lambda ()
    (let*
        ((msg (make-ycl-msg (make-blob sizeof-ycl_msg)))
         (ret ((foreign-lambda int "ycl_msg_init" ycl-msg) msg)))
      (if (< ret 0)
        (ycl-msg-exn)
        (set-finalizer! msg ycl-msgbuf-cleanup)))))

  ;; XXX: string-length on binary data?
  ;; XXX: signal on set failure
  (define ycl-msgbuf-set (lambda (msg data)
    ((foreign-lambda int "ycl_msg_set"
        ycl-msg (const nonnull-c-string) size_t) msg data
        (string-length data))))
)
