(module ycl (ycl-connect ycl-close ycl-msgbuf ycl-msgbuf-reset
    ycl-msgbuf-set)
  (import chicken scheme foreign srfi-13)
  (foreign-declare "#include <lib/ycl/ycl.h>")

  ;; let sizeof-ycl_ctx be sizeof(struct ycl_ctx). Define a record 'ycl-ctx
  ;; with a slot called buffer. This creates make-ycl-ctx, ycl-ctx? as well as
  ;; accessor (ycl-ctx-buffer) and mutator (ycl-ctx-buffer-set!) for the buffer
  ;; slot. Alias scheme-pointer to 'ycl-ctx, and have it evaluate to the
  ;; ycl-ctx accessor ycl-ctx-buffer. Later on, when we make/allocate an
  ;; ycl-ctx record, we initialize the buffer slot with 'make-blob
  (define sizeof-ycl_ctx (foreign-value "sizeof(struct ycl_ctx)" size_t))
  (define-record ycl-ctx buffer)
  (define-foreign-type ycl-ctx scheme-pointer ycl-ctx-buffer)

  (define sizeof-ycl_msg (foreign-value "sizeof(struct ycl_msg)" size_t))
  (define-record ycl-msg buffer)
  (define-foreign-type ycl-msg scheme-pointer ycl-msg-buffer)

  (define (ycl-error* ctx)
    (string-copy ((foreign-lambda c-string "ycl_strerror" ycl-ctx) ctx)))

  (define (ycl-error ctx)
    (if (ycl-ctx? ctx) (ycl-error* ctx)))

  (define (ycl-exn ctx)
    (if (ycl-ctx? ctx)
      (signal (make-property-condition 'ycl-exn 'msg (ycl-error ctx)))))

  (define ycl-msg-exn (lambda ()
    (signal (make-property-condition 'ycl-msg-exn))))

  (define (ycl-close ctx)
    (if (ycl-ctx? ctx)
      ((foreign-lambda void "ycl_close" ycl-ctx) ctx)))

  (define (ycl-connect* ctx dst)
    ((foreign-lambda int "ycl_connect" ycl-ctx (const nonnull-c-string))
        ctx dst))

  (define (ycl-connect dst)
    (let*
        ((ctx (make-ycl-ctx (make-blob sizeof-ycl_ctx)))
         (ret (ycl-connect* ctx dst)))
      (if (< ret 0)
        (ycl-exn ctx)
        (set-finalizer! ctx ycl-close))))

  (define (ycl-msgbuf-cleanup msg)
    (if (ycl-msg? msg)
      ((foreign-lambda void "ycl_msg_cleanup" ycl-msg) msg)))

  (define (ycl-msgbuf-reset msg)
    (if (ycl-msg? msg)
      ((foreign-lambda void "ycl_msg_reset" ycl-msg) msg)))

  (define (ycl-msgbuf)
    (let*
        ((msg (make-ycl-msg (make-blob sizeof-ycl_msg)))
         (ret ((foreign-lambda int "ycl_msg_init" ycl-msg) msg)))
      (if (< ret 0)
        (ycl-msg-exn)
        (set-finalizer! msg ycl-msgbuf-cleanup))))

  (define (ycl-msgbuf-set* msg blob)
    ((foreign-lambda int "ycl_msg_set"
        ycl-msg (const nonnull-c-string) size_t) msg blob (blob-size blob)))

  (define (ycl-msgbuf-set msg blob)
    (if (ycl-msg? msg)
      (let ((ret (ycl-msgbuf-set* msg blob)))
        (if (< ret 0)
          (ycl-msg-exn)
          msg))))
)
