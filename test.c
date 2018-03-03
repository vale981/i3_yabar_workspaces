#include "test.h"

/*
 * Reads a message from the given socket file descriptor and stores its length
 * (reply_length) as well as a pointer to its contents (reply).
 *
 * Returns -1 when read() fails, errno will remain.
 * Returns -2 on EOF.
 * Returns -3 when the IPC protocol is violated (invalid magic, unexpected
 * message type, EOF instead of a message). Additionally, the error will be
 * printed to stderr.
 * Returns 0 on success.
 *
 */
int ipc_recv_message(uint32_t *message_type,
                     uint32_t *reply_length, uint8_t **reply) {
  /* Read the message header first */
  const uint32_t to_read = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t);
  char msg[to_read];
  char *walk = msg;

  uint32_t read_bytes = 0;
  while (read_bytes < to_read) {
    int n = read(i3_sockfd, msg + read_bytes, to_read - read_bytes);
    if (n == -1)
      return -1;
    if (n == 0) {
      if (read_bytes == 0) {
	return -2;
      } else {
	return -3;
      }
    }

    read_bytes += n;
  }

  if (memcmp(walk, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC)) != 0) {
    return -3;
  }

  walk += strlen(I3_IPC_MAGIC);
  memcpy(reply_length, walk, sizeof(uint32_t));
  walk += sizeof(uint32_t);
  if (message_type != NULL)
    memcpy(message_type, walk, sizeof(uint32_t));

  *reply = malloc(*reply_length);

  read_bytes = 0;
  while (read_bytes < *reply_length) {
    const int n = read(i3_sockfd, *reply + read_bytes, *reply_length - read_bytes);
    if (n == -1) {
      if (errno == EINTR || errno == EAGAIN)
	continue;
      return -1;
    }
    if (n == 0) {
      return -3;
    }

    read_bytes += n;
  }

  return 0;
}


/*
 * Formats a message (payload) of the given size and type and sends it to i3 via
 * the given socket file descriptor.
 *
 * Returns -1 when write() fails, errno will remain.
 * Returns 0 on success.
 *
 */
int ipc_send_message_s(const uint32_t message_type, char* msg){
  ipc_send_message(strlen(msg), message_type, (uint8_t *)msg);
}

int ipc_send_message(const uint32_t message_size,
                     const uint32_t message_type, const uint8_t *payload) {
  const i3_ipc_header_t header = {
    /* We don’t use I3_IPC_MAGIC because it’s a 0-terminated C string. */
    .magic = {'i', '3', '-', 'i', 'p', 'c'},
    .size = message_size,
    .type = message_type};

  if (write(i3_sockfd, ((void *)&header), sizeof(i3_ipc_header_t)) == -1)
    return -1;

  if (write(i3_sockfd, payload, message_size) == -1)
    return -1;
  return 0;
}
int i3_connect() {
  if((i3_sockfd=socket (PF_LOCAL, SOCK_STREAM, 0)) < 0)
    return -1;

  i3_addr.sun_family = AF_LOCAL;
  strcpy(i3_addr.sun_path, i3_socket_path);
  if (connect ( i3_sockfd,
                (struct sockaddr *) &i3_addr,
		sizeof (i3_addr)) < 0)
    return -1;

  return 0;
}

workspace get_workspace_by_num(unsigned int num, int * index){
  workspace cur = workspaces.ws;

  int i = 0;
  fflush(stdout);

  for(; i<workspaces.len && cur->num != num; i++){
    //printf("HEY %i, %i\n", cur->num, workspaces.len);
    cur = cur->next;
  }

  if(index != NULL){
    if(i == workspaces.len)
      i--;
    
    *index = i;
  }
  
  return cur;
}

workspace get_workspace(int index){
  if(index < 0){
    return NULL;
  }
  
  workspace ret = workspaces.ws;
  for(int i = 0; i<workspaces.len && i<index; i++)
    ret = ret->next;
  return ret;
}

