/*
 * Renderer 3's draw path and the OpenGL entry points past 1.1. See sdl_opengl_internal.h.
 */
#include <stdio.h>
#include <string.h>
#include "sdl_opengl_internal.h"

int32_t gldrawpathrequest = GLPATH_AUTO;
int32_t gldrawpath = GLPATH_IMMEDIATE;
int32_t glinternalwidth = 0, glinternalheight = 0;
int32_t glsupersample = 0;
int32_t gltexturefilter = 0;
int32_t gl_maxfbtest = 0;
uint32_t gl_ssclamps = 0;
double glcontrast = 1.0, glsaturation = 1.0;
int32_t gl_forcefail = 0;

int gl_choosepath(int requested, int glmajor, int glminor, int hasvbo, int hasglsl, int hasfbo)
{
    int best = GLPATH_IMMEDIATE;

    if (hasvbo)
    {
        best = GLPATH_VBO;
        if (hasglsl && hasfbo && ((glmajor > 2) || ((glmajor == 2) && (glminor >= 1))))
            best = GLPATH_SHADER;
    } /* if */

    if ((requested <= GLPATH_AUTO) || (requested >= GLPATH_COUNT) || (requested > best))
        return(best);
    return(requested);
} /* gl_choosepath */


int gl_internalsize(int32_t w, int32_t h, int32_t *ow, int32_t *oh)
{
    *ow = *oh = 0;
    if ((w == 0) && (h == 0))
        return(0);
    if ((w == 0) || (h == 0))
        return(1);
    *ow = (w < 320) ? 320 : w;
    *oh = (h < 200) ? 200 : h;
    return(((*ow != w) || (*oh != h)) ? 2 : 0);
} /* gl_internalsize */


void gl_fitrect(int32_t w, int32_t h, int32_t dw, int32_t dh, int32_t *r)
{
    if ((int64_t) w * dh <= (int64_t) dw * h)  /* as tall as the drawable */
    {
        r[3] = dh;
        r[2] = (int32_t) (((int64_t) w * dh + h / 2) / h);
    } /* if */
    else  /* as wide */
    {
        r[2] = dw;
        r[3] = (int32_t) (((int64_t) h * dw + w / 2) / w);
    } /* else */
    r[0] = (dw - r[2]) / 2;
    r[1] = (dh - r[3]) / 2;
} /* gl_fitrect */


void gl_colorcorrect(unsigned char *c, double contrast, double saturation)
{
    double l, v;
    int i;

    if ((contrast == 1.0) && (saturation == 1.0))
        return;
    l = (0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]) / 255.0;
    for (i = 0; i < 3; i++)
    {
        v = (l + saturation * (c[i] / 255.0 - l) - 0.5) * contrast + 0.5;
        v = (v < 0.0) ? 0.0 : (v > 1.0) ? 1.0 : v;
        c[i] = (unsigned char) (v * 255.0 + 0.5);
    } /* for */
} /* gl_colorcorrect */


#ifdef USE_OPENGL
#include "SDL.h"
#include "build.h"

PFNGLGENBUFFERSPROC bglGenBuffers;
PFNGLDELETEBUFFERSPROC bglDeleteBuffers;
PFNGLBINDBUFFERPROC bglBindBuffer;
PFNGLBUFFERDATAPROC bglBufferData;
PFNGLBUFFERSUBDATAPROC bglBufferSubData;

PFNGLACTIVETEXTUREPROC bglActiveTexture;
PFNGLCREATESHADERPROC bglCreateShader;
PFNGLSHADERSOURCEPROC bglShaderSource;
PFNGLCOMPILESHADERPROC bglCompileShader;
PFNGLGETSHADERIVPROC bglGetShaderiv;
PFNGLGETSHADERINFOLOGPROC bglGetShaderInfoLog;
PFNGLDELETESHADERPROC bglDeleteShader;
PFNGLCREATEPROGRAMPROC bglCreateProgram;
PFNGLATTACHSHADERPROC bglAttachShader;
PFNGLBINDATTRIBLOCATIONPROC bglBindAttribLocation;
PFNGLLINKPROGRAMPROC bglLinkProgram;
PFNGLGETPROGRAMIVPROC bglGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC bglGetProgramInfoLog;
PFNGLDELETEPROGRAMPROC bglDeleteProgram;
PFNGLUSEPROGRAMPROC bglUseProgram;
PFNGLGETUNIFORMLOCATIONPROC bglGetUniformLocation;
PFNGLUNIFORM1IPROC bglUniform1i;
PFNGLUNIFORM1FPROC bglUniform1f;
PFNGLUNIFORM2FPROC bglUniform2f;
PFNGLUNIFORM4FPROC bglUniform4f;
PFNGLVERTEXATTRIBPOINTERPROC bglVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC bglEnableVertexAttribArray;
PFNGLDISABLEVERTEXATTRIBARRAYPROC bglDisableVertexAttribArray;

