#define _POSIX_C_SOURCE 201000L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <strings.h>
#include <sys/wait.h>

#define SNET_MAX_ITEMS 100
#define SNET_MAX_NODES 30
#define SNET_MAX_PINS  10
#define SNET_PROGNAME  "snet"
#define EVAR_IFS       "IFS"
#define EVAR_DEBUG     "SNET_DEBUG"
#define SHELL          "/bin/sh"


typedef struct snet_node snet_node;
typedef struct snet_item snet_item;
typedef struct snet      snet;

struct snet_node {
    int               fd[2];
    int               refs;
    char              name[1];
};

struct snet_item {
    const char  *name;
    pid_t        pid;
    snet_node  **pins[SNET_MAX_PINS];
    size_t       pinsz;
    const char  *direction;
    const char  *command;
    char         b[1];
};

struct snet {
    snet_item  *items[SNET_MAX_ITEMS];
    size_t      itemsz;
    snet_node  *nodes[SNET_MAX_NODES];
    size_t      nodesz;
    const char *file;
    long        line;
    char        ifs[32];
    bool        debug;
};

bool snet_init             (snet **_snet);
void snet_destroy          (snet  *_snet);
bool snet_parse_include_fp (snet  *_snet, FILE *_fp);
bool snet_parse_include    (snet  *_snet, const char *_file);
bool snet_run_open         (snet  *_snet);
bool snet_run_close        (snet  *_snet);
bool snet_run_fork         (snet  *_snet);
void snet_run_kill         (snet  *_snet);
bool snet_run_wait         (snet  *_snet, int *_retval);

bool snet_item_create (snet_item **_item, snet *_snet, const char _b[]);
void snet_item_free   (snet_item  *_item);
bool snet_item_fork   (snet_item  *_item, snet *_snet);
void snet_item_child  (snet_item  *_item, snet *_snet) __attribute__ ((noreturn));

bool snet_node_create (snet_node **_n, snet *_snet, const char *_name);
bool snet_node_open   (snet_node *_n, snet *_snet);
void snet_node_free   (snet_node *_n);
void snet_node_close  (snet_node *_n);

#define snet_log(SNET, LEVEL, FORMAT, ...) do {                         \
      if ((SNET) && (SNET)->file) {                                     \
         syslog(LEVEL, "%s:%li: " FORMAT, (SNET)->file, (SNET)->line, ##__VA_ARGS__); \
      } else {                                                          \
         syslog(LEVEL, FORMAT, ##__VA_ARGS__);                          \
      }                                                                 \
   } while(0)


struct snet *g_snet = NULL;

void sighandle(int sig) {
    snet_run_kill(g_snet);
}

int main (int argc, char *argv[]) {

    int          e1;
    int          retval  = 1;

    /* Print help if specified. */
    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        printf("Usage: %s [FILENAME]\n", SNET_PROGNAME);
        return 0;
    }

    /* Initialize logging. */
    openlog(SNET_PROGNAME, LOG_PERROR, LOG_USER);
    
    /* Initialize network. */
    e1 = snet_init(&g_snet);
    if (!e1) { goto cleanup; }

    /* Parse the network specification. */
    if (argc == 1) {
        e1 = snet_parse_include_fp(g_snet, stdin);
    } else {
        e1 = snet_parse_include(g_snet, argv[1]);
    }
    if (!e1) { goto cleanup; }

    /* Create descriptors, fork the nodes and wait. */
    e1 =snet_run_open(g_snet) &&
        snet_run_fork(g_snet) &&
        snet_run_close(g_snet) &&
        (signal(SIGINT, sighandle) || true) &&
        snet_run_wait(g_snet, &retval);
    if (!e1) { goto cleanup; }
    
    /* Cleanup. */
    if (g_snet && g_snet->debug) {
        snet_log(g_snet, LOG_DEBUG, "Finishing with %i", retval);
    }
    cleanup:
    snet_destroy(g_snet);
    fflush(stderr);
    fflush(stdout);
    return retval;
}



