#ifndef GAME2D_OPENGL_INCLUDE_H
#define GAME2D_OPENGL_INCLUDE_H

#include <SDL_opengl.h>

extern void (*glGenerateMipmap)(GLenum target);
extern void (*glGenFramebuffers)(GLsizei n, GLuint *ids);
extern void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
extern void (*glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern void (*glDeleteFramebuffers)(GLsizei n, GLuint *framebuffers);

#endif