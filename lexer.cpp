#include <map>
#include <memory>
#include <string>
#include <vector>

// The lexer returns [0-255] if it is an unknown character, otherwise
// one of these for known tokens.

enum Token {
	tok_eof = -1,

	// commands
	tok_def = -2,
	tok_extern = -3,

	// primary
	tok_identifier = -4,
	tok_number = -5,
};

// IdentifierStr holds the namke of the identifier, if tok_identifier is set
static std::string IdentifierStr;
// NumVal holds the numerical value of tok_number, if set
static double NumVal;

static int gettok() {
	static int LastChar = ' ';

	// skip whitespace
	while (isspace(LastChar))
		LastChar = getchar();


	// check for identifiers and other reserved words
	if (isalpha(LastChar)) {
		IdentifierStr = LastChar;
		while (isalnum((LastChar = getchar())))
			IdentifierStr += LastChar;

		if (IdentifierStr == "def")
			return tok_def;
		if (IdentifierStr == "extern")
			return tok_extern;
		return tok_identifier;
	}

	// check for number type
	if (isdigit(LastChar) || LastChar == '.') {
		std::string NumStr;
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while (isdigit(LastChar) || LastChar == '.');

		NumVal = strtod(NumStr.c_str(), 0);
		return tok_number;
	}

	// ignore comment lines starting with #
	if (LastChar == '#') {
		do {
			LastChar = getchar();
		} while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		if (LastChar != EOF)
			return gettok();
	}

	// Don't consume EOF
	if (LastChar == EOF)
		return tok_eof;

	int ThisChar = LastChar;
	LastChar = getchar();
	return ThisChar;
}

// Abstract Syntax Tree
// One object for each construct in the language

// Expressions AST
class ExprAST {
public: 
	virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST {
	double Val;

public:
	NumberExprAST(double val) : Val(val) {}
};


class VariableExprAST : public ExprAST {
	std::string Name;

public:
	VariableExprAST(const std::string &name) : Name(name) {}
};

// Expression class for binary operator
class BinaryExprAST : public ExprAST {
	char Op; // '+' '*' etc. 
	std::unique_ptr<ExprAST> LHS, RHS;

public:
	BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
			   std::unique_ptr<ExprAST> RHS)
	: Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

// Expression class for function calls
class CallExprAST : public ExprAST {
	std::string Callee;
	std::vector<std::unique_ptr<ExprAST>> Args;

public:
	CallExprAST(const std::string &Callee,
			 std::vector<std::unique_ptr<ExprAST>> Args)
	: Callee(Callee), Args(std::move(Args)) {}
};

// PrototypeAST - represents a protoype for a function which captures its name and 
// its argument names 
class PrototypeAST {
	std::string Name;
	std::vector<std::string> Args;

public:
	PrototypeAST(const std::string &Name, std::vector<std::string> Args)
	: Name(Name), Args(std::move(Args)) {}

	const std::string &getName() const {return Name; }
};

// FunctionAST - Represents a function definition
// TODO type field
class FunctionAST {
	std::unique_ptr<PrototypeAST> Proto;
	std::unique_ptr<ExprAST> Body;

public:
	FunctionAST(std::unique_ptr<PrototypeAST> Proto, 
			 std::unique_ptr<ExprAST> Body)
	: Proto(std::move(Proto)), Body(std::move(Body)) {}
};

auto LHS = std::make_unique<VariableExprAST>("x");
auto RHS = std::make_unique<VariableExprAST>("y");
auto Result = std::make_unique<BinaryExprAST>('+', std::move(LHS), std::move(RHS));

// simple token buffer
static int CurTok;
static int getNextToken() {
	return CurTok = gettok();
}

// LogError - Helper functions for error handling
std::unique_ptr<ExprAST> LogError(const char* Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorProto(const char* Str) {
	LogError(Str);
	return nullptr;
}

// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
	auto Result = std::make_unique<NumberExprAST>(NumVal);
	getNextToken(); // consume number
	return std::move(Result);
}

// parse parethesis ie '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
	getNextToken();
	auto V = ParseExpression();
	if (!V)
		return nullptr;

	if (CurTok != ')')
		return LogError("expected ')'");

	getNextToken();
	return V;
}

// called if the current token is tok_identifier
// returns either variable name or function name and arguments
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;
	getNextToken();

	// if there are no parethesis, it must be a variable 
	if (CurTok != '(')
		return std::make_unique<VariableExprAST>(IdName);

	getNextToken();
	std::vector<std::unique_ptr<ExprAST>> Args;
	// add any parameters to Args vector
	if (CurTok != ')' ) { 
		while (true) {
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
				else
				return nullptr;

			if (CurTok == ')')
			break;

			if (CurTok != ',')
				return LogError("Expected ')' or ',' in argument list");
			getNextToken();
		}
	}

	// consume ')'
	getNextToken();

	return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

