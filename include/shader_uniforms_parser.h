#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

int getUniformIndex(GLuint program, const char *uniformName);
void getUniformsFromShader(GLuint program, GLuint shader);
void removeProgramFromCache(GLuint program);
#ifdef __cplusplus
}
#endif
