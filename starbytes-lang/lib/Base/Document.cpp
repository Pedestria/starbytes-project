#include "starbytes/Base/Document.h"

STARBYTES_STD_NAMESPACE
using namespace std;

string document_position_to_str(DocumentPosition & pos){
	return string("{Line:" +to_string(pos.line)+",Start:"+to_string(pos.start)+",End:"+to_string(pos.end)+",RawIndex:"+to_string(pos.raw_index)+"}");
}

// void src_location_to_str(std::string & result,SrcLocation & loc_ref){
//     result += "{\nFilename:"+loc_ref.id+"\nPosition:{\n\tStartChar:"+std::to_string(loc_ref.pos.start)+"\n\tEndChar:"+std::to_string(loc_ref.pos.end)+"\n\tLineNum:"+std::to_string(loc_ref.pos.line)+"\n\t}\n}";
// };

NAMESPACE_END