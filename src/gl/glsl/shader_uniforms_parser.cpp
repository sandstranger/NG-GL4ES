#include "shader_uniforms_parser.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <regex>
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

    const char* sourcePtr = glshader->converted ? glshader->converted : glshader->source;
    if (!sourcePtr) return;

    if (!parsedUniformsCache.contains(program)) {
        parsedUniformsCache[program] = make_unique<uniformInfo>();
    }
    auto& info = parsedUniformsCache.at(program);

    string src = sourcePtr;
    static const regex kDeclRegex(R"(\buniform\s+([^;]+);)");

    string usageBody = regex_replace(src, kDeclRegex, " ");

    auto decls_begin = sregex_iterator(src.begin(), src.end(), kDeclRegex);
    auto decls_end = sregex_iterator();

    for (auto i = decls_begin; i != decls_end; ++i) {
        smatch match = *i;
        string content = match[1].str();

        stringstream ss(content);
        string segment;
        bool firstSegment = true;
        while (getline(ss, segment, ',')) {
            string s = trim(segment);
            if (s.empty()) continue;

            string name;
            if (firstSegment) {
                size_t lastSpace = s.find_last_of(" \t\n\r");
                name = (lastSpace != string::npos) ? trim(s.substr(lastSpace + 1)) : s;
                firstSegment = false;
            } else {
                name = s;
            }

            size_t eq = name.find('=');
            if (eq != string::npos) name = trim(name.substr(0, eq));
            size_t bracket = name.find('[');
            if (bracket != string::npos) name = trim(name.substr(0, bracket));

            if (!name.empty() && !info->uniformNameToIndex.contains(name)) {
                regex usageRegex("\\b" + name + "\\b");
                if (regex_search(usageBody, usageRegex)) {
                    info->uniformNameToIndex[name] = info->currentUniformIndex++;
                }
            }
        }
    }
}
}
