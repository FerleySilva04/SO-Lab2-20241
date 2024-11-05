// Importaciones
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

// Definición de constantes
#define SUCCESSFUL_END 0
#define FAILURE_END 1
#define MAX_PATH 4096

// Definición de variables globales
char *paths[100] = {"/bin/","/usr/bin/"}; // Rutas predeterminadas donde buscar los comandos
int countPaths = 2; // Contador de rutas
char *tokens[100]; // Array para almacenar los tokens (comandos y argumentos)
int indx = 0; // Índice para contar los tokens
int childs[100]; // Array para almacenar los PID de los procesos hijos
int countChilds = 0; // Contador de procesos hijos

// Declaración de funciones
void getTokens(FILE *); // Acepta un archivo como entrada y convierte las líneas en comandos separados almacenados en tokens
void executeCommand(); // Ejecuta el siguiente comando en tokens
int commandHasRedirection(char *arr[], int start); // Verifica si el comando tiene redirección '>'
void error(); // Imprime el mensaje de error en stderr
int isValidPath(const char *path); // Verifica si una ruta es válida

// Funciones

// Esta función lee las líneas de entrada y las convierte en comandos separados en tokens
void getTokens(FILE* input) {
	while(1) { // El shell debe estar esperando una entrada
		if(input == stdin) { // Modo interactivo
			printf("wish> "); // El prompt del shell
		}
		int temp = indx; // Guardar el valor actual del índice
		indx = 0; // Reiniciar el índice de tokens
		char *original; // Cadena original para procesar la entrada
		size_t len = 0;
		if(getline(&original, &len, input) == EOF) { // Leer hasta el final del archivo
			exit(SUCCESSFUL_END); // Salir si se llega al final del archivo
		}
		unsigned long long original_size = strlen(original); // Obtener el tamaño de la entrada original
		char *modified = (char *)malloc(sizeof(char) * original_size * 6); // Espacio para la versión modificada de la entrada
		int shift = 0; // Desplazamiento para manejar caracteres especiales
		if(!(strcmp(original, "&\n"))) { // Si la entrada es solo '&', ignorarla
			continue;
		}
		// Modificar la cadena para separar los signos especiales
		for(int i = 0; i < original_size; i++) {
			if(original[i] == '>' || original[i] == '&' || original[i] == '|') { // Verificar signos especiales
				modified[i + shift] = ' '; // Añadir espacio antes del signo
				shift++;
				modified[i + shift] = original[i]; // Copiar el signo
				i++;
				modified[i + shift] = ' '; // Añadir espacio después del signo
				shift++;
			}
			modified[i + shift] = original[i]; // Copiar el carácter original
		}
		// Separar los comandos usando el delimitador de espacio
		char *token;
		while( (token = strsep(&modified, " \n")) != NULL ) { // Eliminar espacios y saltos de línea
			if(!strcmp(token, "&")) { // Verificar si es el símbolo '&' para ejecutar múltiples comandos
				tokens[indx] = NULL;
				indx++;
			}
			if(memcmp(token, "\0", 1)) { // Verificar si el token no está vacío
				tokens[indx] = token;
				indx++;
			}
		}
		// Limpiar los tokens después del índice actual
		for(int i = indx; i < temp; i++) {
			tokens[i] = NULL;
		}
		// Ejecutar el comando
		executeCommand();
		free(modified); // Liberar la memoria
		// Esperar a los procesos hijos
		for(int x = 0; x < countChilds; x++) {
			int stat;
			waitpid(childs[x], &stat, 0);
		}
	}
}

