#include "shader_uniforms_parser.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include "../gl4es.h"

using namespace std;

struct uniformInfo {
    int currentUniformIndex = 0;
    unordered_map<string, int> uniformNameToIndex;
};

static unordered_map<GLuint, unique_ptr<uniformInfo>> parsedUniformsCache;

static string trim(const string& s) {
    size_t first = s.find_first_not_of(" \t\n\r");
    if (string::npos == first) return "";
    size_t last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, (last - first + 1));
}

static bool isIdChar(char c) {
    return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

__attribute__((used)) __attribute__((visibility("default")))
extern "C" int getUniformIndex(GLuint program, const char* uniformName) {
    if (parsedUniformsCache.contains(program)) {
        const string uniformNameString = uniformName;
        auto uniformInfo = parsedUniformsCache[program].get();
        if (uniformInfo->uniformNameToIndex.contains(uniformNameString)){
            return uniformInfo->uniformNameToIndex[uniformNameString];
        }
    }
    return 0;
}

__attribute__((used)) __attribute__((visibility("default")))
extern "C" void removeProgramFromCache(GLuint program) {
    parsedUniformsCache.erase(program);
}

__attribute__((used)) __attribute__((visibility("default")))
extern "C" void getUniformsFromShader(GLuint program, GLuint shader) {
    FLUSH_BEGINEND;
    CHECK_PROGRAM(void, program)
    CHECK_SHADER(void, shader)

    const auto source = glshader->converted;
    if (!source) return;

    uniformInfo* info;

    if (!parsedUniformsCache.contains(program)) {
        auto newInfo = make_unique<uniformInfo>();
        newInfo->currentUniformIndex = 0;
        info = newInfo.get();
        parsedUniformsCache[program] = std::move(newInfo);
    } else {
        info = parsedUniformsCache[program].get();
    }

    string src = source;
    size_t pos = 0;

    while ((pos = src.find("uniform", pos)) != string::npos) {
        if ((pos == 0 || !isIdChar(src[pos - 1])) &&
            (pos + 7 >= src.length() || !isIdChar(src[pos + 7]))) {

            size_t endDecl = src.find(';', pos);
            if (endDecl != string::npos) {
                string decl = src.substr(pos + 7, endDecl - (pos + 7));

                stringstream ss(decl);
                string segment;
                bool firstSegment = true;
                while (getline(ss, segment, ',')) {
                    string s = trim(segment);
                    if (s.empty()) continue;

                    size_t eq = s.find('=');
                    if (eq != string::npos) s = trim(s.substr(0, eq));

                    string name;
                    if (firstSegment) {
                        size_t lastSpace = s.find_last_of(" \t\n\r");
                        if (lastSpace != string::npos) {
                            name = trim(s.substr(lastSpace + 1));
                        } else {
                            name = s;
                        }
                        firstSegment = false;
                    } else {
                        name = s;
                    }

                    size_t bracket = name.find('[');
                    if (bracket != string::npos) name = trim(name.substr(0, bracket));

                    if (!name.empty() && !info->uniformNameToIndex.contains(name)) {
                        info->uniformNameToIndex[name] = info->currentUniformIndex++;
                    }
                }
                pos = endDecl + 1;
            } else {
                pos += 7;
            }
        } else {
            pos += 7;
        }
    }
}