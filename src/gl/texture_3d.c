#include "texture.h"

#include <stdlib.h>
#include <string.h>

#include "../glx/hardext.h"
#include "../glx/streaming.h"
#include "array.h"
#include "blit.h"
#include "decompress.h"
#include "debug.h"
#include "enum_info.h"
#include "fpe.h"
#include "framebuffers.h"
#include "gles.h"
#include "init.h"
#include "loader.h"
#include "matrix.h"
#include "pixel.h"
#include "raster.h"
#include "GL/gl.h"

typedef void(APIENTRY_GLES* glTexImage3D_PTR)(GLenum target, GLint level, GLint internalFormat, GLsizei width,
                                              GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                                              const GLvoid* data);
typedef void(APIENTRY_GLES* glTexSubImage3D_PTR)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                 GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                                 GLenum format, GLenum type, const GLvoid* data);
typedef void(APIENTRY_GLES* glCopyTexSubImage3D_PTR)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                     GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);

// #define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

static int inline nlevel3d(int size, int level) {
    if (size) {
        size >>= level;
        if (!size) size = 1;
    }
    return size;
}

static int inline maxlevel3d(int w, int h, int d) {
    int mlevel = 0;
    while (w != 1 || h != 1 || d != 1) {
        w >>= 1;
        h >>= 1;
        d >>= 1;
        if (!w) w = 1;
        if (!h) h = 1;
        if (!d) d = 1;
        ++mlevel;
    }
    return mlevel;
}

static size_t pad_to(size_t v, GLint align) {
    if (align <= 1) return v;
    size_t rem = v % (size_t)align;
    return rem ? v + ((size_t)align - rem) : v;
}


