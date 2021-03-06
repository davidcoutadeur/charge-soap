/* charge-soap : program that computes benchmarks SOAP server
 * 06/02/2018
 * David Coutadeur
 * */

/* Declaring using modern UNIX */
#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  // for fstat function (to get file_size)
#include <fcntl.h>     // for open function
#include <unistd.h>    // for close function
#include <time.h>      // for calculating time for test
#include <pthread.h>   // threads library
#include <curl/curl.h> // curl library for making http request

#define DEFAULT_URL "http://example.com"
#define MAX_URL_LENGTH 100
#define DEFAULT_FILE "soap.xml"
#define MAX_FILE_LENGTH 30
#define MAX_MANDATORY_ARGS 2
#define MAX_FILESIZE 100000
#define MAX_RESULT_CODE 1000
#define MAX_RES_SIZE 8192
#define CURL_TIMEOUT 15



typedef struct
{
   long long file_size;       // soap file size
   char *data;                // soap data loaded from soap file
   char *url;                 // soap URL
   char is_read;              // tells if data is read : 'y' / 'n'
   long http_code;            // result of soap operation
   char result[MAX_RES_SIZE]; // result data
}
s_soap_param;


int
countArguments ( int nb_params, char *params[] )
{
    int i;
    int res = 0;
    for ( i=0; i < nb_params ; i++ )
    {
        if (params[i][0] != '-')
        {
            res++;
        }
    }
    /* program name (params[0]) + n arguments */
    return (res - 1);
}

char*
getArgument ( int arg, int nb_params, char *params[] )
{
    int i = 1;
    int pos_arg = 0;
    while( i < nb_params )
    {
        if (params[i][0] != '-')
        {
            pos_arg++;
            if ( pos_arg == arg )
            {
                return params[i];
            }
        }
	i++;
    }
    return NULL;
}

char
getOption ( char option, int nb_params, char *params[] )
{
    int i = 1;
    while( i < nb_params )
    {
        if (params[i][0] == '-')
        {
            if ( strlen(params[i]) >= 2 && params[i][1] == option )
            {
                return 't'; // return true
            }
        }
	i++;
    }
    return 'f';
}

void
getBasicArguments( int nb_params, char *params[], int *nb_iter, int *nb_thr, char ***file, char ***url, char *display_results, int *nb_files )
{
    int i;
    int nb_args = countArguments(nb_params, params);
    int nb_opts = (nb_params - 1) - nb_args;
    if( nb_args < MAX_MANDATORY_ARGS )
    {
        printf("USAGE: %s [-v] iterations threads [file] [url] [[file2] [url2]...]\n\n", params[0]);
        printf("ARGUMENTS\n");
        printf(" * iterations (integer, mandatory): number of iterations the soap request is to be launched\n");
        printf(" * threads (integer, mandatory): number of threads for each iteration (total requests = iterations * threads\n");
        printf(" * file (string, optional): soap file containing the soap request (default %s)\n", DEFAULT_FILE);
        printf(" * url (string, optional): where to send the soap request (default %s)\n", DEFAULT_URL);
        printf("OPTIONS\n");
        printf(" * -v: verbose mode. display all results, can be slow\n");
        printf("EXAMPLE:\n");
        printf("%s 10 100 soap.xml http://example.com\n\n", params[0]);
        exit( 0 );
    }
    else
    {
        *nb_iter = atoi( getArgument(1, nb_params, params) );
        *nb_thr = atoi(getArgument(2, nb_params, params));

	/* allocation */
        *file = (char**)malloc((sizeof(char*) * ((nb_args - MAX_MANDATORY_ARGS + 1) / 2) ));
        *url = (char**)malloc((sizeof(char*) * ((nb_args - MAX_MANDATORY_ARGS + 1) / 2) ));
	*nb_files = ((nb_args - MAX_MANDATORY_ARGS + 1) / 2);
	for ( i = 0 ; i < ((nb_args - MAX_MANDATORY_ARGS + 1) / 2) ; i++ )
        {
            (*file)[i] = (char*)malloc((sizeof(char) * MAX_FILE_LENGTH));
            (*url)[i] =  (char*)malloc((sizeof(char) * MAX_URL_LENGTH));
	    /* default value */
            strcpy((*file)[i],DEFAULT_FILE);
            strcpy((*url)[i],DEFAULT_URL);
        }

	/* fill array with user input values */
	for( i = 0 ; i < ((nb_args - MAX_MANDATORY_ARGS + 1) / 2) ; i++ )
        {
            (*file)[i] = getArgument(( (2*i) + MAX_MANDATORY_ARGS + 1), nb_params, params);
	}
	for( i = 0 ; i < ((nb_args - MAX_MANDATORY_ARGS ) / 2) ; i++ )
        {
            (*url)[i] = getArgument(( (2*i) + MAX_MANDATORY_ARGS + 2 ), nb_params, params);
	}

	/* fill verbose option */
	if( nb_opts > 0 )
	{
            (*display_results) = getOption('v', nb_params, params);
	}
    }
}

