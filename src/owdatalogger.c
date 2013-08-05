#include <owcapi.h>
#include <stdio.h>
#include <my_global.h>
#include <mysql.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include "iniparser.h"

#define RUNNING_DIR	"/tmp"
#define LOCK_FILE	"owdatalogger.lock"

struct sensor {
	char path[256];
	unsigned int time;
	struct sensor *next;
};

struct sensor *sensors = NULL;
dictionary* config;

void signal_handler(int sig)
{
	switch(sig) {
		case SIGHUP:
			syslog(LOG_INFO, "hangup signal catched");
			break;
		case SIGTERM:
			syslog(LOG_INFO,"terminate signal catched");
			exit(0);
			break;
	}
}

int init_senors_list(MYSQL *con)
{
	int i;
	MYSQL_ROW row;
	
	if (mysql_query(con, "SELECT * FROM sensors")) 
	{
		syslog(LOG_ALERT, "%s\n", mysql_error(con));
		mysql_close(con);
		return 3;
	}
	
	MYSQL_RES *result = mysql_store_result(con);
	
	if (result == NULL) 
	{
		syslog(LOG_ALERT, "%s\n", mysql_error(con));
		mysql_close(con);
		return 4;
	}

	while ((row = mysql_fetch_row(result))) 
	{
		struct sensor *chain = (struct sensor *)malloc(sizeof(struct sensor));
		strncpy(chain->path,row[0],254);
		chain->time = atoi(row[1]);
		chain->next = sensors;
		sensors = chain;
	}
	
	mysql_free_result(result);

	return 0;
}

MYSQL *init_db(void)
{
	char *hostname = iniparser_getstring(config,"Database:hostname","localhost");
	char *user = iniparser_getstring(config,"Database:user","weather");
	char *password = iniparser_getstring(config,"Database:password","weather43");
	char *database = iniparser_getstring(config,"Database:database","weather");
	MYSQL *con = mysql_init(NULL);
	
	if (con == NULL)
	{
		syslog(LOG_ALERT,  "mysql_init() failed\n");
		return NULL;
	}
	
	if (mysql_real_connect(con, hostname, user, password, database, 0, NULL, 0) == NULL) 
	{
		syslog(LOG_ALERT,  "%s\n", mysql_error(con));
		mysql_close(con);
		return NULL;
	}
	return con;
}

void daemonize()
{
	int i,lfp;
	char str[10];
	if(getppid()==1) return; /* already a daemon */
	i=fork();
	if (i<0) exit(1); /* fork error */
	if (i>0) exit(0); /* parent exits */
	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
	i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
	umask(027); /* set newly created file permissions */
	chdir(RUNNING_DIR); /* change running directory */
	lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
	if (lfp<0) exit(1); /* can not open */
	if (lockf(lfp,F_TLOCK,0)<0) exit(0); /* can not lock */
	/* first instance continues */
	sprintf(str,"%d\n",getpid());
	write(lfp,str,strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP,signal_handler); /* catch hangup signal */
	signal(SIGTERM,signal_handler); /* catch kill signal */
}

int main(void) 
{
	char *tmp;
	size_t len = 256;
	MYSQL *con;
	unsigned int counter = 0;
	char *owportstr;
	unsigned int owport;
	
	daemonize();
	
	/* Open any logs here */        
	openlog("owdatalogger",LOG_PID,LOG_DAEMON);
	
	/* Daemon-specific initialization goes here */
	config = iniparser_load("/etc/owdatalogger.cfg");
	owportstr = iniparser_getstring(config,"OWFS:port","4304");
	owport = atoi(owportstr);

	con = init_db();
	init_senors_list(con);
	OW_init("4304");
	
	syslog(LOG_INFO, "Starting OW data logger");
	
	/* The Big Loop */
	while (1) {
		struct sensor *list = sensors;
		
		while(list != NULL)
		{
			if(counter % list->time == 0)
			{
				char query[128]; 
				OW_get(list->path, &tmp, &len);
				snprintf(query, 128, "INSERT INTO measures (path,value) VALUES('%s', '%f')", list->path, atof(tmp));
				if (mysql_query(con, query))
				{
					syslog(LOG_ALERT, "ERROR in query %s\n",query);
				}
				free(tmp);
			}
			list = list->next;
		}
		counter ++;
		sleep(60);
	}
	
	mysql_close(con);
	closelog();
	
	exit(EXIT_SUCCESS);
}
