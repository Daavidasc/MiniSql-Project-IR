-- 1. Crear una tabla de ejemplo para el log
CREATE TABLE log_operaciones (
    int id,
    varchar(50) descripcion,
    decimal(10,2) monto,
    boolean estado
);

-- 2. Ejecutar el bucle FOR (simularemos 3 registros)
FOR i = 1 TO 3 DO
INSERT INTO log_operaciones VALUES (1, 'Log_Automatico', 10.50, true);
END FOR;

-- 3. Verificar el contenido de la tabla
SELECT * FROM log_operaciones;