PFNGLGENFRAMEBUFFERSPROC bglGenFramebuffers;
PFNGLDELETEFRAMEBUFFERSPROC bglDeleteFramebuffers;
PFNGLBINDFRAMEBUFFERPROC bglBindFramebuffer;
PFNGLFRAMEBUFFERTEXTURE2DPROC bglFramebufferTexture2D;
PFNGLCHECKFRAMEBUFFERSTATUSPROC bglCheckFramebufferStatus;
PFNGLGENRENDERBUFFERSPROC bglGenRenderbuffers;
PFNGLDELETERENDERBUFFERSPROC bglDeleteRenderbuffers;
PFNGLBINDRENDERBUFFERPROC bglBindRenderbuffer;
PFNGLRENDERBUFFERSTORAGEPROC bglRenderbufferStorage;
PFNGLFRAMEBUFFERRENDERBUFFERPROC bglFramebufferRenderbuffer;

typedef struct { void **fn; const char *name; } glproc_t;

static const glproc_t vboprocs[] = {
    { (void **) &bglGenBuffers, "glGenBuffers" },
    { (void **) &bglDeleteBuffers, "glDeleteBuffers" },
    { (void **) &bglBindBuffer, "glBindBuffer" },
    { (void **) &bglBufferData, "glBufferData" },
    { (void **) &bglBufferSubData, "glBufferSubData" },
};

static const glproc_t multitexprocs[] = {
    { (void **) &bglActiveTexture, "glActiveTexture" },
};

static const glproc_t glslprocs[] = {
    { (void **) &bglCreateShader, "glCreateShader" },
    { (void **) &bglShaderSource, "glShaderSource" },
    { (void **) &bglCompileShader, "glCompileShader" },
    { (void **) &bglGetShaderiv, "glGetShaderiv" },
    { (void **) &bglGetShaderInfoLog, "glGetShaderInfoLog" },
    { (void **) &bglDeleteShader, "glDeleteShader" },
    { (void **) &bglCreateProgram, "glCreateProgram" },
    { (void **) &bglAttachShader, "glAttachShader" },
    { (void **) &bglBindAttribLocation, "glBindAttribLocation" },
    { (void **) &bglLinkProgram, "glLinkProgram" },
    { (void **) &bglGetProgramiv, "glGetProgramiv" },
    { (void **) &bglGetProgramInfoLog, "glGetProgramInfoLog" },
    { (void **) &bglDeleteProgram, "glDeleteProgram" },
    { (void **) &bglUseProgram, "glUseProgram" },
    { (void **) &bglGetUniformLocation, "glGetUniformLocation" },
    { (void **) &bglUniform1i, "glUniform1i" },
    { (void **) &bglUniform1f, "glUniform1f" },
    { (void **) &bglUniform2f, "glUniform2f" },
    { (void **) &bglUniform4f, "glUniform4f" },
    { (void **) &bglVertexAttribPointer, "glVertexAttribPointer" },
    { (void **) &bglEnableVertexAttribArray, "glEnableVertexAttribArray" },
    { (void **) &bglDisableVertexAttribArray, "glDisableVertexAttribArray" },
};

