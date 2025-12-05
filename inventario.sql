-- 1. Tabla de Hardware
create table hardware (
    int serial,
    varchar(100) modelo,
    decimal(10,2) precio,
    boolean disponible
);

-- 2. Inserts masivos
insert into hardware values (5001, 'Monitor_LG_24', 150.99, true);
insert into hardware values (5002, 'Teclado_Mecanico', 89.50, true);
insert into hardware values (5003, 'Mouse_Logitech', 25.00, false);
insert into hardware values (5004, 'Cable_HDMI_2m', 5.99, true);
insert into hardware values (5005, 'Webcam_HD', 45.00, true);

-- 3. Tabla de Ventas (Sin FK, solo ids logicos)
create table ventas (
    int id_venta,
    int id_producto,
    int cantidad,
    decimal(10,2) total
);

insert into ventas values (1, 5001, 2, 301.98);
insert into ventas values (2, 5004, 10, 59.90);

-- 4. Probar proyección (solo algunas columnas)
select modelo, precio from hardware;
select * from ventas;