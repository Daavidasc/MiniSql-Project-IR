-- TEST 2: Bucle FOR

CREATE TABLE auditoria (
    codigo_evento int,
    mensaje varchar(50),
    prioridad decimal(5,2)
);

-- Ejecuta el INSERT 5 veces
FOR i = 1 TO 5 DO
    INSERT INTO auditoria VALUES (100, 'Proceso_Automatico', 1.00);
END FOR;

-- Deberías ver 5 filas idénticas
SELECT * FROM auditoria;