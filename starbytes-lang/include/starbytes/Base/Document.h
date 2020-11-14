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

    #define MAKE_SRC_LOCATION(id,pos) SrcLocation src_loc; loc.id = id; loc.pos = pos;

    void src_location_to_str(std::string & result,SrcLocation & loc_ref);
NAMESPACE_END

#endif