// The real function to convert format
void internal_convert(GLenum* internal_format, GLenum* type, GLenum* format) {
    if (format && (*format == GL_BGRA || *format == GL_BGR || *format == GL_BGRA8_EXT)) return;
    if (type && *type == GL_UNSIGNED_INT_8_8_8_8) return;

    switch (*internal_format) {
        case GL_DEPTH_COMPONENT16:
            if (type) *type = GL_UNSIGNED_SHORT;
            break;
        case GL_DEPTH_COMPONENT24:
            if (type) *type = GL_UNSIGNED_INT;
            break;
        case GL_DEPTH_COMPONENT32:
            *internal_format = GL_DEPTH_COMPONENT;
            if (type) *type = GL_UNSIGNED_INT;
            break;
        case GL_DEPTH_COMPONENT32F:
            if (type) *type = GL_FLOAT;
            break;
        case GL_DEPTH_COMPONENT:
            if (type) {
                *internal_format = GL_DEPTH_COMPONENT;
                *type = GL_UNSIGNED_INT;
            }
            break;
        case GL_DEPTH_STENCIL:
            *internal_format = GL_DEPTH32F_STENCIL8;
            if (type) *type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
            break;
        case GL_RGB10_A2:
            if (type) *type = GL_UNSIGNED_INT_2_10_10_10_REV;
            break;
        case GL_RGB5_A1:
            if (type) *type = GL_UNSIGNED_SHORT_5_5_5_1;
            break;
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_RG_RGTC2:
            break;
        case GL_SRGB8:
            if (type) *type = GL_UNSIGNED_BYTE;
            break;
        case GL_RGBA32F:
        case GL_RGB32F:
            if (type) *type = GL_FLOAT;
            break;
        case GL_RGB9_E5:
            if (type) *type = GL_UNSIGNED_INT_5_9_9_9_REV;
            break;
        case GL_R11F_G11F_B10F:
            if (type) *type = GL_UNSIGNED_INT_10F_11F_11F_REV;
            if (format) *format = GL_RGB;
            break;
        case GL_RGBA32UI:
        case GL_RGB32UI:
            if (type) *type = GL_UNSIGNED_INT;
            break;
        case GL_RGBA32I:
        case GL_RGB32I:
            if (type) *type = GL_INT;
            break;
        case GL_RGBA16: {
            *internal_format = GL_RGBA16F;
            if (type) *type = GL_FLOAT;
            break;
        }
        case GL_RGBA8:
        case GL_RGBA:
            if (type) *type = GL_UNSIGNED_BYTE;
            if (format) *format = GL_RGBA;
            break;
        case GL_RGBA16F:
            if (type) *type = GL_HALF_FLOAT;
            break;
        case GL_R16:
            *internal_format = GL_R16F;
            if (type) *type = GL_FLOAT;
            break;
        case GL_RGB16:
            *internal_format = GL_RGB16F;
            if (type) *type = GL_HALF_FLOAT;
            if (format) *format = GL_RGB;
            break;
        case GL_RGB16F:
            if (type) *type = GL_HALF_FLOAT;
            if (format) *format = GL_RGB;
            break;
        case GL_RG16:
            *internal_format = GL_RG16F;
            if (type) *type = GL_HALF_FLOAT;
            if (format) *format = GL_RG;
            break;
            // Inline R and RG channel mappings
        case GL_R8:
            if (format) *format = GL_RED;
            if (type) *type = GL_UNSIGNED_BYTE;
            break;
        case GL_R8_SNORM:
            if (format) *format = GL_RED;
            if (type) *type = GL_BYTE;
            break;
        case GL_R16F:
            if (format) *format = GL_RED;
            if (type) *type = GL_HALF_FLOAT;
            break;
        case GL_RED:
            if (type) {
                switch (*type) {
                    case GL_UNSIGNED_BYTE:
                        *internal_format = GL_R8;
                        if (format) *format = GL_RED;
                        break;
                    case GL_BYTE:
                        *internal_format = GL_R8_SNORM;
                        if (format) *format = GL_RED;
                        break;
                    case GL_HALF_FLOAT:
                        *internal_format = GL_R16F;
                        if (format) *format = GL_RED;
                        break;
                    case GL_FLOAT:
                        *internal_format = GL_R32F;
                        if (format) *format = GL_RED;
                        break;
                    default:
                        if (type) *type = GL_UNSIGNED_BYTE; // Fallback to unsigned byte
                        *internal_format = GL_R8;           // Fallback to R8
                        if (format) *format = GL_RED;
                        break;
                }
            }
            break;
        case GL_R8UI:
            if (format) *format = GL_RED_INTEGER;
            if (type) *type = GL_UNSIGNED_BYTE;
            break;
        case GL_R8I:
            if (format) *format = GL_RED_INTEGER;
            if (type) *type = GL_BYTE;
            break;
        case GL_R16UI:
            if (format) *format = GL_RED_INTEGER;
            if (type) *type = GL_UNSIGNED_SHORT;
            break;
        case GL_R16I:
            if (format) *format = GL_RED_INTEGER;
            if (type) *type = GL_SHORT;
            break;
        case GL_R32UI:
            if (format) *format = GL_RED_INTEGER;
            if (type) *type = GL_UNSIGNED_INT;
            break;
        case GL_R32I:
            if (format) *format = GL_RED_INTEGER;
            if (type) *type = GL_INT;
            break;
        case GL_RG8:
            if (format) *format = GL_RG;
            if (type) *type = GL_UNSIGNED_BYTE;
            break;
        case GL_RG8_SNORM:
            if (format) *format = GL_RG;
            if (type) *type = GL_BYTE;
            break;
        case GL_RG16F:
            if (format) *format = GL_RG;
            if (type) *type = GL_HALF_FLOAT;
            break;
        case GL_RG32F:
            if (format) *format = GL_RG;
            if (type) *type = GL_FLOAT;
            break;
        case GL_RG8UI:
            if (format) *format = GL_RG_INTEGER;
            if (type) *type = GL_UNSIGNED_BYTE;
            break;
        case GL_RG8I:
            if (format) *format = GL_RG_INTEGER;
            if (type) *type = GL_BYTE;
            break;
        case GL_RG16UI:
            if (format) *format = GL_RG_INTEGER;
            if (type) *type = GL_UNSIGNED_SHORT;
            break;
        case GL_RG16I:
            if (format) *format = GL_RG_INTEGER;
            if (type) *type = GL_SHORT;
            break;
        case GL_RG32UI:
            if (format) *format = GL_RG_INTEGER;
            if (type) *type = GL_UNSIGNED_INT;
            break;
        case GL_RG32I:
            if (format) *format = GL_RG_INTEGER;
            if (type) *type = GL_INT;
            break;
        case GL_RGBA8_SNORM:
            if (format) *format = GL_RGBA;
            if (type) *type = GL_BYTE;
            break;
        case GL_R32F:
            if (format) *format = GL_RED;
            if (type) *type = GL_FLOAT;
            break;
        default:
            // fallback handling for GL_RGB8, GL_RGBA16_SNORM etc.
            if (*internal_format == GL_RGB8) {
                if (type && *type != GL_UNSIGNED_BYTE) *type = GL_UNSIGNED_BYTE;
                if (format) *format = GL_RGB;
            } else if (*internal_format == GL_RGBA16_SNORM) {
                if (type && *type != GL_SHORT) *type = GL_SHORT;
            }
            break;
    }
}

