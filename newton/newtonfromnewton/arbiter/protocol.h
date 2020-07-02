#ifndef PROTOCOL_H
#define PROTOCOL_H

#define ARB_KEY        "3.141592653582793238"
#define MAX_CMD        128

typedef struct {
  char cmd[128];  // Command 
  char arg[128];  // Argument(s)
} command_t;

extern int RecvData(const int sock, void *d, const int s);
extern int SendData(const int sock, const void *d, const int s);

extern int RecvCommand(const int sock, command_t *cmd);
extern int SendCommand(const int sock, const command_t *cmd);

extern int RecvFile(const int sock, const char *fn);
extern int SendFile(const int sock, const char *fn);

#endif
