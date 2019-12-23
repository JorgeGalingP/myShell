#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "parser.h"

#define SIZE 1024

static int run = 1;

void comunicatePipeFile()
{
    int pid;
    int status;
    int p[2];

    char buf[SIZE];

    FILE *fd;

    if (pipe(p))
    {
        fprintf(stderr, "El pipe ha fallado.\n");
        exit(1);
    }

    pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "Error en el fork(); \n %s\n", strerror(errno));
        exit(1);
    }
    else if (pid == 0) /* child process */
    {
        close(p[1]);
        fd = fdopen(p[0], "r");
        if (fgets(buf, SIZE, fd) != NULL)
        {
            fclose(fd);
        }

        printf("El hijo ha recibido: %s\n", buf);
    }
    else /* parent process */
    {
        close(p[0]);
        fd = fdopen(p[1], "w");

        if (fgets(buf, SIZE, stdin) != NULL)
        {
            fputs(buf, fd);
            fflush(fd);
            fclose(fd);
        }

        wait(&status);

        printf("El padre y el hijo han acabado.\n");
    }

    exit(0);
}

void executeOneCommand(char *command[])
{
    // variables
    int pid;
    int status;

    // hacemos el fork()
    pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "Error en el fork() \n %s\n", strerror(errno));
        exit(-1);
    }
    else if (pid == 0) /* proceso hijo */
    {
        // ejecutamos el comando con sus opciones
        execvp(command[2], command + 2);
        printf("Se ha producido un error en la ejecucion del comando.\n");
        exit(1); // error si consigue llegar aqui
    }
    else /* proceso padre */
    {
        wait(&status); // capturamos estado del hijo

        // para saber si el hijo hizo un exit() o no
        if (WIFEXITED(status) != 0)
        {
            // si el exit() que hizo el hijo funciono o no
            if (WEXITSTATUS(status) != 0)
            {
                printf("¡El comando no se ha ejecutado!\n");
            }
        }

        exit(0);
    }
}