bool snet_init(snet **_snet) {
    struct snet *snet = calloc(1, sizeof(struct snet));
    if (!snet) {
        syslog(LOG_ERR, "%s", strerror(errno));
        return false;
    }
    const char *ifs_v = getenv(EVAR_IFS);
    if (ifs_v) {
        strncpy(snet->ifs, ifs_v, sizeof(snet->ifs)-1);
    } else {
        strcpy(snet->ifs, "\t\n\r ");
    }
    const char *debug_v = getenv(EVAR_DEBUG);
    if (debug_v) {
        snet->debug = true;
    } else {
        snet->debug = false;
    }
    *_snet = snet;
    return true;
}
void snet_destroy(snet *_snet) {
    if (_snet) {
        for (size_t i=0; i<_snet->itemsz; i++) {
            snet_item_free(_snet->items[i]);
        }
        for (size_t i=0; i<_snet->nodesz; i++) {
            snet_node_free(_snet->nodes[i]);
        }
        free(_snet);
    }
}
bool snet_parse_include_fp(snet *_snet, FILE *_fp) {
    struct snet_item *item;
    char b[1024] = {0}, *s,*c, *t1;
    
    for (_snet->line=1; (s = fgets(b, sizeof(b)-1, _fp)) && _snet->itemsz<SNET_MAX_ITEMS; _snet->line++) {
        
        /* Remove comments and shift. Skip empty lines. */
        if (s[0] == '*') s[0] = '\0';
        if ((c = strchr(s, '#'))) *c = '\0';
        while (*s != '\0' && strchr(_snet->ifs, *s)) s++;
        if (*s == '\0') continue;
        
        /* Parse directives. */
        if (*s == '.') {
            char *directive = strtok_r(s, _snet->ifs, &t1);
            if (!directive) continue;
            if (!strcasecmp(directive, ".END")) break; 
            snet_log(_snet, LOG_ERR, "Unknown directive: %s", directive);
            continue;
        }

        /* Too much items. */
        if (_snet->itemsz == SNET_MAX_ITEMS) {
            snet_log(_snet, LOG_ERR, "Too much items in the network. max=%i", SNET_MAX_ITEMS);
            return false;
        }
        
        /* Allocate item and copy string. */
        if (!snet_item_create(&item, _snet, s)) {
            return false;
        }

        _snet->items[_snet->itemsz++] = item;
    }
    
    return true;
}
bool snet_parse_include(snet *_snet, const char *_file) {
    FILE *fp = fopen(_file, "rb");
    if (!fp) { syslog(LOG_ERR, "%s: %s", _file, strerror(errno)); return false; }
    const char *s_file = _snet->file;
    long        s_line = _snet->line;
    _snet->file = _file;
    _snet->line = 0;
    bool err = snet_parse_include_fp(_snet, fp);
    fclose(fp);
    _snet->file = s_file;
    _snet->line = s_line;
    return err;
}
bool snet_run_open(snet *_snet) {
    for (size_t n=0; n<_snet->nodesz; n++) {
        if (!snet_node_open(_snet->nodes[n], _snet)) {
            snet_run_close(_snet);
            return false;
        }
    }
    return true;
}
bool snet_run_close(snet *_snet) {
    for (size_t n=0; n<_snet->nodesz; n++) {
        snet_node_close(_snet->nodes[n]);
    }
    return true;
}
bool snet_run_fork(snet *_snet) {
    for (size_t i=0; i<_snet->itemsz; i++) {
        if (!snet_item_fork(_snet->items[i], _snet)) {
            snet_run_kill(_snet);
            snet_run_wait(_snet, NULL);
            return false;
        }
    }
    return true;
}
void snet_run_kill(snet *_snet) {
    for (size_t i=0; i<_snet->itemsz; i++) {
        if (_snet->items[i]->pid != -1) {
            if (_snet->debug) {
                snet_log(_snet, LOG_DEBUG, "%-10s: Killing...", _snet->items[i]->name);
            }
            kill(SIGINT, _snet->items[i]->pid);
        }
    }
}
bool snet_run_wait(snet *_snet, int *_retval) {
    int retval,status;
    *_retval = 0;
    for (size_t i=0; i<_snet->itemsz; i++) {
        if (_snet->items[i]->pid != -1) {
            assert(waitpid(_snet->items[i]->pid, &status, 0)!=-1);
            if (WIFEXITED(status)) {
                retval = WEXITSTATUS(status);
                if (_snet->debug) {
                    snet_log(_snet, LOG_DEBUG, "%-10s: Waited %i", _snet->items[i]->name, retval);
                }
            } else {
                retval = 1;
            }
            if (retval && _retval) {
                *_retval = retval;
            }
        }
    }
    return true;
}