static std::unique_ptr<ExprAST> ParsePrimary() {
	switch (CurTok) {
		default:
			return LogError("Unknown token: expected an expression");
		case tok_identifier:
			return ParseIdentifierExpr();
		case tok_number:
			return ParseNumberExpr();
		case '(':
			return ParseParenExpr();
	}
}

static std::map<char, int> BinopPrecedence;

// get precedence of pending binary operator
static int GetTokPrecedence() {
	if (!isascii(CurTok))
		return -1;

	int TokPrec = BinopPrecedence[CurTok];
	if (TokPrec <= 0)
		return -1;
	return TokPrec;
}

static std::unique_ptr<ExprAST> ParseExpression() {
	auto LHS = ParsePrimary();
	if (!LHS)
		return nullptr;

	return ParseBinOpRHS(0, std::move(LHS));
}

// parse sequence of pairs
// takes precedence and pointer to expression for the part that has already been parsed
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
	while (true) {
		int TokPrec = GetTokPrecedence();

		// 
		if (TokPrec < ExprPrec)
			return LHS;

		// we know this is a binary operation
		int BinOp = CurTok;
		getNextToken();

		// parse primary expression after binary operator
		auto RHS = ParsePrimary();
		if (!RHS)
			return nullptr;

		// if BinOp binds less tightly with RHS than the operator after RHS,
		// let the pending operator take RHS as its LHS
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			// parse right hand side in full
			RHS = ParseBinOpRHS(TokPrec+1, std::move(RHS));
			if (!RHS)
				return nullptr;
		}
		// merge LHS and RHS
		LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
	} //loop around to top
}

// prototype
static std::unique_ptr<PrototypeAST> ParsePrototype() {
	if (CurTok != tok_identifier)
		return LogErrorProto("Expected function name in prototype");

	std::string FnName = IdentifierStr;
	getNextToken();

	if (CurTok != '(')
		return LogErrorProto("Expected '(' in prototype");

	// read list of argument names
	std::vector<std::string> ArgNames;
	while (getNextToken() == tok_identifier)
		ArgNames.push_back(IdentifierStr);
	if (CurTok != ')')
		return LogErrorProto("Expected ')' in prototype");

	// success
	getNextToken(); // eat ')'

	// return PrototypeAST of filename and argument name list
	return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

// definiton 'def' prototype expression
static std::unique_ptr<FunctionAST> ParseDefinition() {
	getNextToken(); // eat "def"
	auto Proto = ParsePrototype();
	if (!Proto) 
		return nullptr;

	if (auto E = ParseExpression())
		return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));

	return nullptr;
}

// external 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
	getNextToken();
	return ParsePrototype();
}

// toplevelexpr expression
static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
	if (auto E = ParseExpression()) {
		// make anonyomus prototype
		auto Proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
		return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
	}
	return nullptr;
}

// top-level parsing
static void HandleDefinition() {
	if (ParseDefinition()) {
		fprintf(stderr, "Parsed a function definition.\n");
	} else {
		// skip token for error recovery
		getNextToken();
	}
}
static void HandleExtern() {
	if (ParseExtern()) {
		fprintf(stderr, "Parsed an extern.\n");
	} else {
		// skip token for error recovery
		getNextToken();
	}
}
static void HandleTopLevelExpression() {
	if (ParseTopLevelExpr()) {
		fprintf(stderr, "Parsed a top-level expr.\n");
	} else {
		// skip token for error recovery
		getNextToken();
	}
}

// top = def | extern | expression | ';'
static void MainLoop() {
	while (true) {
		fprintf(stderr, "> ");
		switch (CurTok) {
			case tok_eof:
			return;
			case ';': //ignore top-level semicolons
				getNextToken();
			break;
			case tok_def:
				HandleDefinition();
			break;
			case tok_extern:
				HandleExtern();
			break;
			default:
				HandleTopLevelExpression();
			break;
		}
	}
}

int main() {
	// 1 is lowest precedence
	BinopPrecedence['<'] = 10;
	BinopPrecedence['+'] = 20;
	BinopPrecedence['-'] = 30;
	BinopPrecedence['*'] = 40;
	
	fprintf(stderr, "> ");
	getNextToken();

	MainLoop();
	
	return 0;
}
