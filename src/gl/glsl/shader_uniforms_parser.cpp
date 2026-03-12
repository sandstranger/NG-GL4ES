#include "shader_uniforms_parser.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>
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

static bool isUniformUsed(const string& src, const string& name) {
    if (name.empty()) return false;
    size_t count = 0;
    size_t pos = 0;
    const size_t nameLen = name.length();
    while ((pos = src.find(name, pos)) != string::npos) {
        bool startMatch = (pos == 0 || !isIdChar(src[pos - 1]));
        bool endMatch = (pos + nameLen >= src.length() || !isIdChar(src[pos + nameLen]));
        if (startMatch && endMatch) {
            count++;
            if (count > 1) return true;
        }
        pos += nameLen;
    }
    return false;
}

extern "C" {

__attribute__((used)) __attribute__((visibility("default")))
int getUniformIndex(GLuint program, const char* uniformName) {
    if (parsedUniformsCache.contains(program)) {
        const string nameStr = uniformName;
        auto& info = parsedUniformsCache.at(program);
        if (info->uniformNameToIndex.contains(nameStr)) {
            return info->uniformNameToIndex.at(nameStr);
        }
    }
    return 0;
}

__attribute__((used)) __attribute__((visibility("default")))
void removeProgramFromCache(GLuint program) {
    parsedUniformsCache.erase(program);
}

__attribute__((used)) __attribute__((visibility("default")))
void getUniformsFromShader(GLuint program, GLuint shader) {
    FLUSH_BEGINEND;
    CHECK_PROGRAM(void, program)
    CHECK_SHADER(void, shader)

    const char* source = glshader->converted ? glshader->converted : glshader->source;
    if (!source) return;

    if (!parsedUniformsCache.contains(program)) {
        parsedUniformsCache[program] = make_unique<uniformInfo>();
    }
    auto& info = parsedUniformsCache.at(program);

    string src = source;

    // 1. Remove comments
    size_t p = 0;
    while ((p = src.find("//", p)) != string::npos) {
        size_t end = src.find('\n', p);
        if (end == string::npos) {
            src.erase(p);
            break;
        }
        src.erase(p, end - p);
    }
    p = 0;
    while ((p = src.find("/*", p)) != string::npos) {
        size_t end = src.find("*/", p);
        if (end == string::npos) {
            src.erase(p);
            break;
        }
        src.erase(p, end - p + 2);
    }

    static const string kUniformToken = "uniform";
    const size_t kTokenLen = kUniformToken.length();
    size_t pos = 0;

    while ((pos = src.find(kUniformToken, pos)) != string::npos) {
        if ((pos == 0 || !isIdChar(src[pos - 1])) &&
            (pos + kTokenLen >= src.length() || !isIdChar(src[pos + kTokenLen]))) {

            size_t endDecl = src.find(';', pos);
            if (endDecl != string::npos) {
                string decl = src.substr(pos + kTokenLen, endDecl - (pos + kTokenLen));

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
                        name = (lastSpace != string::npos) ? trim(s.substr(lastSpace + 1)) : s;
                        firstSegment = false;
                    } else {
                        name = s;
                    }

                    size_t bracket = name.find('[');
                    if (bracket != string::npos) name = trim(name.substr(0, bracket));

                    if (!name.empty() && !info->uniformNameToIndex.contains(name)) {
                        if (isUniformUsed(src, name)) {
                            info->uniformNameToIndex[name] = info->currentUniformIndex++;
                        }
                    }
                }
                pos = endDecl + 1;
            } else {
                pos += kTokenLen;
            }
        } else {
            pos += kTokenLen;
        }
    }
}
}