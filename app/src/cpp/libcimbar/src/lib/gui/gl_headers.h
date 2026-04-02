/* This code is subject to the terms of the Mozilla Public License, v.2.0. http://mozilla.org/MPL/2.0/. */
#pragma once

#if defined(__has_include)
#if __has_include(<GLES2/gl2.h>)
#include <GLES2/gl2.h>
#endif
#if __has_include(<GLES2/gl2ext.h>)
#include <GLES2/gl2ext.h>
#endif
#if !defined(GL_ES_VERSION_2_0) && __has_include(<GLES3/gl3.h>)
#include <GLES3/gl3.h>
#endif
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif
