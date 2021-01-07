#include "starbytes/Core/Core.h"
#include "starbytes/Interpreter/BCReader.h"
#include "starbytes/Parser/Lexer.h"
#include "starbytes/Parser/Parser.h"
#include "starbytes/Semantics/Main.h"
#include "starbytes/Gen/Gen.h"

STARBYTES_STD_NAMESPACE

using namespace Starbytes;

std::string highlightCode(std::string & code,std::vector<Token> & tokens){
	int index = 0;
	size_t s = sizeof("\x1b[30m");
	for(auto & tok : tokens){
		DocumentPosition & pos = tok.getPosition();
		std::string color_code;
		if(tok.getType() == Starbytes::TokenType::Keyword){
			color_code = PURPLE;
		} else if(tok.getType() == Starbytes::TokenType::Identifier){
			color_code = YELLOW;
		} else if(tok.getType() == Starbytes::TokenType::Operator){
			color_code = PURPLE;
		} else {
			color_code = "0m";
		}
		
		if(pos.raw_index == 0){
			std::cout << "StrLength:" << code.length();
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

void logError(std::string message,std::string code, std::vector<Token> &tokens){
	std::string result = highlightCode(code,tokens);
	std::cerr << message << "\n\n" << result << "\n";
}

void Driver::evalDirEntry(const std::filesystem::directory_entry & entry){
	if(entry.is_directory()){
		std::filesystem::directory_iterator it(entry);
		for(const auto & e : it){
			evalDirEntry(e);
		};
	}
	else if(entry.is_regular_file()){
		srcs.push_back(entry.path().string());
	};
};

void Driver::doWork(){
	//Get all srcs!
	std::filesystem::directory_iterator it(opts.directory);
	for(const auto & e : it){
		evalDirEntry(e);
	};
	if(!srcs.empty()){
		std::vector<AbstractSyntaxTree *> trees;
		auto begin_it = srcs.begin();
		auto & first_src = *begin_it;
		std::vector<Token> tok_stream;
		AbstractSyntaxTree *tree;
		Lexer lex(first_src,tok_stream);
		Parser p(tok_stream,tree);
		Semantics::SemanticASettings settings (false,opts.module_search);
		Semantics::SemanticA sem(settings);
		sem.initialize();
		sem.analyzeFileForModule(tree);
		trees.push_back(tree);
		++begin_it;
		while(begin_it != srcs.end()){
			tok_stream.clear();
			lex.resetWithNewCode(*begin_it,tok_stream);
			lex.tokenize();
			AbstractSyntaxTree *new_tree;
			p.clearAndResetWithNewTokens(tok_stream,new_tree);
			p.convertToAST();
			sem.analyzeFileForModule(new_tree);
			trees.push_back(new_tree);
			++begin_it;
		};

		if(!sem.finish()){
			exit(1);
		};
		std::ofstream out (opts.out);
		CodeGen::CodeGenROpts cg_opts (opts.module_search);
		CodeGen::generateToBCProgram(trees,out,cg_opts);

	};
};

NAMESPACE_END