/* --------------------------------------------------------------------------
 * ---- ITEMS ---------------------------------------------------------------
 * -------------------------------------------------------------------------- */
bool snet_item_create(snet_item **_item, snet *_snet, const char _b[]) {
    
    char       *str, *t1;
    snet_node **pin;
    snet_item  *item;
    int         e1;
    size_t      b_len;

    /* Allocate item and copy string. */
    b_len = strlen(_b);
    item = calloc(1, sizeof(struct snet_item)+b_len);
    if (!item/*err*/) {
        snet_log(_snet, LOG_ERR, "%s", strerror(errno));
        return false;
    }
    memcpy(item->b, _b, b_len);

    /* Get name. */
    item->name = strtok_r(item->b, _snet->ifs, &t1);
    item->pid  = -1;

    /* Parse nodes. */
    str = strtok_r(NULL, _snet->ifs, &t1);
    while(str != NULL                 /* Missing ending. */           &&
          str[0] != '@'               /* Good nodes section final. */ &&
          item->pinsz < SNET_MAX_PINS /* To much pins. */) {

        /* Search node. */
        pin = &_snet->nodes[_snet->nodesz];
        for (size_t n = 0; n<_snet->nodesz; n++) {
            if (!strcasecmp(str, _snet->nodes[n]->name)) {
                pin = &_snet->nodes[n];
                break;
            }
        }
        
        /* If doesn't exist, create it. */
        if (!(*pin)) {
            e1 = snet_node_create(pin, _snet, str);
            if (!e1) { snet_item_free(item); return false; }
            _snet->nodesz++;
        }

        /* Increase reference. */
        item->pins[item->pinsz++] = pin;
        (*pin)->refs++;

        str = strtok_r(NULL, _snet->ifs, &t1);
    }

    /* Bad loop finals. */
    if (str == NULL || item->pinsz == SNET_MAX_PINS) {
        snet_log(_snet, LOG_ERR, "%s", "Invalid syntax: Missing @ or too much pins.");
        free(item);
        return false;
    }

    /* Get directions. */
    item->direction = str+1;
    if (strlen(item->direction) < item->pinsz) {
        snet_log(_snet, LOG_ERR, "%s", "Invalid pin count.");
        free(item);
        return false;
    }
    
    /* Save command. */
    item->command = strtok_r(NULL, "\r\n", &t1);
    if (!item->command) {
        snet_log(_snet, LOG_ERR, "%s", "Missing command.");
        free(item);
        return false;
    }
    while (strchr(_snet->ifs, *item->command)) item->command++;

    
    *_item = item;
    return true;   
}
void snet_item_free(snet_item *_item) {
    if (_item) {
        for (size_t i=0; i<_item->pinsz; i++) {
            (*(_item->pins[i]))->refs--;
        }
    }
}
bool snet_item_fork(snet_item *_item, snet *_snet) {
    _item->pid = fork();
    if (_item->pid == -1) {
        snet_log(_snet, LOG_ERR, "Can't fork: %s", _item->command);
        return false;
    }
    if (_item->pid == 0) {
        snet_item_child(_item, _snet);
    }
    return true;
}
void snet_item_child(snet_item *_item, snet *_snet) {
    
    /* Close other descriptors. */
    for (int i=0; i<_snet->nodesz; i++) {
        snet_node *p = _snet->nodes[i];
        for (int j=0; j<_item->pinsz; j++) {
            if (&_snet->nodes[i] == _item->pins[j]) { p = NULL; break; }
        }
        if (p) {
            if (p->fd[0]!=-1) close(p->fd[0]);
            if (p->fd[1]!=-1) close(p->fd[1]);
        }
    }

    /* Set environment variables and close other side. */
    char   nn[SNET_MAX_PINS*20];
    size_t nnsz = 0;
    char   nX_var[5];
    char   nX_val[20];
    for (int i=0; i<_item->pinsz; i++) {
        sprintf(nX_var, "n%i", i+1);
        int fd_o = -1;
        int fd_c = -1;
        if (_item->direction[i]=='r') {
            fd_o = (*_item->pins[i])->fd[0];
            fd_c = (*_item->pins[i])->fd[1];
        } else if (_item->direction[i]=='w') {
            fd_o = (*_item->pins[i])->fd[1];
            fd_c = (*_item->pins[i])->fd[0];
        } else if (_item->direction[i]=='0') {
            dup2((*_item->pins[i])->fd[0], 0);
            close((*_item->pins[i])->fd[1]);
            fd_o = 0;
            fd_c = (*_item->pins[i])->fd[1];
        } else if (_item->direction[i]=='1') {
            dup2((*_item->pins[i])->fd[1], 1);
            close((*_item->pins[i])->fd[1]);
            fd_o = 1;
            fd_c = (*_item->pins[i])->fd[0];
        } else if (_item->direction[i]=='2') {
            dup2((*_item->pins[i])->fd[1], 2);
            close((*_item->pins[i])->fd[1]);
            fd_o = 2;
            fd_c = (*_item->pins[i])->fd[0];
        } else {
            fd_o = (*_item->pins[i])->fd[0];
            fd_c = (*_item->pins[i])->fd[1];
        }
        if (fd_c!=-1) close(fd_c);
        sprintf(nX_val, "%i", fd_o);
        nnsz += sprintf(nn+nnsz, "%s%s",(i)?" ":"", nX_val);
        setenv(nX_var, nX_val, 1);
    }
    setenv("nn", nn, 1);
    
    /* Execute. */
    if (_snet->debug) {
        snet_log(_snet, LOG_DEBUG, "%-10s: %-10s : %s", _item->name, nn, _item->command);
    }
    execl(SHELL, SHELL, "-e", "-c", _item->command, NULL);
    snet_log(_snet, LOG_ERR, "%s", "Can't execute.");
    exit(1);
}


