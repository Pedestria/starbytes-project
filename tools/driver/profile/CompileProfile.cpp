#include "CompileProfile.h"

#include <iomanip>
#include <ostream>

namespace {

double nsToMs(uint64_t ns) {
    return static_cast<double>(ns) / 1000000.0;
}

}

namespace starbytes::driver::profile {

void printCompileProfile(std::ostream &out,const CompileProfileData &profile,bool success) {
    if(!profile.enabled) {
        return;
    }

    out << std::fixed << std::setprecision(3);
    out << "{\n";
    out << "  \"type\": \"compile_profile\",\n";
    out << "  \"command\": \"" << profile.command << "\",\n";
    out << "  \"success\": " << (success ? "true" : "false") << ",\n";
    out << "  \"input\": \"" << profile.input << "\",\n";
    out << "  \"module\": \"" << profile.moduleName << "\",\n";
    out << "  \"counts\": {\n";
    out << "    \"modules\": " << profile.moduleCount << ",\n";
    out << "    \"sources\": " << profile.sourceCount << ",\n";
    out << "    \"parser_files\": " << profile.parserFileCount << ",\n";
    out << "    \"parser_tokens\": " << profile.parserTokenCount << ",\n";
    out << "    \"parser_statements\": " << profile.parserStatementCount << ",\n";
    out << "    \"parser_source_bytes\": " << profile.parserSourceBytes << ",\n";
    out << "    \"module_cache_hits\": " << profile.moduleCacheHits << ",\n";
    out << "    \"module_cache_misses\": " << profile.moduleCacheMisses << "\n";
    out << "  },\n";
    out << "  \"timings_ms\": {\n";
    out << "    \"total\": " << nsToMs(profile.totalNs) << ",\n";
    out << "    \"module_graph\": " << nsToMs(profile.moduleGraphNs) << ",\n";
    out << "    \"module_file_load\": " << nsToMs(profile.moduleFileLoadNs) << ",\n";
    out << "    \"import_scan\": " << nsToMs(profile.importScanNs) << ",\n";
    out << "    \"import_resolve\": " << nsToMs(profile.importResolveNs) << ",\n";
    out << "    \"parse_total\": " << nsToMs(profile.parseTotalNs) << ",\n";
    out << "    \"lex\": " << nsToMs(profile.lexNs) << ",\n";
    out << "    \"syntax\": " << nsToMs(profile.syntaxNs) << ",\n";
    out << "    \"semantic\": " << nsToMs(profile.semanticNs) << ",\n";
    out << "    \"consumer\": " << nsToMs(profile.consumerNs) << ",\n";
    out << "    \"module_build\": " << nsToMs(profile.moduleBuildNs) << ",\n";
    out << "    \"module_link\": " << nsToMs(profile.moduleLinkNs) << ",\n";
    out << "    \"gen_finish\": " << nsToMs(profile.genFinishNs) << ",\n";
    out << "    \"runtime_exec\": " << nsToMs(profile.runtimeExecNs) << "\n";
    out << "  }\n";
    out << "}\n";
    out.unsetf(std::ios::floatfield);
}

}
