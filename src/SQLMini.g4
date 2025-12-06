grammar SQLMini;

// === PARSER RULES ===
program: query+;

query
    : create  
    | insert
    | select
    | drop
    | forLoop; 
    
drop: DROP TABLE ID PYC;

create: CREATE TABLE ID IPARE 
    columnDefinition (COM columnDefinition)* (COM tableConstraint)* DPARE PYC;

// FIX: Cambiado el orden a 'ID tipoDato' (Standard SQL) para soportar "id int"
columnDefinition: ID tipoDato; 

tableConstraint
    : (CONSTRAINT ID)? FOREIGN KEY IPARE ID DPARE REFERENCES ID IPARE ID DPARE 
    | PRIMARY KEY IPARE ID (COM ID)* DPARE; 

insert: INSERT INTO ID VALUES  
    IPARE dato (COM dato)* DPARE
    (COM IPARE dato (COM dato)* DPARE)*
    PYC;

select: SELECT (ESTRELLA |  
    expr (COM expr)* ) FROM ID PYC;

forLoop
    : FOR ID EQUAL VINT TO VINT DO
      insert 
      END FOR PYC;

expr
    : ID                      
    | functionCall;           

functionCall
    : IF_FUNC IPARE comparison COM dato COM dato DPARE (AS ID)?; 

comparison
    : ID (COMP_OP | EQUAL) dato; 

tipoDato
    : INT_TYPE
    | DECIMAL_TYPE IPARE VINT COM VINT DPARE
    | VARCHAR_TYPE IPARE VINT DPARE
    | BOOLEAN_TYPE;

dato
    : VINT
    | STRING
    | VINT PTO VINT
    | bool ; 
    
bool
    : TRUE_VAL
    | FALSE_VAL;

// === LEXER RULES (CASE INSENSITIVE) ===

CREATE:     [Cc][Rr][Ee][Aa][Tt][Ee];
TABLE:      [Tt][Aa][Bb][Ll][Ee];
DROP:       [Dd][Rr][Oo][Pp];
CONSTRAINT: [Cc][Oo][Nn][Ss][Tt][Rr][Aa][Ii][Nn][Tt];
PRIMARY:    [Pp][Rr][Ii][Mm][Aa][Rr][Yy];
KEY:        [Kk][Ee][Yy];
FOREIGN:    [Ff][Oo][Rr][Ee][Ii][Gg][Nn];
REFERENCES: [Rr][Ee][Ff][Ee][Rr][Ee][Nn][Cc][Ee][Ss];

INSERT:     [Ii][Nn][Ss][Ee][Rr][Tt];
INTO:       [Ii][Nn][Tt][Oo];
VALUES:     [Vv][Aa][Ll][Uu][Ee][Ss];
SELECT:     [Ss][Ee][Ll][Ee][Cc][Tt];
FROM:       [Ff][Rr][Oo][Mm];
WHERE:      [Ww][Hh][Ee][Rr][Ee]; 

INT_TYPE:     [Ii][Nn][Tt];
DECIMAL_TYPE: [Dd][Ee][Cc][Ii][Mm][Aa][Ll];
VARCHAR_TYPE: [Vv][Aa][Rr][Cc][Hh][Aa][Rr];
BOOLEAN_TYPE: [Bb][Oo][Oo][Ll][Ee][Aa][Nn];

TRUE_VAL:   [Tt][Rr][Uu][Ee];
FALSE_VAL:  [Ff][Aa][Ll][Ss][Ee];

FOR:        [Ff][Oo][Rr];
TO:         [Tt][Oo];
DO:         [Dd][Oo];
END:        [Ee][Nn][Dd];
IF_FUNC:    [Ii][Ff]; 
AS:         [Aa][Ss];

EQUAL:      '='; 
COMP_OP:    '>' | '<' | '>=' | '<=' | '!=';

PLUS:       '+';
MINUS:      '-';
ESTRELLA:   '*'; 
DIV:        '/';

DPARE:      ')';
IPARE:      '(';
PYC:        ';';
COM:        ',';
PTO:        '.';

ID:         [a-zA-Z_][a-zA-Z_0-9]*;
VINT:       [0-9]+;
STRING:     '\'' ( ~'\'' )* '\'';

LINE_COMMENT: '--' ~[\r\n]* -> skip; 
WS:           [ \t\r\n]+ -> skip;