void APIENTRY_GL4ES gl4es_glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                                       GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid* data) {

    if (width == 0 || height == 0 || depth == 0) {
        DBG(SHUT_LOGE("Error: width, height or depth is zero."))
        return;
    }
    if (format == GL_DEPTH_COMPONENT) {
        internalformat = GL_DEPTH_COMPONENT;
        type = GL_UNSIGNED_INT;
    }
    if (internalformat == GL_RGBA16) {
        internalformat = GL_RGBA16F;
        type = GL_FLOAT;
#ifdef GL_RGBA16_SNORM
    } else if (internalformat == GL_RGBA16_SNORM) {
        internalformat = GL_RGBA;
#endif
    }

    internal_convert(&internalformat, &type, &format);

    if (data == NULL && (internalformat == GL_RGB16F || internalformat == GL_RGBA16F))
        internal2format_type(&internalformat, &format, &type);
    if (internalformat == GL_R16F) internal2format_type(&internalformat, &format, &type);
    if (data == NULL && (internalformat == GL_RED || internalformat == GL_RGB))
        internal2format_type(&internalformat, &format, &type);

    if (internalformat == GL_DEPTH32F_STENCIL8 && type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV) {
        internalformat = GL_DEPTH24_STENCIL8;
        type = GL_UNSIGNED_INT_24_8;
    }

    const GLuint itarget = what_target(target);
    const GLuint rtarget = map_tex_target(target);

    if (globals4es.force16bits) {
        if (internalformat == GL_RGBA || internalformat == 4 || internalformat == GL_RGBA8)
            internalformat = GL_RGBA4;
        else if (internalformat == GL_RGB || internalformat == 3 || internalformat == GL_RGB8)
            internalformat = GL_RGB5;
    }

    if (rtarget == GL_PROXY_TEXTURE_2D) {
        int max1 = hardext.maxsize;
        glstate->proxy_width = ((width << level) > max1) ? 0 : width;
        glstate->proxy_height = ((height << level) > max1) ? 0 : height;
        glstate->proxy_intformat = swizzle_internalformat((GLenum*)&internalformat, format, type);
        return;
    }

    realize_bound(glstate->texture.active, target);

    if (glstate->list.pending) {
        gl4es_flush();
    } else {
        PUSH_IF_COMPILING(glTexImage3D);
    }

    if (type == GL_HALF_FLOAT) type = GL_HALF_FLOAT_OES;

    gltexture_t* bound = glstate->texture.bound[glstate->texture.active][itarget];
    bound->alpha = pixel_hasalpha(format);

    if (glstate->fpe_state) {
        switch (internalformat) {
        case GL_COMPRESSED_ALPHA:
        case GL_ALPHA4:
        case GL_ALPHA8:
        case GL_ALPHA16:
        case GL_ALPHA16F:
        case GL_ALPHA32F:
        case GL_ALPHA:
            bound->fpe_format = FPE_TEX_ALPHA;
            break;
        case 1:
        case GL_COMPRESSED_LUMINANCE:
        case GL_LUMINANCE4:
        case GL_LUMINANCE8:
        case GL_LUMINANCE16:
        case GL_LUMINANCE16F:
        case GL_LUMINANCE32F:
        case GL_LUMINANCE:
            bound->fpe_format = FPE_TEX_LUM;
            break;
        case 2:
        case GL_COMPRESSED_LUMINANCE_ALPHA:
        case GL_LUMINANCE4_ALPHA4:
        case GL_LUMINANCE8_ALPHA8:
        case GL_LUMINANCE16_ALPHA16:
        case GL_LUMINANCE_ALPHA16F:
        case GL_LUMINANCE_ALPHA32F:
        case GL_LUMINANCE_ALPHA:
            bound->fpe_format = FPE_TEX_LUM_ALPHA;
            break;
        case GL_COMPRESSED_INTENSITY:
        case GL_INTENSITY8:
        case GL_INTENSITY16:
        case GL_INTENSITY16F:
        case GL_INTENSITY32F:
        case GL_INTENSITY:
            bound->fpe_format = FPE_TEX_INTENSITY;
            break;
        case 3:
        case GL_RED:
        case GL_RG:
        case GL_RGB:
        case GL_RGB5:
        case GL_RGB565:
        case GL_RGB8:
        case GL_RGB16:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGB:
        case GL_R11F_G11F_B10F:
        case GL_R32F:
        case GL_RGB10_A2:
            bound->fpe_format = FPE_TEX_RGB;
            break;
        default:
            bound->fpe_format = FPE_TEX_RGBA;
        }
    }

    if (GL4ES_AUTOMIPMAP_PLACEHOLDER) {
        if (level > 0) {
            if (GL4ES_AUTOMIPMAP_PLACEHOLDER == 3) {
                return;
            } else if (GL4ES_AUTOMIPMAP_PLACEHOLDER == 2) {
                bound->mipmap_need = 1;
            }
        }
    }

    if (level == 0 || !bound->valid) {
        bound->wanted_internal = internalformat;
    }
    GLenum new_format = swizzle_internalformat((GLenum*)&internalformat, format, type);
    if (level == 0 || !bound->valid) {
        bound->orig_internal = internalformat;
        bound->internalformat = new_format;
    }

    bound->format = format;
    bound->type = type;
    bound->inter_format = format;
    bound->inter_type = type;

    bound->width = width;
    bound->height = height;
    bound->depth = depth;
    GLsizei nwidth = (hardext.npot) ? width : npot(width);
    GLsizei nheight = (hardext.npot) ? height : npot(height);
    GLsizei ndepth = (hardext.npot) ? depth : npot(depth);
    bound->nwidth = nwidth;
    bound->nheight = nheight;
    bound->ndepth = ndepth;
    bound->npot = (nwidth != width || nheight != height || ndepth != depth);

    if (level == 0) bound->valid = 1;

    noerrorShim();
    LOAD_GLES3(glTexImage3D);
    gles_glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, data);

    if (glstate->bound_changed < glstate->texture.active + 1) glstate->bound_changed = glstate->texture.active + 1;
}
void APIENTRY_GL4ES gl4es_glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                          GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                          const GLvoid* data) {

    if (width == 0 || height == 0 || depth == 0) {
        DBG(SHUT_LOGE("Error: width, height or depth is zero."))
        return;
    }

    extern void* rgb565_to_rgba8(int width, int height, const void* data);

    gltexture_t* bound = gl4es_getCurrentTexture(target);

    bool isRGB565 = bound->internalformat == GL_RGB565;
    GLvoid* rgb565Pixels = nullptr;

    if (isRGB565) {
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        data = rgb565Pixels = rgb565_to_rgba8(width, height,data);
    }

    if (bound->wanted_internal == GL_RGBA8) {
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
    } else if (bound->wanted_internal == GL_RGB8){
        format = GL_RGB;
        type = GL_UNSIGNED_BYTE;
    }

    if (glstate->list.pending) {
        gl4es_flush();
    } else {
        PUSH_IF_COMPILING(glTexSubImage3D);
    }

    if (!data) {
        DBG(SHUT_LOGD("LIBGL: glTexSubImage3D called with NULL data\n");)
        return;
    }

    LOAD_GLES3(glTexSubImage3D);
    gles_glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type,
                         (const GLvoid*)data);

    if (rgb565Pixels){
        free(rgb565Pixels);
    }
}


