#include "starbytes/Core/Core.h"
#include "starbytes/Interpreter/BCReader.h"
#include "starbytes/Parser/Lexer.h"
#include "starbytes/Parser/Parser.h"
#include "starbytes/Semantics/Main.h"
#include "starbytes/Gen/Gen.h"

#include <cstdint>

// #ifdef _WIN32
// #undef max
// #undef min 
// #endif

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/FormatVariadic.h>

STARBYTES_STD_NAMESPACE

using namespace Starbytes;

namespace LLVMFS = llvm::sys::fs;
namespace LLVMPATH = llvm::sys::path;

std::string highlightCode(std::string code,std::vector<Token> tokens){
	int index = 0;
	size_t s = sizeof("\x1b[30m");
	for(auto & tok : tokens){
		const DocumentPosition & pos = tok.getPosition();
		std::string color_code;
		if(tok.getType() == Starbytes::TokenType::Keyword){
			color_code = ANSI_PURPLE;
		} else if(tok.getType() == Starbytes::TokenType::Identifier){
			color_code = ANSI_YELLOW;
		} else if(tok.getType() == Starbytes::TokenType::Operator){
			color_code = ANSI_PURPLE;
		} else {
			color_code = "0m";
		}
		
		if(pos.raw_index == 0){
			std::cout << "StrLength:" << code.size();
			code = "\x1b["+color_code + code;
			std::cout << "start:" << pos.raw_index << " end:" << pos.raw_index+tok.getTokenSize()+(3*index)+3 << " ";
			++index;
			code.insert(pos.raw_index+tok.getTokenSize()+(3*index)+3,"\x1b[0m");
		}
		else if(tok.getType() != Starbytes::TokenType::EndOfFile){
			std::cout << "start:" << pos.raw_index+(3 * index)+3 << " end:" << pos.raw_index+tok.getTokenSize()+(3*index)+3 << " ";
			code.insert(pos.raw_index+(3* index)+3,"\x1b["+color_code);
			++index;
			if(pos.raw_index == code.size()-1){
				code.append("\x1b[0m");
			}
			else{
				code.insert(pos.raw_index+tok.getTokenSize()+(3*index)+3,"\x1b[0m");
			}
		}
		else if(pos.raw_index == code.size()-1){
			code.append("\x1b[0m");
		}
		++index;
	}
	std::cout << "New Size:" << code.size();
	return code;
}

void logError(std::string message,llvm::StringRef code,ArrayRef<Token> toks){
    std::string result = highlightCode(code.str(),toks.vec());
	std::cerr << message << "\n\n" << result << "\n";
}

void Driver::doWork(){
	std::string error_msg;
	std::vector<uint64_t> file_sizes;
	//Get all srcs!

	if(LLVMFS::is_directory(opts.directory)){;
		std::error_code err;
		LLVMFS::recursive_directory_iterator it(opts.directory,err);
		while(true){
			if(err){
				error_msg = llvm::formatv("Error Code: {0}",err.message());
				break;
			};

			if(LLVMFS::exists(it->path())){
				auto entry = *it;
				if(entry.type() == LLVMFS::file_type::regular_file){
					file_sizes.push_back(entry.status()->getSize());
					llvm::StringRef ext = LLVMPATH::extension(entry.path());
					if(ext == ".stb"){
						srcs.push_back(entry.path());
					};
				};
			}
			else {
				break;
			};
			it.increment(err);
		};
		if(!error_msg.empty()){
			std::cerr << error_msg << std::endl;
		};
	}
	else {
		// MUST BE A DIRECTORY!!!!!!
		return;
	};

	uint64_t sum = 0;

	for(auto & n : file_sizes){
		sum += n;
	};
	
	auto avg_file_size = sum / file_sizes.size();
	/// If number of sources is odd, then the median is the element at index:(n+1)/2, 
	/// on the other hand if the number is even then the median is the 
	/// mean of the two elements at: (n/2) and (n+1/2)
	auto median_file_size = (srcs.size() % 2) != 0? file_sizes[(srcs.size()+1)/2] : ((file_sizes[(srcs.size())/2] + file_sizes[(srcs.size() + 1)/2])/2);
	file_sizes.clear();



	llvm::ThreadPool t_pool (llvm::hardware_concurrency(2));

	std::vector<AbstractSyntaxTree *> trees;
	Semantics::SemanticASettings settings (false,opts.module_search,opts.modules_to_link);
	Semantics::SemanticA sem (settings);
	sem.initialize();

	t_pool.async([&](unsigned file_take_count){
		std::queue<std::string> files_;
		unsigned idx = 0;
		while(idx != file_take_count){
			files_.push(srcs.back());
			srcs.pop_back();
			++idx;
		};
		auto first = files_.front();
		files_.pop();
		std::vector<Token> tok_stream;
		Lexer lex(first,tok_stream);
		AbstractSyntaxTree *tree_res;
		Parser parser(tok_stream,tree_res);
		sem.analyzeFileForModule(tree_res);
		trees.push_back(tree_res);
	},3);

	if(srcs.empty()){
		t_pool.wait();
		if(sem.finish()){
			sem.freeSymbolStores();
		}
	};

};

NAMESPACE_END
