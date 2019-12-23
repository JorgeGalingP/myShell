#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "parser.h"

#define SIZE 1024

static int run = 1;

/* funcion manejadora del signal */
void exit_handler()
{
    run = 0;
}

/* funcion que comprueba si el argumento line contiene el comando seleccionado */
int handle_command(tline *line, int n_commands, char command[])
{
    if (line->ncommands == n_commands && strcmp(line->commands[0].argv[0], command) == 0)
    {
        return 0;
    }

    return 1;
}

/* funcion ejecutar el comando cd */
void execute_cd_command(tline *line)
{
    // variables
    char dir[SIZE];

    // si solo tenemos un comando
    if (line->ncommands == 1)
    {
        // comprobamos si el comando es "cd" o "cd" con argumentos
        if (line->commands[0].argc == 1)
        {
            strcpy(dir, getenv("HOME")); // apuntamos al directorio HOME solo si es 1 argumento
            if (dir == NULL)
            {
                fprintf(stderr, "No existe la variable $HOME"); // no existe
            }
        }
        else
        {
            strcpy(dir, line->commands[0].argv[1]); // apuntamos al directorio del parametro si solo son 2 argumentos
        }

        // cambiamos de directorio y lo imprimimos
        if (chdir(dir) != 0)
        {
            fprintf(stderr, "Error al cambiar de directorio: %s\n", strerror(errno));
        }

        // imprimimos el directorio actual
        printf("El directorio actual es: %s\n", getcwd(dir, SIZE));
    }
}

/* funcion para manejar las redirecciones */ 
int handle_redirect(char *file, char mode)
{
    int d = -1;
    mode_t userMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    // char 'r' para indicar la lectura (read)
    if (mode == 'r')
    {
        d = open(file, O_RDONLY); // abrimos para lectura sólo
    }

    // char 'w' para indicar la escritura (write)
    if (mode == 'w')
    {
        d = open(file, O_WRONLY | O_CREAT | O_TRUNC, userMode); // abrimos para escritura sólo
    }

    // si el descriptor no fue modificado o no se puedo abrir, entonces existe un error
    if (d == -1)
    {
        fprintf(stderr, "No ha sido posible abrir el archivo %s \n. Error: %s\n", file, strerror(errno));
        close(d);
        exit(-1);
    }

    return d;
}

/* funcion para redirigir a entrada estandar */
void redirect_to_stdin(tline *line)
{
    int d;

    if (line->redirect_input != NULL)
    {
        d = handle_redirect(line->redirect_input, 'r');
        dup2(d, 0);
    }
}

/* funcion para redirigir a salida estandar */
void redirect_to_stdout(tline *line)
{
    int d;

    if (line->redirect_output != NULL)
    {
        d = handle_redirect(line->redirect_output, 'w');
        dup2(d, 1);
    }
}

/* funcion para redirigir a la salida de error */
void redirect_to_stderr(tline *line)
{
    int d;

    if (line->redirect_error != NULL)
    {
        d = handle_redirect(line->redirect_error, 'w');
        dup2(d, 2);
    }
}