static const glproc_t fboprocs[] = {
    { (void **) &bglGenFramebuffers, "glGenFramebuffers" },
    { (void **) &bglDeleteFramebuffers, "glDeleteFramebuffers" },
    { (void **) &bglBindFramebuffer, "glBindFramebuffer" },
    { (void **) &bglFramebufferTexture2D, "glFramebufferTexture2D" },
    { (void **) &bglCheckFramebufferStatus, "glCheckFramebufferStatus" },
    { (void **) &bglGenRenderbuffers, "glGenRenderbuffers" },
    { (void **) &bglDeleteRenderbuffers, "glDeleteRenderbuffers" },
    { (void **) &bglBindRenderbuffer, "glBindRenderbuffer" },
    { (void **) &bglRenderbufferStorage, "glRenderbufferStorage" },
    { (void **) &bglFramebufferRenderbuffer, "glFramebufferRenderbuffer" },
};

#define NUMPROCS(a) ((int) (sizeof (a) / sizeof ((a)[0])))

/*
 * Loads a group of entry points, each name with suffix ("", "ARB", "EXT")
 *  when want is set. 1 if all loaded; otherwise all are NULL.
 */
static int gl_loadprocs(const glproc_t *procs, int n, int want, const char *suffix)
{
    char name[64];
    int i;

    for (i = 0; i < n; i++)
    {
        *procs[i].fn = NULL;
        if (want)
        {
            snprintf(name, sizeof (name), "%s%s", procs[i].name, suffix);
            *procs[i].fn = SDL_GL_GetProcAddress(name);
        } /* if */
        if (*procs[i].fn == NULL)
            want = 0;
    } /* for */

    if (!want)
        for (i = 0; i < n; i++)
            *procs[i].fn = NULL;
    return(want);
} /* gl_loadprocs */


/* Whole-word match in a GL_EXTENSIONS string. */
int gl_has_extension(const char *exts, const char *name)
{
    size_t len = strlen(name);
    const char *p = exts;

    while ((p = strstr(p, name)) != NULL)
    {
        if (((p == exts) || (p[-1] == ' ')) && ((p[len] == ' ') || (p[len] == '\0')))
            return(1);
        p += len;
    } /* while */

    return(0);
} /* gl_has_extension */


void gl_initdrawpath(const char *exts)
{
    const char *version = (const char *) glGetString(GL_VERSION);
    int major = 1, minor = 1;
    int core13, core15, core20, arbfbo;

    if ((version == NULL) || (sscanf(version, "%d.%d", &major, &minor) != 2))
        major = 1, minor = 1;
    core13 = (major > 1) || ((major == 1) && (minor >= 3));
    core15 = (major > 1) || ((major == 1) && (minor >= 5));
    core20 = (major >= 2);
    arbfbo = (major >= 3) || gl_has_extension(exts, "GL_ARB_framebuffer_object");

    gl_loadprocs(multitexprocs, NUMPROCS(multitexprocs),
                 core13 || gl_has_extension(exts, "GL_ARB_multitexture"), core13 ? "" : "ARB");
    glinfo.vbo = gl_loadprocs(vboprocs, NUMPROCS(vboprocs),
                              core15 || gl_has_extension(exts, "GL_ARB_vertex_buffer_object"),
                              core15 ? "" : "ARB");
        /* GLSL only as core 2.0: GL_ARB_shader_objects's calls differ. */
    glinfo.glsl = gl_loadprocs(glslprocs, NUMPROCS(glslprocs), core20 && (gl_forcefail != 1), "");
    glinfo.fbo = gl_loadprocs(fboprocs, NUMPROCS(fboprocs),
                              (arbfbo || gl_has_extension(exts, "GL_EXT_framebuffer_object")) && (gl_forcefail != 1),
                              arbfbo ? "" : "EXT");

    gldrawpath = gl_choosepath(gldrawpathrequest, major, minor, glinfo.vbo, glinfo.glsl, glinfo.fbo);
    if ((gldrawpathrequest > GLPATH_AUTO) && (gldrawpathrequest < GLPATH_COUNT) &&
        (gldrawpath != gldrawpathrequest))
        printf("OpenGL path %s unavailable; using %s.\n",
               gl_pathname(gldrawpathrequest), gl_pathname(gldrawpath));
    printf("OpenGL path: %s (%d.%d)\n", gl_pathname(gldrawpath), major, minor);
} /* gl_initdrawpath */
#endif
