#include "glsl_for_es.h"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Include/Types.h>
#include <spirv_cross/spirv_cross_c.h>
#include <iostream>
#include <fstream>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <string>
#include <algorithm>
#include <sstream>
#include <atomic>
#include <mutex>
#include "../../../version.h"
#include <spirv-tools/libspirv.hpp>
#include <spirv-tools/optimizer.hpp>

extern "C"
{
#include <stdbool.h>
#include "../logs.h"
}


#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

std::string GLSLtoGLSLES(const char *glsl_code, GLenum glsl_type, unsigned int essl_version,
                         unsigned int glsl_version,
                         int &return_code);

std::string GLSLtoGLSLES_2(const char *glsl_code, GLenum glsl_type, unsigned int essl_version,
                           int &return_code);

const char *atomicCounterEmulatedWatermark = "// Non-opaque atomic uniform converted to SSBO";


static const TBuiltInResource &GetCachedResources() {
    static TBuiltInResource Resources{};
    static std::once_flag init_flag;
    std::call_once(init_flag, []() {
        Resources.maxLights = 32;
        Resources.maxClipPlanes = 6;
        Resources.maxTextureUnits = 32;
        Resources.maxTextureCoords = 32;
        Resources.maxVertexAttribs = 64;
        Resources.maxVertexUniformComponents = 4096;
        Resources.maxVaryingFloats = 64;
        Resources.maxVertexTextureImageUnits = 32;
        Resources.maxCombinedTextureImageUnits = 80;
        Resources.maxTextureImageUnits = 32;
        Resources.maxFragmentUniformComponents = 4096;
        Resources.maxDrawBuffers = 32;
        Resources.maxVertexUniformVectors = 128;
        Resources.maxVaryingVectors = 8;
        Resources.maxFragmentUniformVectors = 16;
        Resources.maxVertexOutputVectors = 16;
        Resources.maxFragmentInputVectors = 15;
        Resources.minProgramTexelOffset = -8;
        Resources.maxProgramTexelOffset = 7;
        Resources.maxClipDistances = 8;
        Resources.maxComputeWorkGroupCountX = 65535;
        Resources.maxComputeWorkGroupCountY = 65535;
        Resources.maxComputeWorkGroupCountZ = 65535;
        Resources.maxComputeWorkGroupSizeX = 1024;
        Resources.maxComputeWorkGroupSizeY = 1024;
        Resources.maxComputeWorkGroupSizeZ = 64;
        Resources.maxComputeUniformComponents = 1024;
        Resources.maxComputeTextureImageUnits = 16;
        Resources.maxComputeImageUniforms = 8;
        Resources.maxComputeAtomicCounters = 8;
        Resources.maxComputeAtomicCounterBuffers = 1;
        Resources.maxVaryingComponents = 60;
        Resources.maxVertexOutputComponents = 64;
        Resources.maxGeometryInputComponents = 64;
        Resources.maxGeometryOutputComponents = 128;
        Resources.maxFragmentInputComponents = 128;
        Resources.maxImageUnits = 8;
        Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
        Resources.maxCombinedShaderOutputResources = 8;
        Resources.maxImageSamples = 0;
        Resources.maxVertexImageUniforms = 0;
        Resources.maxTessControlImageUniforms = 0;
        Resources.maxTessEvaluationImageUniforms = 0;
        Resources.maxGeometryImageUniforms = 0;
        Resources.maxFragmentImageUniforms = 8;
        Resources.maxCombinedImageUniforms = 8;
        Resources.maxGeometryTextureImageUnits = 16;
        Resources.maxGeometryOutputVertices = 256;
        Resources.maxGeometryTotalOutputComponents = 1024;
        Resources.maxGeometryUniformComponents = 1024;
        Resources.maxGeometryVaryingComponents = 64;
        Resources.maxTessControlInputComponents = 128;
        Resources.maxTessControlOutputComponents = 128;
        Resources.maxTessControlTextureImageUnits = 16;
        Resources.maxTessControlUniformComponents = 1024;
        Resources.maxTessControlTotalOutputComponents = 4096;
        Resources.maxTessEvaluationInputComponents = 128;
        Resources.maxTessEvaluationOutputComponents = 128;
        Resources.maxTessEvaluationTextureImageUnits = 16;
        Resources.maxTessEvaluationUniformComponents = 1024;
        Resources.maxTessPatchComponents = 120;
        Resources.maxPatchVertices = 32;
        Resources.maxTessGenLevel = 64;
        Resources.maxViewports = 16;
        Resources.maxVertexAtomicCounters = 0;
        Resources.maxTessControlAtomicCounters = 0;
        Resources.maxTessEvaluationAtomicCounters = 0;
        Resources.maxGeometryAtomicCounters = 0;
        Resources.maxFragmentAtomicCounters = 8;
        Resources.maxCombinedAtomicCounters = 8;
        Resources.maxAtomicCounterBindings = 1;
        Resources.maxVertexAtomicCounterBuffers = 0;
        Resources.maxTessControlAtomicCounterBuffers = 0;
        Resources.maxTessEvaluationAtomicCounterBuffers = 0;
        Resources.maxGeometryAtomicCounterBuffers = 0;
        Resources.maxFragmentAtomicCounterBuffers = 1;
        Resources.maxCombinedAtomicCounterBuffers = 1;
        Resources.maxAtomicCounterBufferSize = 16384;
        Resources.maxTransformFeedbackBuffers = 4;
        Resources.maxTransformFeedbackInterleavedComponents = 64;
        Resources.maxCullDistances = 8;
        Resources.maxCombinedClipAndCullDistances = 8;
        Resources.maxSamples = 4;
        Resources.maxMeshOutputVerticesNV = 256;
        Resources.maxMeshOutputPrimitivesNV = 512;
        Resources.maxMeshWorkGroupSizeX_NV = 32;
        Resources.maxMeshWorkGroupSizeY_NV = 1;
        Resources.maxMeshWorkGroupSizeZ_NV = 1;
        Resources.maxTaskWorkGroupSizeX_NV = 32;
        Resources.maxTaskWorkGroupSizeY_NV = 1;
        Resources.maxTaskWorkGroupSizeZ_NV = 1;
        Resources.maxMeshViewCountNV = 4;

        Resources.limits.nonInductiveForLoops = true;
        Resources.limits.whileLoops = true;
        Resources.limits.doWhileLoops = true;
        Resources.limits.generalUniformIndexing = true;
        Resources.limits.generalAttributeMatrixVectorIndexing = true;
        Resources.limits.generalVaryingIndexing = true;
        Resources.limits.generalSamplerIndexing = true;
        Resources.limits.generalVariableIndexing = true;
        Resources.limits.generalConstantMatrixVectorIndexing = true;
    });
    return Resources;
}


