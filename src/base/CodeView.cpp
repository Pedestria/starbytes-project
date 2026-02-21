#include "starbytes/base/CodeView.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string_view>
#include <vector>

namespace starbytes {

namespace {

static bool regionHasLocation(const Region &region){
    return region.startLine > 0 || region.endLine > 0 || region.startCol > 0 || region.endCol > 0;
}

static unsigned clampLine(unsigned line,unsigned min,unsigned max){
    if(line < min){
        return min;
    }
    if(line > max){
        return max;
    }
    return line;
}

static uint64_t hashSourceKey(const std::string &sourceName,const std::string &sourceText){
    uint64_t hash = 1469598103934665603ULL;
    auto hashChars = [&](const std::string &value){
        for(unsigned char c : value){
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ULL;
        }
    };
    hashChars(sourceName);
    hash ^= 0xFFu;
    hash *= 1099511628211ULL;
    hashChars(sourceText);
    return hash;
}

struct IndexedSourceCacheEntry {
    uint64_t hash = 0;
    std::weak_ptr<const CodeViewSourceIndex> source;
};

std::shared_ptr<const CodeViewSourceIndex> acquireIndexedSource(std::string sourceName,
                                                                std::string sourceText){
    static std::mutex cacheMutex;
    static std::vector<IndexedSourceCacheEntry> cacheEntries;
    static constexpr size_t CACHE_LIMIT = 64;

    auto hash = hashSourceKey(sourceName,sourceText);
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        for(auto &entry : cacheEntries){
            if(entry.hash != hash){
                continue;
            }
            auto shared = entry.source.lock();
            if(!shared){
                continue;
            }
            if(shared->sourceName == sourceName && shared->sourceText == sourceText){
                return shared;
            }
        }
    }

    auto indexed = std::make_shared<CodeViewSourceIndex>();
    indexed->sourceName = std::move(sourceName);
    indexed->sourceText = std::move(sourceText);

    if(!indexed->sourceText.empty()){
        indexed->lineStarts.push_back(0);
        for(size_t i = 0; i < indexed->sourceText.size(); ++i){
            if(indexed->sourceText[i] == '\n'){
                indexed->lineStarts.push_back(i + 1);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if(cacheEntries.size() >= CACHE_LIMIT){
            cacheEntries.erase(cacheEntries.begin());
        }
        cacheEntries.push_back({hash,indexed});
    }
    return indexed;
}

static std::string_view lineView(const CodeViewSourceIndex &index,unsigned lineNo){
    if(lineNo == 0 || lineNo > index.lineStarts.size()){
        return {};
    }
    auto begin = index.lineStarts[lineNo - 1];
    auto end = (lineNo < index.lineStarts.size()) ? index.lineStarts[lineNo] : index.sourceText.size();
    if(end > begin && index.sourceText[end - 1] == '\n'){
        --end;
    }
    if(end > begin && index.sourceText[end - 1] == '\r'){
        --end;
    }
    return std::string_view(index.sourceText.data() + begin,end - begin);
}

}

CodeView::CodeView(std::string sourceName,std::string sourceText){
    setSource(std::move(sourceName),std::move(sourceText));
}

void CodeView::setSource(std::string sourceNameIn,std::string sourceText){
    indexedSource = acquireIndexedSource(std::move(sourceNameIn),std::move(sourceText));
}

bool CodeView::hasSource() const{
    return indexedSource != nullptr && !indexedSource->lineStarts.empty();
}

const std::string &CodeView::getSourceName() const{
    static const std::string emptySourceName;
    if(!indexedSource){
        return emptySourceName;
    }
    return indexedSource->sourceName;
}

std::string CodeView::renderRegion(const Region &region,const std::string &annotation,unsigned contextLines) const{
    if(!hasSource() || !regionHasLocation(region)){
        return {};
    }

    const auto &index = *indexedSource;
    unsigned lineCount = static_cast<unsigned>(index.lineStarts.size());
    unsigned startLine = region.startLine == 0 ? 1 : region.startLine;
    unsigned endLine = region.endLine == 0 ? startLine : region.endLine;
    if(endLine < startLine){
        endLine = startLine;
    }
    startLine = clampLine(startLine,1,lineCount);
    endLine = clampLine(endLine,startLine,lineCount);

    unsigned renderStart = startLine > contextLines ? startLine - contextLines : 1;
    unsigned renderEnd = std::min(lineCount,endLine + contextLines);
    unsigned numberWidth = std::to_string(renderEnd).size();

    unsigned headerCol = region.startCol + 1;
    std::ostringstream out;
    out << " --> " << (index.sourceName.empty() ? "<source>" : index.sourceName) << ":" << startLine << ":" << headerCol << "\n";
    out << " " << std::setw((int)numberWidth) << " " << " |\n";

    bool annotationPrinted = false;
    for(unsigned lineNo = renderStart; lineNo <= renderEnd; ++lineNo){
        auto line = lineView(index,lineNo);
        out << " " << std::setw((int)numberWidth) << lineNo << " | " << line << "\n";

        if(lineNo < startLine || lineNo > endLine){
            continue;
        }

        unsigned startCol = lineNo == startLine ? region.startCol : 0;
        unsigned endCol = lineNo == endLine ? region.endCol : static_cast<unsigned>(line.size());
        startCol = std::min<unsigned>(startCol,static_cast<unsigned>(line.size()));
        endCol = std::min<unsigned>(endCol,static_cast<unsigned>(line.size()));
        if(endCol <= startCol){
            endCol = std::min<unsigned>(static_cast<unsigned>(line.size()),startCol + 1);
            if(endCol <= startCol){
                startCol = 0;
                endCol = std::min<unsigned>(1,static_cast<unsigned>(line.size()) + 1);
            }
        }
        unsigned markerLen = std::max<unsigned>(1,endCol - startCol);
        out << " " << std::setw((int)numberWidth) << " " << " | " << std::string(startCol,' ') << std::string(markerLen,'^');
        if(!annotationPrinted && !annotation.empty()){
            out << " " << annotation;
            annotationPrinted = true;
        }
        out << "\n";
    }
    return out.str();
}

}