int insert_workspace(workspace ws, int position){
  if(position < 0) {
    position = (workspaces.len == 0) ? 0 : workspaces.len;
  }

  //printf("Insert %s at %i \n", ws->name, position);
  fflush(stdout);
    
  if(position > workspaces.len)
    return -1;

  workspace before = get_workspace(position - 1);
  workspace after = (before == NULL) ? NULL : before->next;

  if(workspaces.len == 0 || position == 0){
    after = workspaces.ws;
    before = NULL;
    workspaces.ws = ws;
  }
  
  if(before != NULL){
    before->next = ws;
    ws->prev = before;
  } else {
    ws->prev = NULL;
  }
  
  if(after != NULL){
    after->prev = ws;
    ws->next = after;
  } else {
    ws->next = NULL;
  }

  workspaces.len++;
  return 0;
};

int remove_workspace(int position){
   if(position < 0 || position > workspaces.len - 1)
    return -1;
  
  workspace to_delete = get_workspace(position);
  workspace before = to_delete->prev;
  workspace after = to_delete->next;

  free(to_delete);

  
  if(before != NULL && after != NULL){
    after->prev = before;
    before->next = after;
  } if(after == NULL && before != NULL){
    before->next = NULL;
  } if (after != NULL && before != NULL){
    after->prev = 0;
  }

  if(position == 0){
    workspaces.ws = after;
  }

  workspaces.len--;
  return 1;
}

int clean_workspaces(){
  workspace ws = workspaces.ws;
  while(ws != NULL) {
    workspace next = ws->next;
    free(ws);
    ws = next;
  };

  workspaces.ws = NULL;
  workspaces.len = 0;
}

int sanitize_reply(uint8_t ** reply, uint32_t reply_length){  
  char * reply_san = malloc(reply_length + 1);
  memcpy(reply_san, *reply, reply_length);
  reply_san[reply_length] = '\0';

  free(*reply);

  *reply = reply_san;

  return 0;
};

int get_workspaces(){
  if (ipc_send_message(0, I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL) == -1)
    return -1;
  
  uint32_t reply_length;
  uint32_t reply_type;
  uint8_t *reply;
  int ret;
  if ((ret = ipc_recv_message(&reply_type, &reply_length, &reply)) != 0) {
    if (ret == -1)
      return -1;	
  }

  if(reply_type != I3_IPC_MESSAGE_TYPE_GET_WORKSPACES)
    return -1;

  sanitize_reply(&reply, reply_length);
  
  yajl_val node = yajl_tree_parse(reply, NULL, 0);
  const char * path_num[] = { "num", (const char *) 0 };
  const char * path_name[] = { "name", (const char *) 0 };
  const char * path_focused[] = { "focused", (const char *) 0 };

  if(node && YAJL_IS_ARRAY(node)){
    size_t len = node->u.array.len;
    for(int i = 0; i  < len; i++){
      yajl_val obj = node->u.array.values[ i ];
      yajl_val num = yajl_tree_get(obj, path_num, yajl_t_number);
      yajl_val name = yajl_tree_get(obj, path_name, yajl_t_string);
      yajl_val focused = yajl_tree_get(obj, path_focused, yajl_t_true);

      workspace new_ws = malloc(sizeof(struct workspace_t));
      new_ws->num = (YAJL_GET_INTEGER(num));
      new_ws->active = YAJL_IS_TRUE(focused) ? 1 : 0;
      new_ws->name = strdup(YAJL_GET_STRING(name));

      insert_workspace(new_ws, -1);
    }
  } else {
    return -1;
  }

  yajl_tree_free(node);
  free(reply);
  return 0;
}; 

int format_ws_list(){
  // Let's rock!
  char* template_inactive = "<span>%s</span> ";
  char* template_active = "<span background=\"green\" underline=\"double\">%s</span> ";
  char* buff = malloc(strlen(template_inactive) * (workspaces.len - 1)
		      + strlen(template_active));
  char* walk = buff;
  
  workspace ws = workspaces.ws;
  while(ws != NULL) {
    char * template = ws->active == 1 ? template_active : template_inactive;
    sprintf(walk, template, ws->name);
    walk += strlen(template) - 1;
    ws = ws->next;
  };

  free(workspaces.workspaces_format);
  workspaces.workspaces_format = buff;
}

