
#ifndef ARX_CONFIGURE_H
#define ARX_CONFIGURE_H

#cmakedefine HAVE_WINAPI

// Threads
#cmakedefine HAVE_PTHREADS

// Timing
#cmakedefine HAVE_NANOSLEEP
#cmakedefine HAVE_CLOCK_GETTIME

// Audio backend
#cmakedefine HAVE_OPENAL
#cmakedefine HAVE_OPENAL_EFX
#cmakedefine HAVE_DSOUND

#cmakedefine HAVE_OPENGL
#cmakedefine HAVE_D3D7
#cmakedefine HAVE_SDL

// Compiler features
#cmakedefine HAVE_DYNAMIC_STACK_ALLOCATION

#cmakedefine BUILD_EDITOR

#cmakedefine BUILD_EDIT_LOADSAVE

#endif // ARX_CONFIGURE_H