void APIENTRY_GL4ES gl4es_glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                         GLsizei height, GLsizei depth) {
    DBG(SHUT_LOGD("glTexStorage3D(%s, %d, %s, %d, %d, %d)\n", PrintEnum(target), levels, PrintEnum(internalformat),
                  width, height, depth);)
    if (!levels) {
        noerrorShim();
        return;
    }

    GLenum wanted_internalformat = GL_RGBA8;

    if (internalformat == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || internalformat == GL_COMPRESSED_SRGB_S3TC_DXT1_EXT){
        wanted_internalformat = GL_RGB8;
    } else if (internalformat == GL_DEPTH24_STENCIL8 || internalformat == GL_RG16 ||internalformat == GL_RG8 ||
            internalformat == GL_R32F || internalformat == GL_RG16F || internalformat == GL_R8 ||
            internalformat == GL_RGBA32F || internalformat == GL_RGBA16F){
        wanted_internalformat = internalformat;
    }

    noerrorShim();
    LOAD_GLES3(glTexStorage3D);
    gles_glTexStorage3D(target, levels, wanted_internalformat, width, height, depth);

    int mlevel = maxlevel3d(width, height, depth);
    gltexture_t* bound = gl4es_getCurrentTexture(target);
    bound->internalformat = internalformat;
    bound->wanted_internal = wanted_internalformat;
    bound->width = width;
    bound->height = height;
    bound->depth = depth;
    GLsizei nwidth = (hardext.npot) ? width : npot(width);
    GLsizei nheight = (hardext.npot) ? height : npot(height);
    GLsizei ndepth = (hardext.npot) ? depth : npot(depth);
    bound->nwidth = nwidth;
    bound->nheight = nheight;
    bound->ndepth = ndepth;
    bound->npot = (nwidth != width || nheight != height || ndepth != depth);

    if (levels > 1 && isDXTc(internalformat)) {
        bound->mipmap_need = 1;
        bound->mipmap_auto = 1;
        return;
    }
    if (mlevel > levels - 1) {
        bound->max_level = levels - 1;
        if (levels > 1 && GL4ES_AUTOMIPMAP_PLACEHOLDER != 3) bound->mipmap_need = 1;
    }
}

