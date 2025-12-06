-- TEST 1: Tipos de Datos
-- Ahora soporta sintaxis estándar: nombre_columna tipo_dato

CREATE TABLE inventario (
    id int,
    producto varchar(50),
    precio decimal(10,2),
    en_stock boolean
);

-- Inserta datos variados (mezclando mayúsculas/minúsculas en los comandos)
INSERT INTO inventario VALUES (1, 'Laptop_Gamer', 1500.50, true);
insert into inventario values (2, 'Mouse_RGB', 25.00, FALSE);
INSERT INTO inventario VALUES (3, 'Teclado_Mecanico', 80.99, True);

-- Verifica la lectura correcta
SELECT * FROM inventario;