void
print_status_code(s_soap_param ***soap_params, int nb_files, int nb_iterations, int nb_threads)
{
    int n,i,j, k;
    int result[MAX_RESULT_CODE] = { 0 };
    for ( k=0 ; k < MAX_RESULT_CODE ; k++ )
    {
        for ( n=0 ; n < nb_files ; n++ )
        {
            for ( i=0 ; i < nb_iterations ; i++ )
            {
                for (j = 0; j < nb_threads; j++)
                {
                    if(k == soap_params[n][i][j].http_code)
                    {
                        result[k]++;
                    }
                }
            }
        }
    }
    for ( k=0 ; k < MAX_RESULT_CODE ; k++ )
    {
        if(result[k] > 0)
        {
            printf("http code %d: %d occurrences\n",k,result[k]);
        }
    }
}

void
print_results(s_soap_param ***soap_params, int nb_files, int nb_iterations, int nb_threads)
{
    int n,i,j;

    printf("\n\nResults:\n\n");
    for ( n=0 ; n < nb_files ; n++ )
    {
        for ( i=0 ; i < nb_iterations ; i++ )
        {
            for ( j = 0; j < nb_threads; j++ )
            {
                 printf("file[%d]: iteration: %d | thread: %d | result: %s\n", n, i, j, soap_params[n][i][j].result);
            }
        }
    }
}

size_t
write_data(void *ptr, size_t size, size_t nmeb, void *stream)
{
    // for writing in a file:
    //return fwrite(ptr,size,nmeb,stream);
    s_soap_param *param = (s_soap_param*) stream;
    strncat(param->result, ptr, (nmeb*size));
    return (nmeb*size);
}


size_t
read_data(void *ptr, size_t size, size_t nmeb, void *stream)
{
    s_soap_param *param = (s_soap_param*) stream;
    if(param->is_read == 'y') {
        return 0;
    }
    else {
        memcpy(ptr, param->data, (strlen(param->data)+1));
        param->is_read = 'y';
        return strlen(ptr);
        //return fread(ptr,size,nmeb,stream);
    }
}

static void *
sendMessage (void *soap_param) {
    s_soap_param *param = (s_soap_param*) soap_param;
    char *url = (char*) (*param).url;

    struct curl_slist *header = NULL;
    header = curl_slist_append (header, "Content-Type: text/xml;charset=UTF-8");
    header = curl_slist_append (header, "SOAPAction: \"\"");
    header = curl_slist_append (header, "Transfer-Encoding: chunked");
    header = curl_slist_append (header, "Expect:");
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_data);  //Reads xml file to be sent via POST operation
        curl_easy_setopt(curl, CURLOPT_READDATA, param); 
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);  //Gets data to be written to file
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, param);  //Writes result to file
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)-1);
//        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
//        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);
        //curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        res = curl_easy_perform(curl);
	if(res != CURLE_ABORTED_BY_CALLBACK) {
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &param->http_code);
        }

        curl_easy_cleanup(curl);
	curl_slist_free_all(header);

    }
    return NULL;
}

