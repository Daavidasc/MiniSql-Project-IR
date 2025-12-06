-- TEST: Drop Table

-- 1. Crear tabla temporal
CREATE TABLE temp_table (
    id int,
    data varchar(20)
);

INSERT INTO temp_table VALUES (1, 'To be deleted');

-- Verificar que existe
SELECT * FROM temp_table;

-- 2. Eliminar tabla
DROP TABLE temp_table;

-- 3. Intentar seleccionar (Debería dar error o tabla vacía si la volviste a crear)
-- Nota: Si intentas hacer SELECT de una tabla que no existe en el esquema, tu driver actual imprimirá un error.
SELECT * FROM temp_table;