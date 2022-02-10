#ifndef GAME2D_OPENGL_INCLUDE_H
#define GAME2D_OPENGL_INCLUDE_H

#include "common.h"

#if PLATFORM_IOS
  #include <OpenGLES/ES1/gl.h>
  #include <OpenGLES/ES1/glext.h>
  #include <OpenGLES/ES2/gl.h>
  #include <OpenGLES/ES2/glext.h>

  #define GL_FRAMEBUFFER_EXT GL_FRAMEBUFFER
  #define GL_COLOR_ATTACHMENT0_EXT GL_COLOR_ATTACHMENT0
#else
  #include <SDL_opengl.h>

extern void (*glGenerateMipmap)(GLenum target);
extern void (*glGenFramebuffers)(GLsizei n, GLuint *ids);
extern void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
extern void (*glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
extern void (*glDeleteFramebuffers)(GLsizei n, GLuint *framebuffers);

#endif  /* !PLATFORM_IOS */

#endif  /* GAME2D_OPENGL_INCLUDE_H */
