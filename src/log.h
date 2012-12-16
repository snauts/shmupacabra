#ifndef GAME2D_LOG_H
#define GAME2D_LOG_H

/* No logging for release builds. */
#ifdef NDEBUG

#define log_open(...)           ((void)0)
#define log_err(...)            ((void)0)
#define log_warn(...)           ((void)0)
#define log_msg(...)            ((void)0)
#define log_write(...)          ((void)0)
#define log_buffer(...)         ((void)0)
void    fatal_error(const char *fmt, ...);

#else

void    log_open(const char *filename);

void    log_err(const char *fmt, ...);          /* [ERROR] tag + newline.    */
void    log_warn(const char *fmt, ...);         /* [WARNING] tag + newline.  */
void    log_msg(const char *fmt, ...);          /* No tag, newline appended. */

void    log_write(const char *fmt, ...);        /* No tag, no newline.       */
void    log_buffer(const char *tag, void *buf, unsigned int bufsize);

void    fatal_error(const char *fmt, ...);      /* log_err(), then abort().  s*/

#endif  /* NDEBUG */

#endif  /* GAME2D_LOG_H */
