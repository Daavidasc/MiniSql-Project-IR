CREATE TABLE hardware (
    id_producto int,          -- Antes tenías: int id_producto
    modelo varchar(50),       -- Antes: varchar(50) modelo
    precio decimal(10,2),     -- Antes: decimal(10,2) precio
    en_stock boolean          -- Antes: boolean en_stock
);

INSERT INTO hardware VALUES (1, 'Monitor_4K', 299.99, true);
INSERT INTO hardware VALUES (2, 'Teclado_Mec', 89.50, true);
INSERT INTO hardware VALUES (3, 'Mouse_Gamer', 45.00, false);

CREATE TABLE ventas (
    id_venta int
);

INSERT INTO ventas VALUES (1001);

SELECT * FROM hardware;
SELECT * FROM ventas;