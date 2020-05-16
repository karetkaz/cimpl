grammar Cmpl;

// starting point for parsing a file
unit: statementList EOF;

statementList: statement*;
statement
    : ';'                                                                                               # EmptyStatement
    | 'inline' Literal ';'                                                                            # IncludeStatement
    | qualifiers? '{' statementList '}'                                                              # CompoundStatement
    | qualifiers? 'if' '(' declExpr ')' statement ('else' statement)?                                      # IfStatement
    | qualifiers? 'for' '(' declExpr? ';' expression? ';' expression? ')' statement                       # ForStatement
    | 'for' '(' variable ':' expression ')' statement                                                    # EachStatement
    | 'return' initializer? ';'                                                                        # ReturnStatement
    | 'break' ';'                                                                                       # BreakStatement
    | 'continue' ';'                                                                                 # ContinueStatement
    | expression ';'                                                                               # ExpressionStatement
    | declaration                                                                                 # DeclarationStatement
    ;

declarationList: declaration*;
declaration
    : qualifiers? 'enum' identifier? (':' typename)? '{' propertyList '}'                              # EnumDeclaration
    | qualifiers? 'struct' identifier? (':' (Literal | typename))? '{' declarationList '}'             # TypeDeclaration
    | qualifiers? 'inline' identifier ('(' parameterList? ')')? '=' initializer ';'                   # PropertyOperator
    | qualifiers? 'inline' '('')' ('(' parameterList? ')')? '=' initializer ';'                        # InvokerOperator
    | qualifiers? 'inline' '['']' ('(' parameterList? ')')? '=' initializer ';'                        # IndexerOperator
    | qualifiers? 'inline' unary ('(' parameterList? ')')? '=' initializer ';'                           # UnaryOperator
    | qualifiers? 'inline' arithmetic ('(' parameterList? ')')? '=' initializer ';'                 # ArithmeticOperator
    | qualifiers? 'inline' bitwise ('(' parameterList? ')')? '=' initializer ';'                       # BitwiseOperator
    | qualifiers? 'inline' relational ('(' parameterList? ')')? '=' initializer ';'                 # RelationalOperator
    | qualifiers? 'inline' equality ('(' parameterList? ')')? '=' initializer ';'                     # EqualityOperator
    | qualifiers? (variable | function) ( '=' initializer)? ';'                                    # VariableDeclaration
    | qualifiers? functionImpl                                                                     # FunctionDeclaration
    ;

argumentList: '&'?expression (',' '&'?expression)*;
expressionList: expression (',' expression)*;
expression
    : Literal                                                                                        # LiteralExpression
    | Identifier                                                                                  # IdentifierExpression
    | '(' argumentList? ')'                                                                    # ParenthesizedExpression
    | expression '(' argumentList? ')'                                                          # FunctionCallExpression
    | '[' expressionList? ']'                                                                  # ParenthesizedExpression
    | expression '[' expressionList? ']'                                                         # ArrayAccessExpression
    | expression '[' expression '..' expression ']'                                               # ArraySliceExpression
    | expression '.' Identifier                                                                 # MemberAccessExpression
    | unary expression                                                                                 # UnaryExpression
    | expression arithmetic expression                                                            # ArithmeticExpression
    | expression bitwise expression                                                                  # BitwiseExpression
    | expression relational expression                                                            # RelationalExpression
    | expression equality expression                                                                # EqualityExpression
    | expression ('='| '*=' | '/=' | '%=' | '+=' | '-=' ) expression                              # AssignmentExpression
    | expression ('&=' | '|=' | '^=' | '<<=' | '>>=') expression                                  # AssignmentExpression
    | expression ('&&' | '||') expression                                                            # LogicalExpression
    | expression '?' expression ':' expression                                                   # ConditionalExpression
    ;

initializer
    : typename? '{' propertyList '}'                                                                 # ObjectInitializer
    | typename? '{' expressionList '}'                                                                # ArrayInitializer
    | expression                                                                                      # ValueInitializer
    ;

typename: Identifier ('.' Identifier)*;
variable: typename ('&' | '&&')? identifier ('[' ('*' | expressionList)? ']')*;
function: typename identifier '(' parameterList? ')';
functionImpl: function '{' statementList '}';
property: (Literal | Identifier) ':' initializer;
propertyList: (functionImpl | (property ';'))* | (property (',' property)*)?;
parameter: qualifiers? (variable | function);
parameterList: parameter (',' parameter)* '...'?;

declExpr: expression | (variable '=' initializer);

qualifiers: ('const' | 'static' | 'parallel')+;
unary: ('+' | '-' | '~' | '!');
arithmetic: ('*' | '/' | '%' | '+' | '-');
bitwise: ('&' | '|' | '^' | '<<' | '>>');
relational: ('<' | '<=' | '>' | '>=');
equality: ('==' | '!=');

identifier: Identifier;
Identifier: Letter (Letter | Number)*;

Literal
    : '0'[bB][0-1]+                                                                                            // binary
    | '0'[oO]?[0-7]+                                                                                            // octal
    | '0'[xX][0-9a-fA-F]+                                                                                 // hexadecimal
    | Decimal Suffix?                                                                                         // decimal
    | Decimal Exponent Suffix?                                                                 // floating point literal
    | '.' Number+ Exponent? Suffix?                                                            // floating point literal
    | Decimal '.' Number* Exponent? Suffix?                                                    // floating point literal
    | '\'' .*? '\''                                                                                 // character literal
    | '"' .*? '"'                                                                                      // string literal
    ;

fragment Number: [0-9];
fragment Letter: [_A-Za-z];
fragment Suffix: [dDuUfF];
fragment Decimal: [0] | ([1-9] Number*);
fragment Exponent: [eE] [+-]? Number+;

Whitespace: [ \t\n\r]+ -> skip;
LineComment: '//' ~[\r\n]* -> skip;
BlockComment: '/*' .*? '*/' -> skip;
NestedComment: '/+' .*? '+/' -> skip;	// todo: nesting
