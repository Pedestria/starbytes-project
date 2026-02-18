#include "starbytes/base/CodeView.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

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

}

CodeView::CodeView(std::string sourceName,std::string sourceText){
    setSource(std::move(sourceName),std::move(sourceText));
}

void CodeView::setSource(std::string sourceNameIn,std::string sourceText){
    sourceName = std::move(sourceNameIn);
    lines.clear();
    std::istringstream stream(sourceText);
    std::string line;
    while(std::getline(stream,line)){
        lines.push_back(line);
    }
    if(!sourceText.empty() && sourceText.back() == '\n'){
        lines.emplace_back("");
    }
}

bool CodeView::hasSource() const{
    return !lines.empty();
}

const std::string &CodeView::getSourceName() const{
    return sourceName;
}

std::string CodeView::renderRegion(const Region &region,const std::string &annotation,unsigned contextLines) const{
    if(!hasSource() || !regionHasLocation(region)){
        return {};
    }

    unsigned lineCount = lines.size();
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
    out << " --> " << (sourceName.empty() ? "<source>" : sourceName) << ":" << startLine << ":" << headerCol << "\n";
    out << " " << std::setw((int)numberWidth) << " " << " |\n";

    bool annotationPrinted = false;
    for(unsigned lineNo = renderStart; lineNo <= renderEnd; ++lineNo){
        const auto &line = lines[lineNo - 1];
        out << " " << std::setw((int)numberWidth) << lineNo << " | " << line << "\n";

        if(lineNo < startLine || lineNo > endLine){
            continue;
        }

        unsigned startCol = lineNo == startLine ? region.startCol : 0;
        unsigned endCol = lineNo == endLine ? region.endCol : (unsigned)line.size();
        startCol = std::min<unsigned>(startCol,(unsigned)line.size());
        endCol = std::min<unsigned>(endCol,(unsigned)line.size());
        if(endCol <= startCol){
            endCol = std::min<unsigned>((unsigned)line.size(),startCol + 1);
            if(endCol <= startCol){
                startCol = 0;
                endCol = std::min<unsigned>(1,(unsigned)line.size() + 1);
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
