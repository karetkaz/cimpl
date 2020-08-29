# Statements
Statements are the basic blocks of a program.

**[Syntax](Cmpl.g4)**
```antlrv4
statement
    : ';'                                                                                               # EmptyStatement
    | 'inline' Literal ';'                                                                            # IncludeStatement
    | qualifiers? '{' statementList '}'                                                              # CompoundStatement
    | qualifiers? 'if' '(' for_init ')' statement ('else' statement)?                                      # IfStatement
    | qualifiers? 'for' '(' for_init? ';' expression? ';' expression? ')' statement                       # ForStatement
    | 'for' '(' variable ':' expression ')' statement                                                 # ForEachStatement
    | 'return' initializer? ';'                                                                        # ReturnStatement
    | 'break' ';'                                                                                       # BreakStatement
    | 'continue' ';'                                                                                 # ContinueStatement
    | expression ';'                                                                               # ExpressionStatement
    | declaration                                                                                 # DeclarationStatement
    ;
```