/* funcion principal para ejecutar 1 o n comandos */
void execute_command(tline *line)
{
    // variables
    pid_t pid;
    int status;

    // control del numero de comandos introducidos
    if (line->ncommands == 1)
    {
        // hacemos el fork()
        pid = fork();

        // error
        if (pid < 0)
        {
            fprintf(stderr, "Error en el fork() \n %s\n", strerror(errno));
            exit(-1);
        }
        
        // proceso hijo
        if (pid == 0)
        {
            // redirecciones en el primer comando por que sólo hay un comando
            redirect_to_stdin(line);
            redirect_to_stdout(line);
            redirect_to_stderr(line);

            if (line->commands[0].filename != NULL)
            {
                // el hijo ejecuta el comando con sus opciones
                execvp(line->commands[0].argv[0], line->commands[0].argv);
                printf("Se ha producido un error en la ejecucion del comando.\n");
                exit(1); // error si consigue llegar hasta aqui
            }
            else
            {
                fprintf(stderr, "El comando %s no se encuentra.", line->commands[0].argv[0]);
                exit(1);
            }
        }
    }
    else if (line->ncommands > 1)
    {
        // variables
        int i;
        int size_array = line -> ncommands - 1;
        int size_commands = line -> ncommands;
        int **p = (int **) malloc (size_array * sizeof(int *));

        // una vez reservado el espacio del array de arrays de integer, 
        // en cada posicion reservamos un array de 2 posiciones
        for (i = 0; i < size_array; i++)
        {
            p[i] = (int *)malloc(2 * sizeof(int));
        }

        // por cada comando, duplicamos el proceso y creamos un pipe
        for (i = 0; i < size_commands; i++)
        {
            // creamos un pipe excepto en el último comando
            if (i != size_array)
            {
                pipe(p[i]);
            }

            // hacemos el fork()
            pid = fork();

            // error
            if (pid < 0)
            {
                fprintf(stderr, "Error en el fork() \n %s\n", strerror(errno));
                exit(-1);
            }
            
            // proceso hijo
            if (pid == 0)
            {
                // redirección primer comando
                if (i == 0)
                {
                    // cerramos la parte de lectura no utilizada
                    close(p[i][0]);

                    // el primer hijo controla la redirección del input
                    redirect_to_stdin(line);
                    
                    // y escribimos
                    dup2(p[i][1], 1);
                }
                else if (i == size_array)
                {
                    // cerramos la parte de escritura no utilizada
                    close(p[i-1][1]);

                    // el ultimo hijo controla la redirección del output y error
                    redirect_to_stdout(line);
                    redirect_to_stderr(line);

                    // y leeemos
                    dup2(p[i-1][0], 0);
                }
                else
                {
                    // cerramos de manera correcta
                    close(p[i-1][1]);
                    close(p[i][0]);

                    // y sino es ni el primero ni el ultimo,
                    // conectamos las tuberias de la manera correcta
                    dup2(p[i-1][0], 0);
                    dup2(p[i][1], 1);
                }

                // cerrar los pipes previos
                for (int j = 0; j < i; j++)
                {
                    close(p[j][0]);
                    close(p[j][1]);
                }

                // una vez hecho las redirecciones necesarias, ejecutamos
                if (line->commands[i].filename != NULL)
                {
                    // ejecutamos el comando con sus opciones
                    pid = execvp(line->commands[i].argv[0], line->commands[i].argv);
                    printf("Se ha producido un error en la ejecucion del comando %s.\n", line->commands[i].argv[0]);
                    
                    exit(1); // error si consigue llegar aqui
                }
                else
                {
                    printf("El comando introducido %s no se ha encontrado.\n", line->commands[i].argv[0]);
                    
                    exit(1);
                }
            }
        }

        // cerramos pipes y liberamos memoria
        for (i = 0; i < size_array; i++)
        {
            // si no se cierran los descriptores de los pipes anteriores
            // el comando se cree que tiene todavía cosas por leer
            close(p[i][0]);
            close(p[i][1]);

            free(p[i]);
        }
        free(p);
    }

    // si no ejecuta en background, esperamos a la finalización normal del proceso
    if (!line->background)
    {
        waitpid(pid, &status, 0);

        // para saber si el hijo hizo un exit() o no
        if (WIFEXITED(status) != 0)
        {
            // si el exit() que hizo el hijo funciono o no
            if (WEXITSTATUS(status) != 0)
            {
                printf("¡El comando no se ha ejecutado!\n");
            }
        }
    }
    else
    {
        printf(" [%d] \n", pid); // si la linea tiene background, imprime el pid del proceso sin esperar
    }
}

/* funcion principal */
int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        // variables
        char buf[SIZE];
        tline *line;

        // deshabilitamos las señales
        signal(SIGINT, exit_handler);
        signal(SIGQUIT, exit_handler);

        // pintamos el prompt por primera vez
        printf("msh> ");

        // variable run para controlar el prompt después de cada instrucción
        while (run)
        {
            // mientras que la línea no sea nula
            if (fgets(buf, SIZE, stdin) != NULL)
            {
                // tokenizamos la linea
                line = tokenize(buf);

                // y la línea tokenizada tampoco sea nula
                if (line != NULL)
                {
                    // ejecutamos comandos dependiendo del caso
                    if (handle_command(line, 1, "exit") == 0 || handle_command(line, 1, "EXIT") == 0)
                    {
                        exit_handler();
                        return 0;
                    }
                    else if (handle_command(line, 1, "cd") == 0)
                    {
                        execute_cd_command(line);
                    }
                    else
                    {
                        execute_command(line);
                    }
                }
            }

            // pintamos el prompt a la vuelta
            printf("msh> ");
        }
    }
    else
    {
        fprintf(stderr, "Error en el uso del programa, el uso correcto es sin comandos: %s \n", argv[0]);
        return 1;
    }

    return 0;
}
