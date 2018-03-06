(module ycl (ycl-connect ycl-close ycl-msgbuf ycl-msgbuf-reset
    ycl-msgbuf-set ycl-sendmsg ycl-recvmsg)
  (import scheme chicken foreign srfi-13)

  (foreign-declare
    "#include <unistd.h>"
    "#include <lib/ycl/ycl.h>")

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

  (define-record ycl-fd fd)
  (define make-ycl-fd* make-ycl-fd)

  (define (ycl-fd-close fd)
    ((foreign-lambda int close int) (ycl-fd-fd fd)))

  (define (make-ycl-fd fd)
    (set-finalizer! (make-ycl-fd* fd) ycl-fd-close))

  (define (ycl-strerror* ctx)
    (string-copy ((foreign-lambda c-string "ycl_strerror" ycl-ctx) ctx)))

  (define (ycl-strerror ctx)
    (if (ycl-ctx? ctx) (ycl-strerror* ctx)))

  (define (throw-ycl-exn ctx)
    (if (ycl-ctx? ctx)
      (signal (make-property-condition 'ycl-exn 'msg (ycl-strerror ctx)))))

  (define (throw-ycl-msg-exn)
    (signal (make-property-condition 'ycl-msg-exn)))

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
        (throw-ycl-exn ctx)
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
        (throw-ycl-msg-exn)
        (set-finalizer! msg ycl-msgbuf-cleanup))))

  (define (ycl-msgbuf-set* msg blob)
    ((foreign-lambda int "ycl_msg_set"
        ycl-msg (const nonnull-blob) size_t) msg blob (blob-size blob)))

  (define (ycl-msgbuf-set msg blob)
    (if (ycl-msg? msg)
      (let ((ret (ycl-msgbuf-set* msg blob)))
        (if (< ret 0)
          (throw-ycl-msg-exn)
          msg))))

  (define (ycl-sendmsg* ctx msg)
    ((foreign-lambda int "ycl_sendmsg" ycl-ctx ycl-msg) ctx msg))

  (define (ycl-sendmsg ctx msg)
    (assert (ycl-ctx? ctx))
    (assert (ycl-msg? msg))
    (let ((ret (ycl-sendmsg* ctx msg)))
      (if (< ret 0)
        (throw-ycl-exn ctx)
        ctx)))

  (define (ycl-recvmsg* ctx msg)
    ((foreign-lambda int "ycl_recvmsg" ycl-ctx ycl-msg) ctx msg))

  (define (ycl-recvmsg ctx msg)
    (assert (ycl-ctx? ctx))
    (assert (ycl-msg? msg))
    (let ((ret (ycl-recvmsg* ctx msg)))
      (if (< ret 0)
        (throw-ycl-exn ctx)
        ctx)))
)