int
main (int argc, char *argv[])
{
    /* iteration parameters */
    int nb_iterations;
    int j;

    /* Thread parameters */
    int nb_threads;
    int i;
    int ret;
    char display_results = 'f';

    /* file and url parameters */
    int n;
    char **file = NULL;
    char **url = NULL;
    struct stat buf; // for storing file statistics
    long long *file_size; // array of file sizes
    char **soap_file; // array of soap files
    int nb_files = 0;
    int fd;     // for computing file size
    FILE * rfd; // for reading file in memory

    /* structure of soap params */
    s_soap_param ***soap_params; // array[files][iterations][threads]

    /* time parameters */
    struct timespec tstart={0,0}, tend={0,0}; 



    getBasicArguments(argc, argv, &nb_iterations, &nb_threads, &file, &url, &display_results, &nb_files);

    // declare array of threads
    pthread_t thread_soap[nb_files][nb_iterations][nb_threads];

    /* allocate some resources */
    // allocate array of files
    file_size = malloc((sizeof(long long) * nb_files));
    // allocate array of soap files
    soap_file = malloc((sizeof(char*) * nb_files));
    // allocate array of soap_params
    soap_params = malloc((sizeof(s_soap_param**) * nb_files));

    for(n=0 ; n < nb_files ; n++)
    {
        // Get file size
        fd = open(file[n], O_RDONLY );
        if(!fd) {
            perror("Read File Open:");
            exit(1);
        }
        fstat(fd, &buf);
        file_size[n] = buf.st_size;
        close(fd);
        if( (file_size[n])< 0 || (file_size[n]) > MAX_FILESIZE ) {
            printf("File too big: %lld / %d\n", file_size[n], MAX_FILESIZE);
            exit(1);
        }
        // Get file into memory (pointed by soap_file)
        rfd = fopen(file[n], "r");
        if(!rfd) {
            perror("Read File Open:");
            exit(1);
        }
        soap_file[n] = malloc(((file_size[n])+1)*sizeof(char));
        fread(soap_file[n],file_size[n],1,rfd);
        soap_file[n][(file_size[n])]='\0';
        fclose(rfd);
        printf("\nLoading file %s for url %s\n%s\n", file[n], url[n], soap_file[n]);
    
        // save useful params into struct
        soap_params[n] = malloc((sizeof(s_soap_param*) * nb_iterations));
    
        for( i=0 ; i<nb_iterations ; i++) {
            soap_params[n][i] = malloc((sizeof(s_soap_param) * nb_threads));
            for(j=0 ; j<nb_threads ; j++) {
                soap_params[n][i][j].file_size = file_size[n];
                soap_params[n][i][j].url = url[n];
                soap_params[n][i][j].is_read = 'n';
    
    	    // Copy data in soap_params
    	    //soap_params[n][i][j].data = malloc((file_size+1)*sizeof(char));
    	    //strncpy(soap_params[n][i][j].data, soap_file[n], ((file_size[n])+1));
    	    // [or] Use the same file for all datas
                soap_params[n][i][j].data = soap_file[n];
                soap_params[n][i][j].http_code = 0;
                strcpy(soap_params[n][i][j].result,"");
            }
        }
    }
    n=0; i=0; j=0;

    /* Must initialize libcurl before any threads are started */
    curl_global_init(CURL_GLOBAL_ALL);

    // start charge test

    clock_gettime(CLOCK_MONOTONIC, &tstart);

    for ( n=0 ; n < nb_files ; n++ )
    {
        for ( i=0 ; i < nb_iterations ; i++ )
        {
    
            /* launch SOAP request in threads */
            for (j = 0; j < nb_threads; j++)
            {
               ret = pthread_create (
                  &thread_soap[n][i][j], NULL,
                  &sendMessage, (void *) &(soap_params[n][i][j])
               );
        
               if (ret)
               {
                  fprintf (stderr, "thread error: %s\n", strerror (ret));
               }
            }
        
            for (j = 0; j < nb_threads; j++)
            {
                pthread_join (thread_soap[n][i][j], NULL);
            }
        
        }
    }


    clock_gettime(CLOCK_MONOTONIC, &tend);
    printf("Finished in %.3f seconds. \n\n",
           ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - 
           ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));

    print_status_code(soap_params, nb_files, nb_iterations, nb_threads );
    if(display_results == 't')
    {
        print_results(soap_params, nb_files, nb_iterations, nb_threads );
    }

    // free soap_params
    for(n=0 ; n < nb_files ; n++)
        for( i=0 ; i<nb_iterations ; i++)
            free(soap_params[n][i]);
    for(n=0 ; n < nb_files ; n++)
        free(soap_params[n]);
    free(soap_params);

    // free soap_file
    for(n=0 ; n < nb_files ; n++)
        free(soap_file[n]);
    free(soap_file);

    exit( EXIT_SUCCESS );



}


