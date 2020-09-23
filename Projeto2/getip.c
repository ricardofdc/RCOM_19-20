#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include "string.h"
#include "getip.h"

int getIPByName(char *name, char *ip)
{
    struct hostent *h;

    /*
struct hostent {
	char    *h_name;	Official name of the host. 
    	char    **h_aliases;	A NULL-terminated array of alternate names for the host. 
	int     h_addrtype;	The type of address being returned; usually AF_INET.
	int     h_length;	The length of the address in bytes.
	char    **h_addr_list;	A zero-terminated array of network addresses for the host.
	Host addresses are in Network Byte Order.
};

#define h_addr h_addr_list[0]	The first address in h_addr_list.
*/
    if ((h = gethostbyname(name)) == NULL)
    {
        herror("gethostbyname");
        return -1;
    }

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr)));
    
    strcpy(ip, inet_ntoa(*((struct in_addr *)h->h_addr)));

    return 0;
}
