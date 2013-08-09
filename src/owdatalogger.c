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
#include <stdarg.h>
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
unsigned int isdaemon = 0;

#define LOG_DEBUG2 (LOG_DEBUG + 1)


int prlog(unsigned int lvl, char *fmt, ...)
{
	int ret;
	
	if(fmt == NULL) 
	{
		printf("FORMAT IS NULL !!!\n");
		return -1;
	}
	
	/* Declare a va_list type variable */
	va_list myargs;

	/* Initialise the va_list variable with the ... after fmt */
	va_start(myargs, fmt);

	/* Forward the '...' to vprintf */
	if(isdaemon)
	{
		if(lvl <= LOG_DEBUG)
		{
			syslog(lvl,fmt,myargs);
		}
	}
	else
	{
		ret = vprintf(fmt, myargs);
		printf("\n");
	}

	/* Clean up the va_list */
	va_end(myargs);

	return ret;
}

void signal_handler(int sig)
{
	switch(sig) {
		case SIGHUP:
			prlog(LOG_INFO, "hangup signal catched");
			break;
		case SIGTERM:
			prlog(LOG_INFO,"terminate signal catched");
			exit(0);
			break;
	}
}

int init_senors_list(MYSQL *con)
{
	int i;
	MYSQL_ROW row;
	
	prlog(LOG_INFO, "Init OW data logger sensors list");
	if (mysql_query(con, "SELECT * FROM sensors")) 
	{
		prlog(LOG_ALERT, "%s\n", mysql_error(con));
		mysql_close(con);
		return 3;
	}
	
	MYSQL_RES *result = mysql_store_result(con);
	
	if (result == NULL) 
	{
		prlog(LOG_ALERT, "%s\n", mysql_error(con));
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
	unsigned int retry = 0;
	char *hostname = iniparser_getstring(config,"Database:hostname","localhost");
	char *user = iniparser_getstring(config,"Database:user","weather");
	char *password = iniparser_getstring(config,"Database:password","weather43");
	char *database = iniparser_getstring(config,"Database:database","weather");
	MYSQL *con = mysql_init(NULL);
	
	if (con == NULL)
	{
		prlog(LOG_ALERT,  "mysql_init() failed\n");
		return NULL;
	}
	
	while (mysql_real_connect(con, hostname, user, password, database, 0, NULL, 0) == NULL) 
	{
		retry++;
		if(retry > 5)
		{
			prlog(LOG_ALERT,  "Failed to install database owdatalogger\n");
			mysql_close(con);
			return NULL;
		}
		prlog(LOG_ALERT,  "Install database owdatalogger %i\n",retry);
		system("mysql < /usr/share/owdatalogger.sql");
		sleep(5);
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
	/* Open any logs here */
	isdaemon = 1;
	openlog("owdatalogger",LOG_PID,LOG_DAEMON);
}

int main(int argc, char *argv[])
{
	char *tmp;
	size_t len = 256;
	MYSQL *con;
	unsigned int counter = 0;
	char *owportstr;
	unsigned int owport;
	
	if(argc < 2)
	{
		daemonize();
	}
	
	/* Daemon-specific initialization goes here */
	config = iniparser_load("/etc/owdatalogger.cfg");
	owportstr = iniparser_getstring(config,"OWFS:port","4304");
	owport = atoi(owportstr);

	con = init_db();
	if(con == NULL)
	{
		prlog(LOG_ALERT, "Failed to start OW data logger, DB failed");
	}
	init_senors_list(con);
	OW_init("4304");
	
	prlog(LOG_INFO, "Starting OW data logger");
	
	/* The Big Loop */
	while (1) {
		struct sensor *list = sensors;
		
		while(list != NULL)
		{
			if(counter % list->time == 0)
			{
				char query[128]; 
				OW_get(list->path, &tmp, &len);
				prlog(LOG_DEBUG2, "Read %s: %f", list->path, atof(tmp));
				snprintf(query, 128, "INSERT INTO measures (path,value) VALUES('%s', '%f')", list->path, atof(tmp));
				if (mysql_query(con, query))
				{
					prlog(LOG_ALERT, "ERROR in query %s\n",query);
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
