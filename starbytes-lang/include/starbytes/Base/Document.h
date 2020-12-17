#include "Macros.h"
#include <string>

#ifndef BASE_DOCUMENT_H
#define BASE_DOCUMENT_H

STARBYTES_STD_NAMESPACE
    using SrcDocumentID = std::string;
    /*A Position on A Starbytes Document. Used to determine Positions of Tokens/ASTNodes/etc.
    */
    struct DocumentPosition {
        unsigned int line;
        unsigned int start;
        unsigned int end;
        unsigned int raw_index;
    };

    struct SrcLocation {
        SrcDocumentID id;
        DocumentPosition pos;
    };

    SrcLocation && makeSrcLoc(SrcDocumentID & id,DocumentPosition & pos);

    std::string document_position_to_str(DocumentPosition & pos);

    // void src_location_to_str(std::string & result,SrcLocation & loc_ref);
NAMESPACE_END

#endif