int listen_to_events(){
  char *listen_command = "[\"workspace\"]";
  ipc_send_message_s(I3_IPC_MESSAGE_TYPE_SUBSCRIBE, listen_command);

  while(1) {
      uint32_t reply_length;
      uint32_t reply_type;
      uint8_t *reply;
      int ret;
      fflush(stdout);
      
      if ((ret = ipc_recv_message(&reply_type, &reply_length, &reply)) != 0){
	free(reply);
	continue;
      }

      if(reply_type == I3_IPC_EVENT_WORKSPACE){
	sanitize_reply(&reply, reply_length);

	yajl_val node = yajl_tree_parse((const char *) reply, NULL, 0);
	const char * type_path[] = { "change", (const char *) 0 };
	const char * curr_num_path[] = { "current", "num", (const char *) 0 };
	const char * curr_name_path[] = { "current", "name", (const char *) 0 };
	
	const char * old_num_path[] = { "old", "num", (const char *) 0 };

	
	yajl_val type = yajl_tree_get(node, type_path, yajl_t_string);
	yajl_val curr_num = yajl_tree_get(node, curr_num_path, yajl_t_number);
	yajl_val old_num = yajl_tree_get(node, old_num_path, yajl_t_number);

	char* type_str = YAJL_GET_STRING(type);

	//printf("EVENT: %s, %i\n", type_str, YAJL_GET_INTEGER(curr_num));
	fflush(stdout);
	switch(type_str[0]){
	case 'i': {
	  int index = -1;
	  workspace ws = get_workspace_by_num(YAJL_GET_INTEGER(curr_num), &index);
	  //printf("%i", index);
	  fflush(stdout);

	  yajl_val curr_name = yajl_tree_get(node, curr_name_path, yajl_t_string);

	  workspace new = malloc(sizeof(struct workspace_t));
	  new->name = strdup(YAJL_GET_STRING(curr_name));
	  new->num = YAJL_GET_INTEGER(curr_num);

	  insert_workspace(new, index + 1);
	  break;
	}

	case 'f': {
	  workspace ws = get_workspace_by_num(YAJL_GET_INTEGER(curr_num), NULL);
	  if(ws != NULL) {
	    ws->active = 1;
	    ws = get_workspace_by_num(YAJL_GET_INTEGER(old_num), NULL);
	    ws->active = 0;
	  }
	  break;
	    
	}
	  
	case 'e': {
	  int index = -1;
	  get_workspace_by_num(YAJL_GET_INTEGER(curr_num), &index);
	  remove_workspace(index);
	  break;
	}
	  
	default:
	  break;
	}

	format_ws_list();
	printf("%s\n", workspaces.workspaces_format);

	yajl_tree_free(node);
      }

      free(reply);
  }
}

int main(int argc, char **argv){
  workspaces.len = 0;
  i3_socket_path = get_i3_socket();
  i3_connect();

  get_workspaces();
  
  format_ws_list();
  listen_to_events();
  
  clean_workspaces();
  free(i3_socket_path);
  free(workspaces.workspaces_format);
}

char* get_i3_socket(){
  FILE *pf;
  char *command = "i3 --get-socketpath";
  char *tmp = malloc(200);
  char* buffer;
  
  // ask i3
  pf = popen(command,"r");

  if(!pf){
    fprintf(stderr, "Could not open pipe for output.\n");
    return NULL;
  }

  // get the socket path
  fscanf(pf, "%s", tmp);
  buffer = malloc(strlen(tmp)+1);
  strcpy(buffer, tmp);
  free(tmp);

  if (pclose(pf) != 0){ // failed to close stream
    return NULL;
  }
       
  return buffer;
}