// Función para ejecutar el comando actual en tokens
void executeCommand() {
	for(int i = 0; i < indx; i++) { // Iterar sobre los tokens
		if (!strcmp(tokens[i], "exit")) { // Comando especial "exit"
			if(tokens[i + 1] == NULL) { // Si no hay más tokens después de "exit"
				exit(SUCCESSFUL_END); // Salir con éxito
			} else {
				i++; // Avanzar al siguiente token
				error(); // Error si hay argumentos adicionales
			}
		}
		else if(!strcmp(tokens[i], "cd")) { // Comando especial "cd"
			if(chdir(tokens[++i])) { // Cambiar el directorio
				error(); // Error si no se puede cambiar
			}
			while (tokens[i] != NULL) { // Avanzar a la siguiente instrucción
				i++;
			}
			i++;
		}
		else if(!strcmp(tokens[i], "path")) { // Comando especial "path"
			countPaths = 0; // Reiniciar el contador de rutas
			for(int j = 1; j < indx; j++) {
				if (isValidPath(tokens[j])) { // Verificar si la ruta es válida
                    paths[j - 1] = tokens[j]; // Asignar la ruta válida
                }
                else {
                    error(); // Error si la ruta no es válida
                }
				countPaths++; // Incrementar el contador de rutas
			}
			i += countPaths; // Avanzar al siguiente comando
		}
		else {
			int pid = fork(); // Crear un nuevo proceso
			childs[countChilds++] = pid; // Almacenar el PID del hijo
			if(pid == -1) {
				error(); // Error si no se puede crear el proceso hijo
				exit(FAILURE_END);
			}
			if(pid == 0) { // Si es el proceso hijo
				if(commandHasRedirection(tokens, i) != -1) { // Verificar si hay redirección '>'
				for(int j = 0; j < countPaths; j++) {
					char *exe = malloc(sizeof(char) * MAX_PATH); 
					strcpy(exe, paths[j]); // Construir la ruta completa
					strcat(exe, "/");
					strcat(exe, tokens[i]); // Añadir el nombre del comando
					if(!access(exe, X_OK)) { // Verificar si el archivo es ejecutable
						execv(exe, tokens + i); // Ejecutar el comando
					}
				}
				error(); // Si no se puede ejecutar el comando, generar error
				exit(FAILURE_END);
				}
			}
			else { // Si es el proceso padre
				while (tokens[i] != NULL) { // Avanzar a la siguiente instrucción
					i++;
				}
				i++;
				if (i < indx) { // Si hay más comandos, continuar con el siguiente
					continue;
				}
			}
		}
	}
}

// Función para verificar si el comando tiene redirección '>'
int commandHasRedirection(char *arr[], int start) {
	for(int i = start; arr[i] != NULL; i++) {
		if(!strcmp(arr[i], ">") && i != start) { // Verificar si '>' no es el primer token
			arr[i] = NULL; // Terminar el comando antes de la redirección
			if(arr[i + 1] == NULL || arr[i + 2] != NULL) { // Verificar si hay un archivo válido después de '>'
				error(); // Error si no hay archivo después de '>'
				return -1;
			}
			else {
				int fd = open(arr[i + 1], O_WRONLY | O_CREAT, 0777); // Abrir o crear el archivo con permisos
				dup2(fd, STDOUT_FILENO); // Redirigir la salida estándar al archivo
				arr[i + 1] = NULL; // Terminar la cadena de argumentos
				close(fd); // Cerrar el archivo
			}
			return i; // Retornar el índice de la redirección
		}
	}
	return 0; // Si no hay redirección
}

// Función para imprimir el mensaje de error
void error() {
	char error_message[30] = "An error has occurred\n";
	write(STDERR_FILENO, error_message, strlen(error_message)); // Escribir el mensaje en stderr
}

// Función para verificar si una ruta es válida
int isValidPath(const char *path) {
  return access(path, X_OK) == 0; // Verificar si el archivo en la ruta es ejecutable
}

// Función principal
int main (int argc, char *argv[]) {
	if(argc == 1) { // Modo interactivo
		getTokens(stdin); // El archivo de entrada es stdin (comandos del usuario)
	}
	else if (argc == 2) { // Modo por lotes
		FILE *input = fopen(argv[1], "r"); // Abrir el archivo de entrada
		if(input != NULL) { // Verificar si el archivo se puede abrir
			getTokens(input); // Procesar el archivo
		}
		else {
			error(); // Error si no se puede abrir el archivo
			exit(FAILURE_END);
		}
	}
	else { // Error si hay un número incorrecto de argumentos
    	error();	
    	exit(FAILURE_END);
	}
	return 0;
} // Fin del main
