-- TEST 3: Función IF

CREATE TABLE estudiantes (
    id int,
    nombre varchar(20),
    nota int
);

INSERT INTO estudiantes VALUES (101, 'Carlos_Perez', 18);
INSERT INTO estudiantes VALUES (102, 'Maria_Gomez', 10);
INSERT INTO estudiantes VALUES (103, 'Luis_Sanz', 15);

-- Prueba de la columna calculada con IF
SELECT 
    nombre, 
    nota, 
    IF(nota > 11, 'APROBADO', 'DESAPROBADO') AS estado 
FROM 
    estudiantes;