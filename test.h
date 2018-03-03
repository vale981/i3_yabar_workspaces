#include <yajl/yajl_tree.h>
#include <yajl/yajl_parse.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "include/ipc.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#define WS_FOCUS "focus"
#define WS_EMPTY "empty"
#define WS_INIT "init"


//TODO: dynamic
char *i3_socket_path;
int i3_sockfd;
struct sockaddr_un i3_addr;

int sanitize_reply(uint8_t ** reply, uint32_t reply_length);

char* get_i3_socket();
int ipc_recv_message(uint32_t *message_type,
                     uint32_t *reply_length, uint8_t **reply);
int ipc_send_message_s(uint32_t message_type, char* msg);
int ipc_send_message(const uint32_t message_size,
                     const uint32_t message_type, const uint8_t *payload);

typedef struct workspace_t* workspace;
struct workspace_t {
  unsigned int num;
  int active;
  char* name;
  workspace next;
  workspace prev;
};

struct workspaces_t {
  unsigned int len;
  workspace ws;
  char* workspaces_format;
} workspaces;


int insert_workspace(workspace ws, int position);
int remove_workspace(int position);
workspace get_workspace(int index);
workspace get_workspace_by_num(unsigned int num, int * index);
int get_workspaces();
int clean_workspaces();
