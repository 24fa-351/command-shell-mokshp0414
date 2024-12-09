#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS 100

typedef struct
{
    char *key;
    char *value;
} EnvVar;

EnvVar env_vars[MAX_ARGS];
int env_count = 0;

void handle_cd(char *path);
void handle_pwd();
void handle_set(char *key, char *value);
void handle_unset(char *key);
char *expand_variables(char *input);
void execute_command(char *command);
void execute_pipes(char *command);

int main()
{
    char input[MAX_INPUT];

    while (1)
    {
        printf("xsh# ");
        fflush(stdout);

        if (!fgets(input, MAX_INPUT, stdin))
            break;
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0)
            break;

        char *expanded_input = expand_variables(input);
        execute_pipes(expanded_input);
        free(expanded_input);
    }

    return 0;
}

void handle_cd(char *path)
{
    if (path == NULL || chdir(path) != 0)
    {
        perror("xsh: cd");
    }
}

void handle_pwd()
{
    char cwd[MAX_INPUT];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        printf("%s\n", cwd);
    }
    else
    {
        perror("xsh: pwd");
    }
}

void handle_set(char *key, char *value)
{
    for (int i = 0; i < env_count; i++)
    {
        if (strcmp(env_vars[i].key, key) == 0)
        {
            free(env_vars[i].value);
            env_vars[i].value = strdup(value);
            return;
        }
    }
    env_vars[env_count].key = strdup(key);
    env_vars[env_count].value = strdup(value);
    env_count++;
}

void handle_unset(char *key)
{
    for (int i = 0; i < env_count; i++)
    {
        if (strcmp(env_vars[i].key, key) == 0)
        {
            free(env_vars[i].key);
            free(env_vars[i].value);
            for (int j = i; j < env_count - 1; j++)
            {
                env_vars[j] = env_vars[j + 1];
            }
            env_count--;
            return;
        }
    }
}

char *expand_variables(char *input)
{
    char *expanded = strdup(input);
    for (int i = 0; i < env_count; i++)
    {
        char placeholder[MAX_INPUT];
        snprintf(placeholder, sizeof(placeholder), "$%s", env_vars[i].key);
        char *pos = strstr(expanded, placeholder);
        if (pos)
        {
            size_t len = strlen(expanded) + strlen(env_vars[i].value) - strlen(placeholder) + 1;
            char *new_expanded = malloc(len);
            snprintf(new_expanded, len, "%.*s%s%s",
                     (int)(pos - expanded), expanded, env_vars[i].value, pos + strlen(placeholder));
            free(expanded);
            expanded = new_expanded;
        }
    }
    return expanded;
}

void execute_command(char *command)
{
    char *args[MAX_ARGS];
    int arg_count = 0;
    char *token = strtok(command, " ");
    while (token != NULL)
    {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    if (arg_count == 0)
        return;

    if (strcmp(args[0], "cd") == 0)
    {
        handle_cd(args[1]);
    }
    else if (strcmp(args[0], "pwd") == 0)
    {
        handle_pwd();
    }
    else if (strcmp(args[0], "set") == 0)
    {
        if (arg_count >= 3)
        {
            handle_set(args[1], args[2]);
        }
        else
        {
            fprintf(stderr, "xsh: set requires a key and a value\n");
        }
    }
    else if (strcmp(args[0], "unset") == 0)
    {
        if (arg_count >= 2)
        {
            handle_unset(args[1]);
        }
        else
        {
            fprintf(stderr, "xsh: unset requires a key\n");
        }
    }
    else
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            execvp(args[0], args);
            perror("xsh");
            exit(EXIT_FAILURE);
        }
        else
        {
            wait(NULL);
        }
    }
}

void execute_pipes(char *command)
{
    char *commands[MAX_ARGS];
    int command_count = 0;
    char *token = strtok(command, "|");
    while (token != NULL)
    {
        commands[command_count++] = token;
        token = strtok(NULL, "|");
    }
    commands[command_count] = NULL;

    int pipefds[2 * (command_count - 1)];
    for (int i = 0; i < command_count - 1; i++)
    {
        if (pipe(pipefds + i * 2) < 0)
        {
            perror("xsh: pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < command_count; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (i > 0)
            {
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < command_count - 1)
            {
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }
            for (int j = 0; j < 2 * (command_count - 1); j++)
            {
                close(pipefds[j]);
            }
            execute_command(commands[i]);
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < 2 * (command_count - 1); i++)
    {
        close(pipefds[i]);
    }
    for (int i = 0; i < command_count; i++)
    {
        wait(NULL);
    }
}