int getGLSLVersion(const char *glsl_code) {
    const char *p = glsl_code;
    while (*p) {
        if (p[0] == '#' && strncmp(p, "#version", 8) == 0) {
            p += 8;
            while (*p == ' ' || *p == '\t') p++;
            int version = 0;
            while (*p >= '0' && *p <= '9') {
                version = version * 10 + (*p - '0');
                p++;
            }
            if (version > 0) return version;
        }
        p++;
    }
    return -1;
}


std::string forceSupporterOutput(const std::string &glslCode) {
    bool hasPrecisionFloat = (glslCode.find("precision ") != std::string::npos &&
                              glslCode.find("float;") != std::string::npos);
    bool hasPrecisionInt = (glslCode.find("precision ") != std::string::npos &&
                            glslCode.find("int;") != std::string::npos);

    std::string result;
    result.reserve(glslCode.size() + 64);

    std::string precisionFloat = hasPrecisionFloat ? "" : "precision highp float;\n";
    std::string precisionInt = hasPrecisionInt ? "" : "precision highp int;\n";

    if (hasPrecisionFloat && hasPrecisionInt) {

        size_t pos = 0;
        while (pos < glslCode.size()) {
            size_t line_start = pos;
            size_t line_end = glslCode.find('\n', pos);
            if (line_end == std::string::npos) line_end = glslCode.size();

            std::string line = glslCode.substr(line_start, line_end - line_start);
            bool isPrecisionLine = (line.find("precision ") != std::string::npos) &&
                                   (line.find("float;") != std::string::npos ||
                                    line.find("int;") != std::string::npos);

            if (!isPrecisionLine) {
                if (!result.empty()) result += '\n';
                result += line;
            }

            pos = line_end + 1;
        }
        precisionFloat = "precision highp float;\n";
        precisionInt = "precision highp int;\n";
    } else {
        result = glslCode;
    }

    size_t lastExtensionPos = result.rfind("#extension");
    size_t insertionPos = 0;

    if (lastExtensionPos != std::string::npos) {
        size_t nextNewline = result.find('\n', lastExtensionPos);
        if (nextNewline != std::string::npos) {
            insertionPos = nextNewline + 1;
        } else {
            insertionPos = result.length();
        }
    } else {
        size_t firstNewline = result.find('\n');
        if (firstNewline != std::string::npos) {
            insertionPos = firstNewline + 1;
        } else {
            result = precisionFloat + precisionInt + result;
            return result;
        }
    }

    result.insert(insertionPos, precisionFloat + precisionInt);
    return result;
}


std::string removeLayoutBinding(const std::string &glslCode) {
    std::string result;
    result.reserve(glslCode.size());

    size_t pos = 0;
    while (pos < glslCode.size()) {
        size_t layout_pos = glslCode.find("layout(", pos);
        if (layout_pos == std::string::npos) {
            result.append(glslCode, pos, std::string::npos);
            break;
        }

        result.append(glslCode, pos, layout_pos - pos);

        size_t layout_end = glslCode.find(')', layout_pos);
        if (layout_end == std::string::npos) {
            result.append(glslCode, layout_pos, std::string::npos);
            break;
        }

        std::string layout_content = glslCode.substr(layout_pos + 7, layout_end - layout_pos - 7);


        size_t binding_pos = layout_content.find("binding");
        if (binding_pos != std::string::npos) {
            size_t binding_end = binding_pos;

            binding_end += 7;

            while (binding_end < layout_content.size() &&
                   (layout_content[binding_end] == ' ' || layout_content[binding_end] == '\t')) {
                binding_end++;
            }

            if (binding_end < layout_content.size() && layout_content[binding_end] == '=') {
                binding_end++;
            }

            while (binding_end < layout_content.size() &&
                   (layout_content[binding_end] == ' ' || layout_content[binding_end] == '\t')) {
                binding_end++;
            }

            while (binding_end < layout_content.size() &&
                   layout_content[binding_end] >= '0' && layout_content[binding_end] <= '9') {
                binding_end++;
            }

            if (binding_end < layout_content.size() && layout_content[binding_end] == ',') {
                binding_end++;

                while (binding_end < layout_content.size() &&
                       (layout_content[binding_end] == ' ' ||
                        layout_content[binding_end] == '\t')) {
                    binding_end++;
                }
            }


            std::string new_layout = layout_content.substr(0, binding_pos) +
                                     layout_content.substr(binding_end);


            size_t start = 0;
            while (start < new_layout.size() &&
                   (new_layout[start] == ' ' || new_layout[start] == '\t' ||
                    new_layout[start] == ',')) {
                start++;
            }
            new_layout = new_layout.substr(start);

            if (!new_layout.empty()) {
                result += "layout(" + new_layout + ")";
            }
        } else {
            result.append(glslCode, layout_pos, layout_end - layout_pos + 1);
        }

        pos = layout_end + 1;
    }

    return result;
}