void executeTwoCommands(char *first[], char *second[])
{
    // variables
    int pid;
    int status;
    int p[2];

    // hacemos el pipe()
    if (pipe(p))
    {
        fprintf(stderr, "El pipe ha fallado.\n");
        exit(1);
    }

    // hacemos el fork()
    pid = fork();

    if (pid < 0)
    {
        fprintf(stderr, "Error en el fork() \n %s\n", strerror(errno));
        exit(-1);
    }
    else if (pid == 0) /* proceso hijo */
    {
        // cerramos parte de escritura no utilizada
        close(p[1]);

        // el hijo maneja el segundo comando
        // y remplaza stdin con el input del pipe
        dup2(p[0], 0);
        close(p[0]);

        // ejecutamos el segundo comando
        execvp(second[0], second);
        exit(1); // error si consigue llegar aqui
    }
    else /* proceso padre */
    {
        // cerramos parte de lectura no utilizada
        close(p[0]);

        // el padre maneja el primer comando
        // y remplaza stdout con el output del pipe
        dup2(p[1], 1);
        close(p[1]);

        // ejecutamos el primer comando
        execvp(first[0], first);
        wait(&status); // capturamos estado del hijo

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
}

void redirectCommand1(char *command[], char input[], char output[], mode_t mode)
{
    // variables
    int in;
    int out;

    // open input and output files
    in = open(input, O_RDONLY); // abrimos para lectura sólo
    out = open(output, O_WRONLY | O_CREAT | O_TRUNC, mode);

    // reemplazamos stdin con input file
    dup2(in, 0);

    // reemplazamos stdout con output file
    dup2(out, 1);

    // cerramos descriptores
    close(in);
    close(out);

    // ejecutamos el comando
    execvp(command[0], command + 1);
}

/* ***************************************************************** */

/*                  GOOD PROGRAM!!!!! NICE :D                        */

/* ***************************************************************** */

void exitHandler()
{
    run = 0;
}

int handleCommand(tline *line, int nCommands, char command[])
{
    if (line->ncommands == nCommands
            && strcmp(line->commands[0].argv[0], command) == 0)
    {
        return 0;
    }

    return 1;
}

void debugTLine(tline *line)
{
    int i;
    int j;

    if (line != NULL)
    {
        if (line->redirect_input != NULL)
        {
            printf("redirección de entrada: %s\n", line->redirect_input);
        }
        if (line->redirect_output != NULL)
        {
            printf("redirección de salida: %s\n", line->redirect_output);
        }
        if (line->redirect_error != NULL)
        {
            printf("redirección de error: %s\n", line->redirect_error);
        }
        if (line->background)
        {
            printf("comando a ejecutarse en background\n");
        }
        printf("orden con %d comandos\n", line->ncommands);
        for (i = 0; i < line->ncommands; i++)
        {
            printf("orden %d (%s):\n", i, line->commands[i].filename);
            for (j = 0; j < line->commands[i].argc; j++)
            {
                printf("  argumento %d: %s\n", j, line->commands[i].argv[j]);
            }
        }
    }
}

void executeCdCommand(tline *line)
{
    // variables
    char dir[SIZE];

    if (line->ncommands == 1)
    {
        if (line->commands[0].argc == 1)
        {
            strcpy(dir, getenv("HOME")); // apuntamos al directorio HOME si solo es 1 argumento
            if (dir == NULL)
            {
                fprintf(stderr, "No existe la variable $HOME");
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

        printf("El directorio actual es: %s\n", getcwd(dir, SIZE));
    }
}

int handleRedirect(char* file, char mode)
{
    int d = -1;
    mode_t userMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    // mode_t mode2 = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    if (mode == 'r')
    {
        d = open(file, O_RDONLY); // abrimos para lectura sólo
    }

    if (mode == 'w')
    {
        d = open(file, O_WRONLY | O_CREAT | O_TRUNC, userMode); // abrimos para escritura sólo
    }

    if (d == -1)
    {
        fprintf(stderr, "No ha sido posible abrir el archivo %s \n. Error: %s\n", file, strerror(errno));
        close(d);
        exit(-1);
    }

    return d;
}

void redirectCommandInput(tline *line)
{
    int d;

    if (line->redirect_input != NULL)
    {
        d = handleRedirect(line->redirect_input, 'r');
        dup2(d, 0);
    }
}

void redirectCommandOutput(tline *line)
{
    int d;

    if (line->redirect_output != NULL)
    {
        d = handleRedirect(line->redirect_output, 'w');
        dup2(d, 1);
    }
}

void redirectCommandError(tline *line)
{
    int d;

    if (line->redirect_error != NULL)
    {
        d = handleRedirect(line->redirect_error, 'w');
        dup2(d, 2);
    }
}

void executeCommand(tline *line)
{
    printf(" ******* ******* *******\n");
    debugTLine(line);
    printf(" ******* ******* *******\n");

    if (line->ncommands == 1)
    {
        // variables
        int pid;
        int status;

        // hacemos el fork()
        pid = fork();

        if (pid < 0)
        {
            fprintf(stderr, "Error en el fork() \n %s\n", strerror(errno));
            exit(-1);
        }
        else if (pid == 0) /* proceso hijo */
        {
            // redirecciones en el primer comando por que sólo hay un comando
            redirectCommandInput(line);
            redirectCommandOutput(line);
            redirectCommandError(line);

            if (line->commands[0].filename != NULL)
            {
                // ejecutamos el comando con sus opciones
                execvp(line->commands[0].argv[0], line->commands[0].argv);
                printf("Se ha producido un error en la ejecucion del comando.\n");
                exit(1); // error si consigue llegar aqui
            }
            else
            {
                fprintf(stderr, "El comando %s no se encuentra.", line->commands[0].argv[0]);
                exit(1);
            }
        }
        else /* proceso padre */
        {
            wait(&status); // capturamos estado del hijo

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
    }
    else if (line->ncommands > 1)
    {
        // Inicializamos array con ncommands-1
        int i;
        int size_array_pipes = line->ncommands - 1;
        int **p = (int**) malloc (size_array_pipes*sizeof(int*));
        for (i = 0; i < line->ncommands-1; i++)
        {
            p[i] = (int*) malloc (2 * sizeof(int)); // en cada posicion reservamos un array de 2 posiciones
        }

        pid_t pid;

        for(i = 0; i < line->ncommands; i++) {

            // Creamos un pipe excepto en el último comando
            if(i != line->ncommands - 1) {
                pipe(p[i]);
            }

            pid = fork();  // Creamos un proceso hijo

            if (pid < 0) {
                fprintf(stderr, "Error en el fork() \n %s\n", strerror(errno));
                exit(-1);
            }

            if (pid == 0) {

                // Redirección primer comando
                if ( i == 0 ) {
                    close(p[i][0]);

                    // el primer hijo controla la redirección del input
                    // y leemos el input
                    redirectCommandInput(line);
                    dup2(p[i][1], 1);
                }

                // Redirección último comando
                else if (i == line->ncommands - 1) {
                    // cerramos parte de escritura no utilizada
                    close(p[i-1][1]);

                    // el ultimo hijo controla la redirección del output y error
                    redirectCommandOutput(line);
                    redirectCommandError(line);
                    dup2(p[i-1][0], 0);
                }

                // Redirección comando intermedio
                else {
                    // y sino es ni el primero ni el ultimo,
                    // conectamos las tuberias de la manera correcta
					close(p[i-1][1]);
                    close(p[i][0]);

                    dup2(p[i-1][0], 0);
					dup2(p[i][1], 1);
                }

                // Cerrar pipes previos
                int j;
                for(j=0; j<i; j++){
                    close(p[j][0]);
                    close(p[j][1]);
                }

                pid=execvp(line->commands[i].argv[0], line->commands[i].argv);
                printf("Se ha producido un error en la ejecucion del comando %s.\n", line->commands[i].argv[0]);
                exit(1); // error si consigue llegar aqui
            }
        }

        // Cerramos pipes
        // Si no se cierran los descriptores de los pipes anteriores
        // el comando se cree que tiene todavía cosas por leer
        // y se queda pillado
        for(i=0;i<size_array_pipes;i++){
            close(p[i][0]);
            close(p[i][1]);
            free(p[i]);
        }
        free(p);

        // Si no ejecuta en background, esperamos a la finalización del proceso
        if(!line->background){
            waitpid(pid,NULL,0);
        } else {
            printf(" %d \n", pid);       // Si es en background imprime el pid del proceso y no lo espera
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        char buf[SIZE];
        tline *line;
        signal(SIGINT, exitHandler);
        signal(SIGQUIT, exitHandler);

        printf("msh> ");

        // variable run para saber cuando el prompt tiene que aparecer automaticamente
        // después de la ejecución de cada instrucción
        while (run)
        {
            // mientras que la línea no sea nula
            if (fgets(buf, SIZE, stdin) != NULL)
            {
                line = tokenize(buf);

                // y la línea tokenizada tampoco sea nula
                if (line != NULL)
                {
                    // ejecutamos comandos
                    if (handleCommand(line, 1, "exit") == 0
                            || handleCommand(line, 1, "EXIT") == 0)
                    {
                        exitHandler();
                        return 0;
                    }
                    else if (handleCommand(line, 1, "cd") == 0)
                    {
                        executeCdCommand(line);
                    }
                    else
                    {
                        executeCommand(line);
                    }
                }
            }

            // pintamos el prompt del cmd a la vuelta
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
