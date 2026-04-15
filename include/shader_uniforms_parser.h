#include <GL/gl.h>
#include <string>
#include <vector>

using namespace std;

vector<string> getUniforms(GLuint program);
void removeProgramFromCache(GLuint program);

#ifdef __cplusplus
extern "C" {
#endif
void parseUniformsFromShader(GLuint program, GLuint shader);
#ifdef __cplusplus
}
#endif