/* --------------------------------------------------------------------------
 * ---- NODES ---------------------------------------------------------------
 * -------------------------------------------------------------------------- */
bool snet_node_create(snet_node **_node, snet *_snet, const char *_name) {
    size_t     name_l = strlen(_name);
    snet_node *node   = calloc(1, sizeof(snet_node)+name_l);
    if (!node/*err*/) {
        snet_log(_snet, LOG_ERR, "%s", strerror(errno));
        return false;
    }
    node->fd[0] = -1;
    node->fd[1] = -1;
    memcpy(node->name, _name, name_l+1);
    *_node = node;
    return true;
}
bool snet_node_open(snet_node *_node, snet *_snet) {
    snet_node_close(_node);
    if (/* Dangling and zero are ground. */ !strcmp(_node->name, "0") || _node->refs == 1) {
        _node->fd[0] = open("/dev/null", O_RDONLY);
        _node->fd[1] = open("/dev/null", O_WRONLY);
        if (_node->fd[0] == -1 || _node->fd[1] == -1) {
            snet_log(_snet, LOG_ERR, "Node %s: /dev/null: %s", _node->name, strerror(errno));
            snet_node_close(_node);
            return false;
        }
        return true;
    } else /* By default pipe. */ {
        if (pipe(_node->fd) == -1) {
            snet_log(_snet, LOG_ERR, "Node %s: Can't create pipe: %s", _node->name, strerror(errno));
            return false;
        }
        return true;
    }
}
void snet_node_free(snet_node *_node) {
    if (_node) {
        snet_node_close(_node);
        free(_node);
    }
}
void snet_node_close(snet_node *_node) {
    if (_node->fd[0]!=-1) close(_node->fd[0]);
    if (_node->fd[1]!=-1) close(_node->fd[1]);
}
