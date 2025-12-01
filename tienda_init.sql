-- TABLA 1: CATEGORIAS
create table categorias (
    int id_cat,
    varchar(50) nombre_categoria,
    boolean activa
);

insert into categorias values (1, 'Procesadores_CPU', true);
insert into categorias values (2, 'Tarjetas_Graficas', true);
insert into categorias values (3, 'Almacenamiento_SSD', true);
insert into categorias values (4, 'Fuentes_Poder', false);

-- TABLA 2: CLIENTES
create table clientes (
    int id_cliente,
    varchar(50) apellido,
    varchar(50) region,
    boolean es_vip
);

insert into clientes values (101, 'Gomez_Perez', 'Lima', true);
insert into clientes values (102, 'Silva_Chavez', 'Arequipa', false);
insert into clientes values (103, 'Quispe_Mamani', 'Cusco', false);
insert into clientes values (104, 'Rodriguez_Sanz', 'Lima', true);
insert into clientes values (105, 'Fernandez_Ruiz', 'Piura', false);

-- TABLA 3: PRODUCTOS (Inventario)
create table productos (
    int id_prod,
    int id_cat,
    varchar(100) modelo,
    decimal(10,2) precio,
    boolean en_stock
);

-- CPUs
insert into productos values (1001, 1, 'Ryzen_5_7600X', 249.99, true);
insert into productos values (1002, 1, 'Core_i9_13900K', 589.50, true);
insert into productos values (1003, 1, 'Ryzen_7_5800X3D', 320.00, false);

-- GPUs
insert into productos values (2001, 2, 'RTX_4090_Gaming_OC', 1599.99, true);
insert into productos values (2002, 2, 'RX_7900_XTX', 999.00, true);
insert into productos values (2003, 2, 'GTX_1650_LowProfile', 149.50, true);

-- SSDs
insert into productos values (3001, 3, 'Samsung_980_Pro_1TB', 89.99, true);
insert into productos values (3002, 3, 'Kingston_NV2_2TB', 110.25, true);