void APIENTRY_GL4ES gl4es_glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                              GLint x, GLint y, GLsizei width, GLsizei height) {
    FLUSH_BEGINEND;

    if (globals4es.skiptexcopies) {
        DBG(SHUT_LOGD("glCopyTexSubImage3D skipped.\n"));
        return;
    }

    LOAD_GLES3(glCopyTexSubImage3D);
    errorGL();
    realize_bound(glstate->texture.active, target);

    glbuffer_t* pack = glstate->vao->pack;
    glbuffer_t* unpack = glstate->vao->unpack;
    glstate->vao->pack = NULL;
    glstate->vao->unpack = NULL;

    readfboBegin();
    gles_glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    readfboEnd();

    glstate->vao->pack = pack;
    glstate->vao->unpack = unpack;
}

// Direct wrapper
AliasExport(void, glTexImage3D, ,
            (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth,
             GLint border, GLenum format, GLenum type, const GLvoid* data));
AliasExport(void, glTexImage3D, EXT,
            (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth,
             GLint border, GLenum format, GLenum type, const GLvoid* data));
AliasExport(void, glTexSubImage3D, ,
            (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
             GLsizei depth, GLenum format, GLenum type, const GLvoid* data));
AliasExport(void, glCopyTexSubImage3D, ,
            (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width,
             GLsizei height));

// EXT mapper
AliasExport(void, glTexSubImage3D, EXT,
            (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
             GLsizei depth, GLenum format, GLenum type, const GLvoid* data));
AliasExport(void, glCopyTexSubImage3D, EXT,
            (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width,
             GLsizei height));

// ARB mapper
AliasExport(void, glTexSubImage3D, ARB,
            (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height,
             GLsizei depth, GLenum format, GLenum type, const GLvoid* data));
AliasExport(void, glCopyTexSubImage3D, ARB,
            (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width,
             GLsizei height));

// TexStorage
AliasExport(void, glTexStorage3D, ,
            (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth));
