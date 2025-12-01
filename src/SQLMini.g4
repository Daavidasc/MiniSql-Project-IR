grammar SQLMini;

// === PARSER RULES ===
program: query+;

query
    : create  
    | insert
    | select
    | drop; // 1. AÑADIDO: Incluir la nueva sentencia DROP
    
drop: DROP TABLE ID PYC; // 2. AÑADIDO: Regla para eliminar tablas

create: CREATE TABLE ID IPARE 
    columnDefinition (COM columnDefinition)* (COM tableConstraint)* // Permitir definición de columnas y luego constraints
    DPARE PYC; 

// Regla para la definición simple de columnas (tipo y nombre)
columnDefinition: tipoDato ID; 

tableConstraint
    : (CONSTRAINT ID)? FOREIGN KEY IPARE ID DPARE REFERENCES ID IPARE ID DPARE // Opción 1: FK
    | PRIMARY KEY IPARE ID (COM ID)* DPARE; // Opción 2: PK (Soportando columnas múltiples, aunque simplificaremos la validación)

insert: INSERT INTO ID VALUES  
IPARE dato (COM dato)* DPARE
(COM IPARE dato (COM dato)* DPARE)*
PYC;

select: SELECT (ESTRELLA |  
ID (COM ID)* ) FROM ID PYC;

tipoDato
    : INT
    | DECIMAL IPARE VINT COM VINT DPARE
    | VARCHAR IPARE VINT DPARE
    | BOOLEAN;

dato
    : VINT
    | STRING
    | VINT PTO VINT
    | bool ;
    
bool
    : TRUE
    | FALSE;

// === LEXER RULES ===

CREATE : 'create';
TABLE : 'table';
INSERT : 'insert';
INTO : 'into';
SELECT : 'select';
VALUES : 'values';
ESTRELLA : '*';
FROM : 'from';

// 4. AÑADIDO: Nuevos Tokens del Lexer
DROP: 'drop';
FOREIGN: 'foreign';
PRIMARY: 'primary'; 
KEY: 'key';
REFERENCES: 'references';
CONSTRAINT: 'constraint';

INT : 'int';
VINT : [0-9]+;
DECIMAL : 'decimal';
VARCHAR : 'varchar';
BOOLEAN : 'boolean';
TRUE : 'true';
FALSE : 'false';

DPARE : ')';
IPARE : '(';
PYC : ';';
COM : ',';
PTO : '.';
ID : [a-zA-Z_][a-zA-Z_0-9]* ; 
STRING : '\'' ( ~'\'' )* '\'';

LINE_COMMENT: '--' ~[\r\n]* -> skip; 
WS : [ \t\r\n]+ -> skip;