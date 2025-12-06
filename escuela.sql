-- 1. Crear tabla Estudiantes
create table estudiantes (
    id int,
    nombre_completo varchar(50),
    edad int,
    becado boolean
);

-- 2. Insertar datos (Sin espacios en los strings)
insert into estudiantes values (100, 'Juan_Perez', 20, true);
insert into estudiantes values (101, 'Maria_Gomez', 19, false);
insert into estudiantes values (102, 'Carlos_Ruiz', 21, true);
insert into estudiantes values (103, 'Ana_Solis', 20, false);

-- 3. Crear tabla Cursos
-- CORREGIDO: Sintaxis NOMBRE TIPO
create table cursos (
    codigo int,
    nombre_curso varchar(50),
    creditos decimal(4,2)
);

insert into cursos values (10, 'Compiladores', 4.50);
insert into cursos values (20, 'Sistemas_Operativos', 3.00);
insert into cursos values (30, 'Base_de_Datos', 4.00);

-- 4. Consultas
select * from estudiantes;
select * from cursos;