-- 1. Crear tabla Estudiantes (Sintaxis: TIPO NOMBRE)
create table estudiantes (
    int id,
    varchar(50) nombre_completo,
    int edad,
    boolean becado
);

-- 2. Insertar datos (Sin espacios en los strings)
insert into estudiantes values (100, 'Juan_Perez', 20, true);
insert into estudiantes values (101, 'Maria_Gomez', 19, false);
insert into estudiantes values (102, 'Carlos_Ruiz', 21, true);
insert into estudiantes values (103, 'Ana_Solis', 20, false);

-- 3. Crear tabla Cursos
create table cursos (
    int codigo,
    varchar(50) nombre_curso,
    decimal(4,2) creditos
);

insert into cursos values (10, 'Compiladores', 4.50);
insert into cursos values (20, 'Sistemas_Operativos', 3.00);
insert into cursos values (30, 'Base_de_Datos', 4.00);

-- 4. Consultas
select * from estudiantes;
select * from cursos;