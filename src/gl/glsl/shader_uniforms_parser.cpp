#include "shader_uniforms_parser.h"
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <memory>
#include <regex>
#include "../gl4es.h"

static unordered_map<GLuint, vector<string>> parsedUniformsCache;
static unordered_map<GLuint, size_t> programSamplerCount;

static string trim(const string& s) {
    size_t first = s.find_first_not_of(" \t\n\r");
    if (string::npos == first) return "";
    size_t last = s.find_last_not_of(" \t\n\r");
    return s.substr(first, (last - first + 1));
}

static bool isSamplerType(const string& type) {
    return type.find("sampler") != string::npos;
}

__attribute__((used)) __attribute__((visibility("default")))
vector<string> getUniforms(GLuint program) {
    if (parsedUniformsCache.contains(program)) {
        return parsedUniformsCache.at(program);
    }
    return {};
}

__attribute__((used)) __attribute__((visibility("default")))
void removeProgramFromCache(GLuint program) {
    parsedUniformsCache.erase(program);
    programSamplerCount.erase(program);
}

extern "C" {
__attribute__((used)) __attribute__((visibility("default")))
void parseUniformsFromShader(GLuint program, GLuint shader) {
    FLUSH_BEGINEND;
    CHECK_PROGRAM(void, program)
    CHECK_SHADER(void, shader)

    const char* sourcePtr = glshader->converted ? glshader->converted : glshader->source;
    if (!sourcePtr) return;

    if (!parsedUniformsCache.contains(program)) {
        parsedUniformsCache[program] = {};
        programSamplerCount[program] = 0;
    }
    auto& uniforms = parsedUniformsCache.at(program);

    string src = sourcePtr;
    static const regex kDeclRegex(R"(\buniform\s+([^;]+);)");

    string usageBody = regex_replace(src, kDeclRegex, " ");

    struct TempDecl {
        string name;
        bool isSampler;
    };
    vector<TempDecl> allFound;

    auto decls_begin = sregex_iterator(src.begin(), src.end(), kDeclRegex);
    auto decls_end = sregex_iterator();

    for (auto i = decls_begin; i != decls_end; ++i) {
        smatch match = *i;
        string content = match[1].str();

        stringstream ss(content);
        string segment;
        string currentType;
        bool firstSegment = true;
        while (getline(ss, segment, ',')) {
            string s = trim(segment);
            if (s.empty()) continue;

            string name;
            if (firstSegment) {
                size_t lastSpace = s.find_last_of(" \t\n\r");
                if (lastSpace != string::npos) {
                    currentType = trim(s.substr(0, lastSpace));
                    name = trim(s.substr(lastSpace + 1));
                } else {
                    name = s;
                }
                firstSegment = false;
            } else {
                name = s;
            }

            size_t eq = name.find('=');
            if (eq != string::npos) name = trim(name.substr(0, eq));
            size_t bracket = name.find('[');
            if (bracket != string::npos) name = trim(name.substr(0, bracket));

            if (!name.empty()) {
                regex usageRegex("\\b" + name + "\\b");
                if (regex_search(usageBody, usageRegex)) {
                    allFound.push_back({name, isSamplerType(currentType)});
                }
            }
        }
    }

    for (const auto& d : allFound) {
        if (d.isSampler) {
            if (find(uniforms.begin(), uniforms.end(), d.name) == uniforms.end()) {
                uniforms.insert(uniforms.begin() + programSamplerCount[program], d.name);
                programSamplerCount[program]++;
            }
        }
    }

    for (const auto& d : allFound) {
        if (!d.isSampler) {
            if (find(uniforms.begin(), uniforms.end(), d.name) == uniforms.end()) {
                uniforms.push_back(d.name);
            }
        }
    }
}
}
