grammar Cmpl;

// options {language=Java;}

unit: statementList EOF;

statement
    : ';'                                                                                               # EmptyStatement
    | qualifiers? '{' statementList '}'                                                              # CompoundStatement
    | qualifiers? 'if' '(' for_init ')' statement ('else' statement)?                                      # IfStatement
    | qualifiers? 'for' '(' for_init? ';' expression? ';' expression? ')' statement                       # ForStatement
    | qualifiers? 'for' '(' variable ':' expression ')' statement                                     # ForEachStatement
    | 'return' expression? ';'                                                                         # ReturnStatement
    | 'break' ';'                                                                                       # BreakStatement
    | 'continue' ';'                                                                                 # ContinueStatement
    | expression ';'                                                                               # ExpressionStatement
    | declaration                                                                                 # DeclarationStatement
    ;

expression
    : Literal                                                                                        # LiteralExpression
    | Identifier                                                                                  # IdentifierExpression
    | '(' expressionList? ')'                                                                  # ParenthesizedExpression
    | '[' expressionList? ']'                                                                  # ParenthesizedExpression
    | expression '(' expressionList? ')'                                                        # FunctionCallExpression
    | expression '.' Identifier                                                                 # MemberAccessExpression
    | expression '[' expressionList? ']'                                                         # ArrayAccessExpression
    | ('+' | '-' | '~' | '!' | '&') expression                                                         # UnaryExpression
    | expression ('*' | '/' | '%' | '+' | '-') expression                                         # ArithmeticExpression
    | expression ('&' | '|' | '^' | '<<' | '>>') expression                                          # BitwiseExpression
    | expression ('<' | '<=' | '>' | '>=') expression                                             # RelationalExpression
    | expression ('==' | '!=' | '===' | '!==') expression                                           # EqualityExpression
    | expression ('='| '*=' | '/=' | '%=' | '+=' | '-=' ) expression                              # AssignmentExpression
    | expression ('&=' | '|=' | '^=' | '<<=' | '>>=') expression                                  # AssignmentExpression
    | expression ('&&' | '||') expression                                                            # LogicalExpression
    | expression '?' expression ':' expression                                                   # ConditionalExpression
    // TODO: deprecated: ('struct' ',')?
    | 'emit' '(' ('struct' ',')? expressionList ')'                                                      # EmitIntrinsic
    ;

declaration
    : qualifiers? 'enum' identifier? (':' basetype)? '{' propertyList? '}'                             # EnumDeclaration
    | qualifiers? 'struct' identifier? (':' (Literal | basetype))? '{' declarationList '}'             # TypeDeclaration
    | qualifiers? 'inline' identifier ('(' parameterList? ')')? '=' initializer ';'                  # InlineDeclaration
    | qualifiers? variable ( '=' initializer)? ';'                                                 # VariableDeclaration
    | qualifiers? function ( '=' initializer)? ';'                                                 # VariableDeclaration
    | qualifiers? function '{' statementList '}'                                                   # FunctionDeclaration
    // TODO: deprecated
    | qualifiers? 'define' identifier ('(' parameterList? ')')? '=' initializer ';'                  # InlineDeclaration
    ;

initializer
    : expression                                                                                      # ValueInitializer
    | typename? '{' expressionList? '}'                                                               # ArrayInitializer
    | typename? '{' propertyList? '}'                                                                # ObjectInitializer
    ;

statementList: statement*;
expressionList: expression (',' expression)*;
declarationList: declaration*;
parameterList: parameter (',' parameter)* '...'?;
propertyList: (property ';')+ | property (',' property)*;

typename: Identifier ('.' Identifier)*;
basetype: Identifier ('.' Identifier)*;
property: (Literal | Identifier) ':' initializer;
function: typename identifier '(' parameterList? ')';
variable: typename ('&' | '&&')? identifier ('[' ('*' | expressionList)? ']')*;
parameter: qualifiers? (variable | function);

for_init: expression | (variable '=' initializer);

qualifiers: ('const' | 'static' | 'parallel')+;

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
