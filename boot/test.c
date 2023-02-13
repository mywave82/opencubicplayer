#include <stdio.h>
int main(int argc, char *argv[])
{
	printf ("%%5s\n");
	printf ("long:  ->%5s<-   12345679\n", "123456789");
	printf ("short: ->%5s<-   123\n", "123");
	printf ("\n");

	printf ("%%.5s\n");
	printf ("long:  ->%.5s<-   12345679\n", "123456789");
	printf ("short: ->%.5s<-   123\n", "123");
	printf ("\n");

	printf ("%%-5s\n");
	printf ("long:  ->%-5s<-   12345679\n", "123456789");
	printf ("short: ->%-5s<-   123\n", "123");
	printf ("\n");

	printf ("%%-.5s\n");
	printf ("long:  ->%-.5s<-   12345679\n", "123456789");
	printf ("short: ->%-.5s<-   123\n", "123");
	printf ("\n");

	printf ("%%-5.5s\n");
	printf ("long:  ->%-5.5s<-   12345679\n", "123456789");
	printf ("short: ->%-5.5s<-   123\n", "123");
	printf ("\n");

	return 0;
}
