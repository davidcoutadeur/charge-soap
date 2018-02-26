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
#define DEFAULT_FILE "soap.xml"
#define MAX_MANDATORY_ARGS 2
#define MAX_FILESIZE 100000
#define MAX_RESULT_CODE 1000
#define CURL_TIMEOUT 15



typedef struct
{
   long long file_size;   // soap file size
   char *data;            // soap data loaded from soap file
   char *url;             // soap URL
   char is_read;          // tells if data is read : 'y' / 'n'
   long http_code;        // result of soap operation
}
s_soap_param;


void
getBasicArguments( int nb_args, char *args[], int *nb_iter, int *nb_thr, char **file, char **url )
{
    if( nb_args < ( MAX_MANDATORY_ARGS + 1 ) ) /* program name (args[0]) + 4 arguments */
    {
        printf("USAGE: %s iterations threads file url\n\n", args[0]);
        printf("Launches [threads] threads [iterations] times, each thread making a soap operation described in file [file] to [url]\n");
        printf("EXAMPLE:\n");
        printf("%s 10 100 soap.xml http://example.com\n\n", args[0]);
        exit( 0 );
    }
    else
    {
        *nb_iter = atoi(args[1]);
        *nb_thr = atoi(args[2]);
	if( nb_args >= ( MAX_MANDATORY_ARGS + 2 ) )
	{
          *file = args[3];
	}
	if( nb_args >= ( MAX_MANDATORY_ARGS + 3 ) )
	{
          *url = args[4];
	}
    }
}

void
print_status_code(s_soap_param **soap_params, int nb_iterations, int nb_threads)
{
    int i,j, k;
    int result[MAX_RESULT_CODE] = { 0 };
    for ( k=0 ; k < MAX_RESULT_CODE ; k++ )
    {
        for ( i=0 ; i < nb_iterations ; i++ )
        {
            for (j = 0; j < nb_threads; j++)
            {
                if(k == soap_params[i][j].http_code)
		{
                    result[k]++;
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

size_t
write_data(void *ptr, size_t size, size_t nmeb, void *stream)
{
    //return fwrite(ptr,size,nmeb,stream);
    // do not write anything back
    return nmeb;
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
        //curl_easy_setopt(curl, CURLOPT_WRITEDATA, wfp);  //Writes result to file
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)-1);
//        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
//        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
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

    /* file and url parameters */
    char *file = DEFAULT_FILE;
    char *url = DEFAULT_URL;
    struct stat buf; // for storing file statistics
    long long file_size;
    char *soap_file; // pointer to file in memory

    /* time parameters */
    struct timespec tstart={0,0}, tend={0,0}; 

    getBasicArguments(argc, argv, &nb_iterations, &nb_threads, &file, &url);

    // declare array of threads
    pthread_t thread_soap[nb_iterations][nb_threads];

    // Get file size
    int fd = open(file, O_RDONLY );
    if(!fd) {
        perror("Read File Open:");
        exit(1);
    }
    fstat(fd, &buf);
    file_size = buf.st_size;
    close(fd);
    if( file_size < 0 || file_size > MAX_FILESIZE ) {
        printf("File too big: %lld / %d\n", file_size, MAX_FILESIZE);
        exit(1);
    }
    // Get file into memory (pointed by soap_file)
    FILE * rfd = fopen(file, "r");
    if(!rfd) {
        perror("Read File Open:");
        exit(1);
    }
    soap_file = malloc((file_size+1)*sizeof(char));
    fread(soap_file,file_size,1,rfd);
    soap_file[file_size]='\0';
    fclose(rfd);
    printf("Loading file %s\n%s\n\n", file, soap_file);

    // save useful params into struct
    s_soap_param **soap_params = (s_soap_param**) malloc((sizeof(s_soap_param*) * nb_iterations));

    for( i=0 ; i<nb_iterations ; i++) {
        soap_params[i] = (s_soap_param*) malloc((sizeof(s_soap_param) * nb_threads));
        for(j=0 ; j<nb_threads ; j++) {
            soap_params[i][j].file_size = file_size;
            soap_params[i][j].url = url;
            soap_params[i][j].is_read = 'n';

	    // Copy data in soap_params
	    //soap_params[i][j].data = malloc((file_size+1)*sizeof(char));
	    //strncpy(soap_params[i][j].data, soap_file, (file_size+1));
	    // [or] Use the same file for all datas
            soap_params[i][j].data = soap_file;
            soap_params[i][j].http_code = 0;
        }
    }
    i=0; j=0;

    /* Must initialize libcurl before any threads are started */
    curl_global_init(CURL_GLOBAL_ALL);

    // start charge test

    clock_gettime(CLOCK_MONOTONIC, &tstart);

    for ( i=0 ; i < nb_iterations ; i++ )
    {

        /* launch SOAP request in threads */
        for (j = 0; j < nb_threads; j++)
        {
           ret = pthread_create (
              &thread_soap[i][j], NULL,
              &sendMessage, (void *) &(soap_params[i][j])
           );
    
           if (ret)
           {
              fprintf (stderr, "thread error: %s\n", strerror (ret));
           }
        }
    
        for (j = 0; j < nb_threads; j++)
        {
            pthread_join (thread_soap[i][j], NULL);
        }
    
    }


    clock_gettime(CLOCK_MONOTONIC, &tend);
    printf("Finished in %.3f seconds. \n\n",
           ((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - 
           ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));

    print_status_code(soap_params, nb_iterations, nb_threads );

    for( i=0 ; i<nb_iterations ; i++)
        free(soap_params[i]);
    free(soap_params);
    free(soap_file);

    exit( EXIT_SUCCESS );



}