void trim(std::string &str) {
    str.erase(str.begin(),
              std::find_if(str.begin(), str.end(), [](int ch) { return !std::isspace(ch); }));
    str.erase(
            std::find_if(str.rbegin(), str.rend(), [](int ch) { return !std::isspace(ch); }).base(),
            str.end());
}


std::string process_uniform_declarations(const std::string &glslCode) {
    std::string result;
    result.reserve(glslCode.length());

    size_t scan_pos = 0;
    size_t chunk_start = 0;
    const size_t length = glslCode.length();
    const std::vector<std::string> precision_kws = {"highp", "lowp", "mediump"};

    while (scan_pos < length) {
        if (glslCode.compare(scan_pos, 7, "uniform") == 0) {
            if (scan_pos > chunk_start) {
                result.append(glslCode, chunk_start, scan_pos - chunk_start);
            }

            const size_t decl_start = scan_pos;
            scan_pos += 7;

            std::string precision, type;
            bool found_precision = false;

            while (scan_pos < length) {
                while (scan_pos < length && std::isspace(glslCode[scan_pos]))
                    ++scan_pos;

                for (const auto &kw: precision_kws) {
                    if (glslCode.compare(scan_pos, kw.length(), kw) == 0) {
                        precision = " " + kw;
                        scan_pos += kw.length();
                        found_precision = true;
                        break;
                    }
                }
                if (found_precision) break;

                const size_t type_start = scan_pos;
                while (scan_pos < length &&
                       (std::isalnum(glslCode[scan_pos]) || glslCode[scan_pos] == '_')) {
                    ++scan_pos;
                }
                type = glslCode.substr(type_start, scan_pos - type_start);
                break;
            }

            while (scan_pos < length) {
                while (scan_pos < length && std::isspace(glslCode[scan_pos]))
                    ++scan_pos;

                bool found = false;
                for (const auto &kw: precision_kws) {
                    if (glslCode.compare(scan_pos, kw.length(), kw) == 0) {
                        if (precision.empty()) precision = " " + kw;
                        scan_pos += kw.length();
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            }

            if (type.empty()) {
                const size_t type_start = scan_pos;
                while (scan_pos < length &&
                       (std::isalnum(glslCode[scan_pos]) || glslCode[scan_pos] == '_')) {
                    ++scan_pos;
                }
                type = glslCode.substr(type_start, scan_pos - type_start);
            }

            while (scan_pos < length && std::isspace(glslCode[scan_pos]))
                ++scan_pos;
            const size_t name_start = scan_pos;
            while (scan_pos < length &&
                   (std::isalnum(glslCode[scan_pos]) || glslCode[scan_pos] == '_')) {
                ++scan_pos;
            }
            const std::string name = glslCode.substr(name_start, scan_pos - name_start);

            size_t decl_end = glslCode.find(';', scan_pos);
            if (decl_end == std::string::npos)
                decl_end = length;
            else
                ++decl_end;

            const bool has_initializer = (glslCode.find('=', scan_pos) < decl_end);
            if (has_initializer) {
                result.append("uniform").append(precision).append(" ").append(type).append(
                        " ").append(name).append(";");
            } else {
                result.append(glslCode, decl_start, decl_end - decl_start);
            }

            scan_pos = chunk_start = decl_end;
        } else {
            ++scan_pos;
        }
    }

    if (chunk_start < length) {
        result.append(glslCode, chunk_start, length - chunk_start);
    }

    return result;
}


std::string processOutColorLocations(const std::string &glslCode) {
    std::string result;
    result.reserve(glslCode.size() + 64);

    size_t pos = 0;
    while (pos < glslCode.size()) {
        size_t out_pos = glslCode.find("\nout highp vec4 outColor", pos);
        if (out_pos == std::string::npos) {
            result.append(glslCode, pos, std::string::npos);
            break;
        }

        result.append(glslCode, pos, out_pos - pos);

        size_t num_start = out_pos + 24;
        size_t num_end = num_start;
        while (num_end < glslCode.size() && glslCode[num_end] >= '0' && glslCode[num_end] <= '9') {
            num_end++;
        }

        if (num_end > num_start && num_end < glslCode.size() && glslCode[num_end] == ';') {
            std::string num = glslCode.substr(num_start, num_end - num_start);
            result += "\nlayout(location=" + num + ") out highp vec4 outColor" + num + ";";
            pos = num_end + 1;
        } else {
            result.append(glslCode, out_pos, 1);
            pos = out_pos + 1;
        }
    }

    return result;
}

bool checkIfAtomicCounterBufferEmulated(const std::string &glslCode) {
    return glslCode.find(atomicCounterEmulatedWatermark) != std::string::npos;
}

__attribute__((used)) __attribute__((visibility("default")))
extern "C" char *GLSLtoGLSLES_c(const char *glsl_code, GLenum glsl_type, unsigned int essl_version,
                                unsigned int glsl_version,
                                int *return_code) {
    int tmp_return_code = 0;
    std::string shader = glsl_code;


    size_t pos = 0;
    while ((pos = shader.find("#version", pos)) != std::string::npos) {
        size_t end_pos = shader.find('\n', pos);
        if (end_pos == std::string::npos) end_pos = shader.size();

        std::string line = shader.substr(pos, end_pos - pos);
        if (line.find("300 es") != std::string::npos ||
            line.find("310 es") != std::string::npos ||
            line.find("320 es") != std::string::npos) {
            shader.replace(pos, end_pos - pos, "#version 410");
        }
        pos = end_pos + 1;
    }

    std::string result = GLSLtoGLSLES(shader.c_str(), glsl_type, essl_version, glsl_version,
                                      tmp_return_code);
    *return_code = tmp_return_code;

    char *cstr = (char *) malloc(result.size() + 1);
    if (!cstr) {
        *return_code = -1;
        return nullptr;
    }
    memcpy(cstr, result.c_str(), result.size() + 1);
    return cstr;
}

std::string GLSLtoGLSLES(const char *glsl_code, GLenum glsl_type, unsigned int essl_version,
                         unsigned int glsl_version,
                         int &return_code) {
    return_code = -1;
    std::string converted = GLSLtoGLSLES_2(glsl_code, glsl_type, essl_version, return_code);
    return (return_code >= 0) ? converted : glsl_code;
}

std::string replace_line_starting_with(const std::string &glslCode, const std::string &starting,
                                       const std::string &substitution = "") {
    std::string result;
    result.reserve(glslCode.size());

    size_t length = glslCode.size();
    size_t start = 0;
    size_t current = 0;

    auto append_chunk = [&](size_t end) {
        if (end > start) {
            result.append(glslCode, start, end - start);
        }
    };

    while (current < length) {
        size_t lineStart = current;
        while (current < length && (glslCode[current] == ' ' || glslCode[current] == '\t')) {
            current++;
        }

        bool isLineDirective = false;
        if (current + 5 <= length && glslCode.compare(current, 5, "#line") == 0) {
            isLineDirective = true;
        }

        while (current < length && glslCode[current] != '\r' && glslCode[current] != '\n') {
            current++;
        }

        size_t newlineLength = 0;
        if (current < length) {
            if (glslCode[current] == '\r') {
                newlineLength = (current + 1 < length && glslCode[current + 1] == '\n') ? 2 : 1;
            } else {
                newlineLength = 1;
            }
        }

        if (isLineDirective) {
            append_chunk(lineStart);
            current += newlineLength;
            start = current;
            result += substitution;
        } else {
            current += newlineLength;
        }
    }

    append_chunk(current);
    return result;
}

static inline void replace_all(std::string &str, const std::string &from, const std::string &to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

static size_t find_insertion_point(const std::string &glsl) {
    size_t pos = 0;
    size_t insertion_point = 0;

    size_t version_pos = glsl.find("#version");
    if (version_pos != std::string::npos) {
        size_t version_end = glsl.find('\n', version_pos);
        if (version_end == std::string::npos) {
            version_end = glsl.length();
        } else {
            version_end++;
        }
        insertion_point = version_end;
        pos = version_end;
    }

    while (pos < glsl.length()) {
        size_t line_begin = pos;
        while (pos < glsl.length() && std::isspace(glsl[pos])) {
            pos++;
        }
        if (pos >= glsl.length()) break;

        if (glsl[pos] == '#') {
            pos++;
            while (pos < glsl.length() && std::isspace(glsl[pos])) {
                pos++;
            }
            if (glsl.compare(pos, 9, "extension") == 0) {
                size_t ext_end = glsl.find('\n', pos);
                if (ext_end == std::string::npos) {
                    ext_end = glsl.length();
                } else {
                    ext_end++;
                }
                insertion_point = ext_end;
                pos = ext_end;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    return insertion_point;
}


bool process_non_opaque_atomic_to_ssbo(std::string &source) {
    if (source.find("atomicCounter") == std::string::npos) return false;

    std::set<std::string> atomic_vars;
    std::map<std::string, std::string> binding_map;


    size_t pos = 0;
    while ((pos = source.find("uniform atomic_uint", pos)) != std::string::npos) {

        size_t layout_start = source.rfind("layout(", pos);
        if (layout_start == std::string::npos || pos - layout_start > 100) {
            pos += 18;
            continue;
        }

        size_t layout_end = source.find(')', layout_start);
        if (layout_end == std::string::npos || layout_end > pos) {
            pos += 18;
            continue;
        }

        std::string layout_content = source.substr(layout_start + 7, layout_end - layout_start - 7);


        size_t binding_pos = layout_content.find("binding");
        if (binding_pos == std::string::npos) {
            pos += 18;
            continue;
        }

        size_t num_start = binding_pos + 7;
        while (num_start < layout_content.size() &&
               (layout_content[num_start] == ' ' || layout_content[num_start] == '\t' ||
                layout_content[num_start] == '=')) {
            num_start++;
        }

        size_t num_end = num_start;
        while (num_end < layout_content.size() &&
               layout_content[num_end] >= '0' && layout_content[num_end] <= '9') {
            num_end++;
        }

        if (num_end == num_start) {
            pos += 18;
            continue;
        }

        std::string binding = layout_content.substr(num_start, num_end - num_start);


        size_t var_start = pos + 18;
        while (var_start < source.size() && std::isspace(source[var_start])) {
            var_start++;
        }

        size_t var_end = var_start;
        while (var_end < source.size() &&
               (std::isalnum(source[var_end]) || source[var_end] == '_')) {
            var_end++;
        }

        if (var_end == var_start) {
            pos += 18;
            continue;
        }

        std::string var = source.substr(var_start, var_end - var_start);


        size_t semicolon = source.find(';', var_end);
        if (semicolon == std::string::npos) {
            pos += 18;
            continue;
        }

        atomic_vars.insert(var);
        binding_map[var] = binding;

        std::string repl =
                "layout(std430, binding=" + binding + ") buffer AtomicCounterSSBO_" + binding +
                " {\n    uint " + var + ";\n};\n";

        source.replace(layout_start, semicolon - layout_start + 1, repl);
        pos = layout_start + repl.size();
    }

    if (atomic_vars.empty()) return true;

    for (auto &var: atomic_vars) {

        std::string search = "atomicCounterIncrement(" + var + ")";
        std::string replace = "atomicAdd(" + var + ", 1u)";
        size_t p = 0;
        while ((p = source.find(search, p)) != std::string::npos) {
            source.replace(p, search.size(), replace);
            p += replace.size();
        }


        search = "atomicCounterDecrement(" + var + ")";
        replace = "atomicAdd(" + var + ", uint(-1))";
        p = 0;
        while ((p = source.find(search, p)) != std::string::npos) {
            source.replace(p, search.size(), replace);
            p += replace.size();
        }


        search = "atomicCounterAdd(" + var + ",";
        replace = "atomicAdd(" + var + ",";
        p = 0;
        while ((p = source.find(search, p)) != std::string::npos) {
            source.replace(p, search.size(), replace);
            p += replace.size();
        }


        search = "atomicCounter(" + var + ")";
        p = 0;
        while ((p = source.find(search, p)) != std::string::npos) {
            source.replace(p, search.size(), var);
            p += var.size();
        }
    }


    size_t p = 0;
    while ((p = source.find("atomicAdd(", p)) != std::string::npos) {
        size_t semicolon = source.find(';', p);
        if (semicolon != std::string::npos) {
            source.insert(semicolon + 1, "\n    memoryBarrierBuffer();");
            p = semicolon + 25;
        } else {
            p += 10;
        }
    }

    source += "\n" + std::string(atomicCounterEmulatedWatermark);
    return true;
}


void process_sampler_buffer(std::string &source) {
    if (source.find("isamplerBuffer") == std::string::npos) {
        return;
    }

    size_t pos = 0;
    while ((pos = source.find("isamplerBuffer", pos)) != std::string::npos) {
        source.replace(pos, 14, "isampler2D");
        pos += 11;
    }


    pos = 0;
    while ((pos = source.find("texelFetch(", pos)) != std::string::npos) {
        size_t args_start = pos + 11;
        size_t args_end = source.find(')', args_start);
        if (args_end == std::string::npos) {
            pos += 11;
            continue;
        }

        std::string args = source.substr(args_start, args_end - args_start);
        size_t comma = args.find(',');
        if (comma == std::string::npos) {
            pos += 11;
            continue;
        }

        std::string name = args.substr(0, comma);
        std::string index = args.substr(comma + 1);


        while (!name.empty() && std::isspace(name.back())) name.pop_back();
        while (!index.empty() && std::isspace(index.front())) index = index.substr(1);

        std::string replacement =
                "texelFetch(" + name + ", ivec2((" + index + ") % u_BufferTexWidth, (" +
                index + ") / u_BufferTexWidth), 0)";

        source.replace(pos, args_end - pos + 1, replacement);
        pos += replacement.size();
    }

    const char *boundaryProtection = R"(
ivec2 bufferCoords(int index) {
    int width = u_BufferTexWidth;
    int x = index % width;
    int y = index / width;
    if (y >= u_BufferTexHeight) {
        y = u_BufferTexHeight - 1;
        x = width - 1;
    }
    return ivec2(x, y);
}
)";


    pos = 0;
    while ((pos = source.find("texelFetch(", pos)) != std::string::npos) {
        size_t args_start = pos + 11;
        size_t args_end = source.find(')', args_start);
        if (args_end == std::string::npos) {
            pos += 11;
            continue;
        }

        std::string args = source.substr(args_start, args_end - args_start);
        size_t comma = args.find(',');
        if (comma == std::string::npos) {
            pos += 11;
            continue;
        }

        std::string name = args.substr(0, comma);
        std::string coords = args.substr(comma + 1);

        if (coords.find("ivec2(") != std::string::npos) {
            size_t ivec_start = coords.find("ivec2(") + 6;
            size_t ivec_end = coords.rfind(')');
            if (ivec_end != std::string::npos) {
                std::string index = coords.substr(ivec_start, ivec_end - ivec_start);
                std::string replacement =
                        "texelFetch(" + name + ", bufferCoords(" + index + "), 0)";
                source.replace(pos, args_end - pos + 1, replacement);
                pos += replacement.size();
                continue;
            }
        }
        pos += 11;
    }

    size_t insertion_point = find_insertion_point(source);
    if (insertion_point != std::string::npos) {
        source.insert(insertion_point, boundaryProtection);
    }

    const char *uniformDecl = R"(
uniform int u_BufferTexWidth;
uniform int u_BufferTexHeight;
)";

    insertion_point = find_insertion_point(source);
    if (insertion_point != std::string::npos) {
        insertion_point = source.find('\n', insertion_point);
        if (insertion_point != std::string::npos) {
            source.insert(insertion_point + 1, uniformDecl);
        }
    }
}


static void inject_textureQueryLod(std::string &glsl) {
    if (glsl.find("textureQueryLod") == std::string::npos) {
        return;
    }
    if (glsl.find("mg_textureQueryLod") != std::string::npos) {
        return;
    }

    const std::string textureQueryLodImpl = R"(
#define textureQueryLod mg_textureQueryLod

vec2 mg_textureQueryLod(sampler2D tex, vec2 uv) {
    vec2 texSizeF = vec2(textureSize(tex, 0));
    vec2 dFdx_uv = dFdx(uv * texSizeF);
    vec2 dFdy_uv = dFdy(uv * texSizeF);
    float maxDerivative = max(length(dFdx_uv), length(dFdy_uv));
    float lod = log2(maxDerivative);
    return vec2(lod);
}
)";

    size_t insertPos = find_insertion_point(glsl);
    glsl.insert(insertPos, "\n" + textureQueryLodImpl + "\n");
}


static inline void inject_temporal_filter(std::string &glsl) {
    if (glsl.find("GI_TemporalFilter") == std::string::npos) {
        return;
    }
    if (glsl.find("vec4 GI_TemporalFilter()") != std::string::npos) {
        return;
    }


    size_t insertPos = 0;
    size_t pos = 0;
    while ((pos = glsl.find("uniform", pos)) != std::string::npos) {
        size_t semicolon = glsl.find(';', pos);
        if (semicolon != std::string::npos) {
            insertPos = semicolon + 1;
            pos = semicolon + 1;
        } else {
            break;
        }
    }

    const std::string GI_TemporalFilterImpl = R"(
vec4 GI_TemporalFilter() {
    vec2 uv = gl_FragCoord.xy / screenSize;
    uv += taaJitter * pixelSize;
    vec4 currentGI = texture(colortex0, uv);
    float depth = texture(depthtex0, uv).r;
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewPos = gbufferProjectionInverse * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = gbufferModelViewInverse * viewPos;
    vec4 prevClipPos = gbufferPreviousProjection * (gbufferPreviousModelView * worldPos);
    prevClipPos /= prevClipPos.w;
    vec2 prevUV = prevClipPos.xy * 0.5 + 0.5;
    vec4 historyGI = texture(colortex1, prevUV);
    float difference = length(currentGI.rgb - historyGI.rgb);
    float thresholdValue = 0.1;
    float adaptiveBlend = mix(0.9, 0.0, smoothstep(thresholdValue, thresholdValue * 2.0, difference));
    vec4 filteredGI = mix(currentGI, historyGI, adaptiveBlend);
    if (difference > thresholdValue * 2.0) {
        filteredGI = currentGI;
    }
    return filteredGI;
}
)";
    glsl.insert(insertPos, "\n" + GI_TemporalFilterImpl + "\n");
}

#define xstr(s) str(s)
#define str(s) #s

void inject_ngg_macro_definition(std::string &glslCode) {
    std::string macro_definitions = "\n#define NGG_NGG\n"
                                    "#define NGG_NGG_VERSION " xstr(MAJOR) xstr(MINOR) xstr(
                                            REVISION) "\n";

    size_t versionPos = glslCode.rfind("#version");
    size_t insertionPos = 0;

    if (versionPos != std::string::npos) {
        size_t nextNewline = glslCode.find('\n', versionPos);
        insertionPos = (nextNewline != std::string::npos) ? nextNewline + 1 : glslCode.length();
    } else {
        size_t firstNewline = glslCode.find('\n');
        insertionPos = (firstNewline != std::string::npos) ? firstNewline + 1 : 0;
    }

    glslCode.insert(insertionPos, macro_definitions);
}

std::string
preprocess_glsl(const std::string &glsl, GLenum shaderType, bool *atomicCounterEmulated) {
    std::string ret = glsl;
    ret = replace_line_starting_with(ret, "#line");
    replace_all(ret, "#ifdef GL_ARB_derivative_control", "#if 0");
    replace_all(ret, "#ifndef GL_ARB_derivative_control", "#if 1");

    replace_all(ret, "const mat3 rotInverse = transpose(rot);",
                "const mat3 rotInverse = mat3(rot[0][0], rot[1][0], rot[2][0], rot[0][1], rot[1][1], rot[2][1], "
                "rot[0][2], rot[1][2], rot[2][2]);");

    inject_temporal_filter(ret);

    if (1) {
        inject_textureQueryLod(ret);
    }

    inject_ngg_macro_definition(ret);

    if (0) {
        process_sampler_buffer(ret);
    }

    *atomicCounterEmulated = process_non_opaque_atomic_to_ssbo(ret);
    return ret;
}

int get_or_add_glsl_version(std::string &glsl) {
    int glsl_version = getGLSLVersion(glsl.c_str());
    if (glsl_version == -1) {
        glsl_version = 140;
        glsl.insert(0, "#version 140\n");
    }
    return glsl_version;
}


std::vector<unsigned int>
glsl_to_spirv(GLenum shader_type, int glsl_version, const char *const *shader_src,
              int &errc) {
    EShLanguage shader_language;
    switch (shader_type) {
        case GL_VERTEX_SHADER:
            shader_language = EShLanguage::EShLangVertex;
            break;
        case GL_FRAGMENT_SHADER:
            shader_language = EShLanguage::EShLangFragment;
            break;
        case GL_COMPUTE_SHADER:
            shader_language = EShLanguage::EShLangCompute;
            break;
        case GL_TESS_CONTROL_SHADER:
            shader_language = EShLanguage::EShLangTessControl;
            break;
        case GL_TESS_EVALUATION_SHADER:
            shader_language = EShLanguage::EShLangTessEvaluation;
            break;
        case GL_GEOMETRY_SHADER:
            shader_language = EShLanguage::EShLangGeometry;
            break;
        default:
            errc = -1;
            return {};
    }

    glslang::TShader shader(shader_language);
    shader.setStrings(shader_src, 1);

    using namespace glslang;
    shader.setEnvInput(EShSourceGlsl, shader_language, EShClientVulkan, glsl_version);
    shader.setEnvClient(EShClientOpenGL, EShTargetOpenGL_450);
    shader.setEnvTarget(EShTargetSpv, EShTargetSpv_1_5);
    shader.setAutoMapLocations(true);
    shader.setAutoMapBindings(true);

    const TBuiltInResource &resources = GetCachedResources();

    if (!shader.parse(&resources, glsl_version, true, EShMsgDefault)) {
        DBG(SHUT_LOGD("glslang: GLSL Compiling ERROR: \n%s", shader.getInfoLog());)
        errc = -1;
        return {};
    }
    DBG(SHUT_LOGD("GLSL Compiled to SPIRV.");)

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(EShMsgDefault)) {
        DBG(SHUT_LOGD("glslang: GLSL Linking ERROR: \n%s", program.getInfoLog());)
        errc = -1;
        return {};
    }
    DBG(SHUT_LOGD("GLSL Linked.");)

    std::vector<unsigned int> spirv_code;
    glslang::SpvOptions spvOptions;
    spvOptions.disableOptimizer = false;
    spvOptions.optimizeSize = true;
    spvOptions.stripDebugInfo = false;
    spvOptions.optimizerAllowExpandedIDBound = true;
    glslang::GlslangToSpv(*program.getIntermediate(shader_language), spirv_code, &spvOptions);
    errc = 0;
    return spirv_code;
}


std::string
spirv_to_essl(const std::vector<uint32_t> &spirv, unsigned int essl_version, int &errc) {

    thread_local spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_5);
    thread_local bool optimizer_initialized = false;

    if (!optimizer_initialized) {
        optimizer.RegisterLegalizationPasses();
        optimizer.RegisterPerformancePasses(false);
        optimizer_initialized = true;
    }

    std::vector<uint32_t> optimized;
    bool ok = optimizer.Run(spirv.data(), spirv.size(), &optimized);
    if (!ok) {
        errc = -1;
        return "";
    }

    spvc_context context = nullptr;
    spvc_parsed_ir ir = nullptr;
    spvc_compiler compiler_glsl = nullptr;
    spvc_compiler_options options = nullptr;
    spvc_resources resources = nullptr;
    const char *result = nullptr;


    spvc_result spvc_err = spvc_context_create(&context);
    if (spvc_err != SPVC_SUCCESS || !context) {
        DBG(SHUT_LOGD("Error: spvc_context_create failed: %d", spvc_err));
        errc = -1;
        return "";
    }

    spvc_err = spvc_context_parse_spirv(context, optimized.data(), optimized.size(), &ir);
    if (spvc_err != SPVC_SUCCESS || !ir) {
        DBG(SHUT_LOGD("Error: spvc_context_parse_spirv failed: %d", spvc_err));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    spvc_err = spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir,
                                            SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler_glsl);
    if (spvc_err != SPVC_SUCCESS || !compiler_glsl) {
        DBG(SHUT_LOGD("Error: spvc_context_create_compiler failed: %d", spvc_err));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    spvc_err = spvc_compiler_create_shader_resources(compiler_glsl, &resources);
    if (spvc_err != SPVC_SUCCESS) {
        DBG(SHUT_LOGD("Error: spvc_compiler_create_shader_resources failed: %d", spvc_err));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    spvc_err = spvc_compiler_create_compiler_options(compiler_glsl, &options);
    if (spvc_err != SPVC_SUCCESS || !options) {
        DBG(SHUT_LOGD("Error: spvc_compiler_create_compiler_options failed: %d", spvc_err));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    spvc_err = spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION,
                                              essl_version >= 300 ? essl_version : 300);
    if (spvc_err != SPVC_SUCCESS) {
        DBG(SHUT_LOGD("Error: spvc_compiler_options_set_uint failed: %d", spvc_err));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    spvc_err = spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);
    if (spvc_err != SPVC_SUCCESS) {
        DBG(SHUT_LOGD("Error: spvc_compiler_options_set_bool failed: %d", spvc_err));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    spvc_err = spvc_compiler_install_compiler_options(compiler_glsl, options);
    if (spvc_err != SPVC_SUCCESS) {
        DBG(SHUT_LOGD("Error: spvc_compiler_install_compiler_options failed: %d", spvc_err));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    spvc_err = spvc_compiler_compile(compiler_glsl, &result);
    if (spvc_err != SPVC_SUCCESS || !result) {
        DBG(SHUT_LOGD("Error: spvc_compiler_compile failed: %d", spvc_err));
        errc = -1;
        spvc_context_destroy(context);
        return "";
    }

    std::string essl = result;
    spvc_context_destroy(context);
    errc = 0;
    return essl;
}


static std::once_flag glslang_init_flag;

std::string GLSLtoGLSLES_2(const char *glsl_code, GLenum glsl_type, unsigned int essl_version,
                           int &return_code) {
    std::call_once(glslang_init_flag, []() {
        glslang::InitializeProcess();
    });

    bool atomicCounterEmulated = false;
    std::string correct_glsl_str = preprocess_glsl(glsl_code, glsl_type, &atomicCounterEmulated);
    DBG(SHUT_LOGD("Firstly converted GLSL:\n%s", correct_glsl_str.c_str()))
    int glsl_version = get_or_add_glsl_version(correct_glsl_str);

    const char *s[] = {correct_glsl_str.c_str()};
    int errc = 0;
    std::vector<unsigned int> spirv_code = glsl_to_spirv(glsl_type, glsl_version, s, errc);
    if (errc != 0) {
        return_code = -1;
        DBG(SHUT_LOGD("Error compiling GLSL to SPIR-V: %d", errc))
        return "";
    }
    errc = 0;
    std::string essl = spirv_to_essl(spirv_code, essl_version, errc);
    if (errc != 0) {
        return_code = -2;
        return "";
    }

    essl = processOutColorLocations(essl);
    essl = forceSupporterOutput(essl);

    DBG(SHUT_LOGD("Originally GLSL to GLSL ES Complete: \n%s", essl.c_str()))
    return_code = errc;
    if (return_code == 0) {
        return_code = atomicCounterEmulated ? 1 : 0;
    }
    return essl;
}

static bool starts_with(const std::string &s, const char *p) {
    return s.compare(0, strlen(p), p) == 0;
}

static bool is_word_char(char c) {
    return std::isalnum((unsigned char) c) || c == '_';
}

static std::string
replace_all_word(const std::string &s, const std::string &from, const std::string &to) {
    if (from.empty()) return s;

    std::string out;
    out.reserve(s.size());

    size_t i = 0;

    while (i < s.size()) {
        size_t pos = s.find(from, i);

        if (pos == std::string::npos) {
            out.append(s, i, std::string::npos);
            break;
        }

        out.append(s, i, pos - i);

        bool left_ok = (pos == 0) || !is_word_char(s[pos - 1]);
        bool right_ok = (pos + from.size() >= s.size()) || !is_word_char(s[pos + from.size()]);

        if (left_ok && right_ok) {
            out += to;
        } else {
            out.append(from);
        }

        i = pos + from.size();
    }

    return out;
}


extern std::atomic<bool> g_nohighp;


static std::string fix_precisions(const std::string &in) {
    using namespace std;
    const string mediump_precision = "mediump";
    const string highp_precision = "highp";
    const string precision_touse = g_nohighp.load() ? mediump_precision : highp_precision;
    const string input_string = replace_all_word(in, g_nohighp.load() ? highp_precision
                                                                      : mediump_precision,
                                                 precision_touse);

    string out;
    out.reserve(input_string.size() + 64);

    bool has_precision = false;
    size_t pos = 0;

    while (pos < input_string.size()) {
        size_t line_end = input_string.find('\n', pos);
        if (line_end == std::string::npos) line_end = input_string.size();

        string line = input_string.substr(pos, line_end - pos);


        size_t i = 0;
        while (i < line.size() && isspace((unsigned char) line[i])) i++;
        string trimmed = line.substr(i);

        if (starts_with(trimmed, "precision")) {
            if (trimmed.find(precision_touse) != string::npos) {
                if (!has_precision) {
                    out += "precision " + precision_touse + " float;\n";
                    out += "precision " + precision_touse + " int;\n";
                    has_precision = true;
                }
            }
            pos = line_end + 1;
            continue;
        }

        out += line;
        out += '\n';
        pos = line_end + 1;
    }

    return out;
}

extern "C" char *sanitize_glsl(const char *inputString) {
    std::string in = fix_precisions(inputString);
    std::string out;
    out.reserve(in.size());

    bool prev_space = false;
    bool prev_newline = true;

    for (size_t i = 0; i < in.size(); i++) {
        char c = in[i];

        if (c == '\r')
            continue;

        if (c == '\n') {
            if (!prev_newline) {
                out += '\n';
                prev_newline = true;
            }
            prev_space = false;
            continue;
        }

        if (std::isspace((unsigned char) c)) {
            if (!prev_space && !prev_newline) {
                out += ' ';
                prev_space = true;
            }
            continue;
        }

        out += c;
        prev_space = false;
        prev_newline = false;
    }

    while (!out.empty() && (out.back() == '\n' || out.back() == ' '))
        out.pop_back();

    return strdup(out.c_str());
}