#include "starbytes/Base/Document.h"

STARBYTES_STD_NAMESPACE
using namespace std;

SrcLocation && makeSrcLoc(SrcDocumentID & id,DocumentPosition & pos){
	SrcLocation src_loc;
	src_loc.id = id;
	src_loc.pos = pos;
	return std::move(src_loc);
};

string document_position_to_str(DocumentPosition & pos){
	return string("{Line:" +to_string(pos.line)+",Start:"+to_string(pos.start)+",End:"+to_string(pos.end)+",RawIndex:"+to_string(pos.raw_index)+"}");
}

void src_location_to_str(std::string & result,SrcLocation & loc_ref){
    result += "{\nFilename:"+loc_ref.id+"\nPosition:"+document_position_to_str(loc_ref.pos)+"\n}";
};

NAMESPACE_END