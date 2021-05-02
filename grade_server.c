#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <memory.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "threadpool.c"
#define MAXT_IN_POOL 5
#define THREAD_POOL_SIZE 5

void* do_work(void* p);
threadpool* create_threadpool(int num_threads_in_pool);
void destroy_threadpool(threadpool* destroyme);
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, int param);


#define DO_SYS(syscall) do {		\
    if( (syscall) == -1 ) {		\
        perror( #syscall );		\
        exit(EXIT_FAILURE);		\
    }						\
} while( 0 )


typedef struct TA {
    char *id;
    char *password;
} TA;

typedef struct student {
    char *id;
    char *password;
    char *grade;
} student;

typedef struct logins {
    char *id;
    int is_TA;
    char *clifd;
} logins;

int TA_len = 0;
int student_len = 0;
int logins_len = 0;

static student *students_list;
static TA *TA_list;
static logins *logins_list = NULL;

void input_check(int clifd, char *msg);
int is_logged_in(char *clifd);
void add_new_student(char *id, char *grade);

struct addrinfo*
alloc_tcp_addr(const char *host, uint16_t port, int flags)
{
    int err;   struct addrinfo hint, *a;   char ps[16];

    snprintf(ps, sizeof(ps), "%hu", port);
    memset(&hint, 0, sizeof(hint));
    hint.ai_flags    = flags;
    hint.ai_family   = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_protocol = IPPROTO_TCP;

    if( (err = getaddrinfo(host, ps, &hint, &a)) != 0 ) {
        fprintf(stderr,"%s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    return a;
}


int tcp_establish(int port) {
    int srvfd;
    struct addrinfo *a =
	    alloc_tcp_addr(NULL/*host*/, port, AI_PASSIVE);
    DO_SYS( srvfd = socket( a->ai_family,
				 a->ai_socktype,
				 a->ai_protocol ) 	);
    DO_SYS( bind( srvfd,
				 a->ai_addr,
				 a->ai_addrlen  ) 	);
    DO_SYS( listen( srvfd,
				 5/*backlog*/   ) 	);
    freeaddrinfo( a );
    return srvfd;
}

void read_and_execute(int clifd) {
    char *buf = (char *) malloc(256 * sizeof(char));
    do {
        memset(buf, '\0', 256);
        DO_SYS(read(clifd, buf, 256));
        input_check(clifd, buf);
    } while (strcmp(buf, "Exit"));
    DO_SYS(close(clifd));
    free(buf);
}

void server(int port, threadpool *tp)
{
  int clifd;
  int srvfd = tcp_establish(port);

  while(1) {
        DO_SYS( clifd = accept(srvfd, NULL, NULL) );
        dispatch(tp, &read_and_execute, clifd);
  }
  exit(1);
}




char* my_strtok(char* s, char* delm)
{
    static int currIndex = 0;
    if(!s || !delm || s[currIndex] == '\0') {
        return NULL;
    }
    char *W = (char *)malloc(sizeof(char)*strlen(s));
    memset(W, '\0', strlen(s));
    int i = currIndex, k = 0, j = 0;

    while (s[i] != '\0'){
        j = 0;
        while (delm[j] != '\0'){
            if (s[i] != delm[j])
                W[k] = s[i];
            else goto It;
            j++;
        }

        i++;
        k++;
    }
It:
    W[i] = 0;
    W[k] = '\0';
    currIndex = i+1;
    if (s[i]=='\0') {
        currIndex=0;
    }
    //Iterator = ++ptr;
    return W;
}


void try_login(char **params) {
    int found = 0;
    char *answer;
    answer = malloc(sizeof(char)*35);
    if (!strcmp(params[1], "Login") || !strcmp(params[2], "Login")) {
        strcpy(answer, "Wrong user information\n");
    }
    for (int i=0; i<TA_len; i++) {
        if ((strcmp(TA_list[i].id, params[1])==0) && (strcmp(TA_list[i].password, params[2])==0)) {
            found =1;
            logins_len++;
            logins_list = realloc(logins_list, logins_len);
            if (!logins_list) {
				printf("logins list reallocation failed");
				exit(1);
			}
            logins cur = {.clifd = params[0], .id = params[1], .is_TA = 1};
            logins_list[logins_len - 1] = cur;
            sprintf(answer, "Welcome TA %s\n", cur.id);
            break;
		 }
    }
    if (found==0) {
        for (int i=0; i<student_len; i++) {
            if ((strcmp(students_list[i].id, params[1])==0) && (strcmp(students_list[i].password, params[2])==0)) {
                found=1;
                logins_len++;
                logins_list = realloc(logins_list, logins_len);
                if (!logins_list) {
                    printf("logins list reallocation failed");
                    exit(1);
                }
                logins cur = {.clifd = params[0], .id = params[1], .is_TA = 0};
                logins_list[logins_len - 1] = cur;
                sprintf(answer, "Welcome student %s\n", cur.id);
                break;
            }
        }
    }
    if (found==0) {
	    strcpy(answer, "Wrong user information\n");
    }
    int clifd = atoi(params[0]);
    DO_SYS(   write (clifd, answer, strlen(answer)   ) );
    free(params[0]);
    free(params[1]);
    free(params[2]);
    free(answer);
}


void read_grade(char **params) {
    char *answer;
    int found = 0;
    answer = malloc(19*sizeof(char));
    if (is_logged_in(params[0])==0) {
        strcpy(answer,"Not logged in\n");
    }
    else if (is_logged_in(params[0])==1) {
        if (strcmp(params[1],"ReadGrade")) {
	        for (int i=0; i<student_len; i++) {
		        if (strcmp(students_list[i].id, params[1]) == 0) {
                    found = 1;
                    snprintf(answer, 4, "%s\n", students_list[i].grade);
                }
            }
            if (found==0) {
                strcpy(answer, "Invalid id\n");
            }
        }
        else {
            strcpy(answer, "Missing argument\n");
        }
    }
    else {
        if (strcmp(params[1],"ReadGrade")==0) {
	        for (int i = 0; i < logins_len; i++) {
		        if (!strcmp(logins_list[i].clifd, params[0])) {
			        for (int j=0; j<student_len; j++) {
				        if (!strcmp(logins_list[i].id, students_list[j].id)) {
					        strcpy(answer, students_list[j].grade);
                        }
                    }
				}
			}
		}
        else {
            strcpy(answer, "Action not allowed\n");
        }
	}
    int clifd = atoi(params[0]);
    for (int i=0;i<3;i++) {
        free(params[i]);
    }
	DO_SYS(   write (clifd, answer , strlen(answer)   ) );
    free(answer);
}

int is_logged_in(char *clifd) { //returns 0 if not logged in, 1 if TA and 2 if student
	for (int i = 0; i < logins_len; i++) {
        fprintf(stdout, "Comparing %s with %s", logins_list[i].clifd, clifd);
		if (strcmp(logins_list[i].clifd, clifd)==0) {
			if (logins_list[i].is_TA == 1) {
				return 1;
			}
			else {
				return 2;
			}
		}
	}
	return 0;
}

void grade_list(char **params) {
    char* answer;
    answer = malloc(sizeof(char)*20);
    if (is_logged_in(params[0])==1) {
        int clifd = atoi(params[0]);
        for (int i=0; i<student_len; i++) {
            sprintf(answer, "%s: %s\n", students_list[i].id, students_list[i].grade);
            DO_SYS(   write (clifd, answer , strlen(answer)   ) );
        }
    }
    else if (is_logged_in(params[0])==1) {
        int clifd = atoi(params[0]);
        strcpy(answer, "Action not allowed\n");
        DO_SYS(   write (clifd, answer , strlen(answer)   ) );
    }
    else {
        int clifd = atoi(params[0]);
        strcpy(answer, "Not logged in\n");
        DO_SYS(   write (clifd, answer , strlen(answer)   ) );

    }
    free(params[0]);
    free(params[1]);
    free(params[2]);
    free(answer);
}

void update_grade(char **params) {
    char *answer;
    answer = malloc(sizeof(char)*30);
    if (is_logged_in(params[0])==1) {
        if (!strcmp(params[1], "UpdateGrade") ||  !strcmp(params[2], "UpdateGrade")) {
            int found = 0;
            for (int i = 0; i<student_len; i++) {
                if (strcmp(students_list[i].id,params[1])==0) {
                    found = 1;
                    strcpy(students_list[i].grade, params[2]);
                    break;
                }
            }
            if (found==0) {
                add_new_student(params[1], params[2]);
            }
        }
    }
    else if (is_logged_in(params[0])==2)
    {
        strcpy(answer, "Action not allowed\n");
        int clifd = atoi(params[0]);
        DO_SYS( write(clifd, answer, strlen(answer)));

    }
    else {

        strcpy(answer, "Not logged in\n");
        int clifd = atoi(params[0]);
        DO_SYS( write(clifd, answer, strlen(answer)));
    }
    free(params[0]);
    free(params[1]);
    free(params[2]);
    free(answer);
}

void logout(char **params) {
    char *answer;
    answer = malloc(20*sizeof(char));
    if (is_logged_in(params[0]) == 1 || is_logged_in(params[0])==2) {
        for (int i=0;i<logins_len;i++) {
            if (strcmp(logins_list[i].clifd, params[0])) {
                sprintf(answer, "Good bye %s\n", logins_list[i].id);
                if (i <= logins_len - 2) {
                    for (int c = i; c < logins_len - 1; c++) {
                        logins_list[c] = logins_list[c + 1];
                    }
                }
                logins_len--;
                logins_list = realloc(logins_list, logins_len);
                break;
            }
        }
    }
    else if (!strcmp(params[1], "Exit")) {
    }
    else
    {
        strcpy(answer, "Not logged in\n");
    }
    int clifd = atoi(params[0]);
    DO_SYS(write(clifd, answer, strlen(answer)));
    free(params[0]);
    free(params[1]);
    free(params[2]);
    free(answer);
}


void input_check(int clifd, char *msg) {
    char *params[3];
    params[0] = malloc(20*sizeof(char));
    sprintf(params[0], "%d", clifd);
    char *request1 = my_strtok(msg, " ");
    for (int i = 1; i<3; i++) {
        params[i]=my_strtok(msg, " ");
    }
    if (strcmp(request1, "Login")==0) {
        try_login(params);
    }
	else if (strcmp(request1, "ReadGrade")==0) {
        read_grade(params);
	}
    else if (strcmp(request1, "GradeList")==0) {
        grade_list(params);
    }
    else if (strcmp(request1, "UpdateGrade")==0) {
        update_grade(params);
    }
    else if (strcmp(request1, "Logout")==0) {
        logout(params);
    }
    else if (strcmp(request1, "Exit")==0) {
        sprintf(params[1], "Exit");
        logout(params);
    }
    else {
        DO_SYS(     write (clifd, "Invalid input\n" , 15  ) );
        free(params[0]);
        free(params[1]);
        free(params[2]);
	}
	free(request1);
}


TA *read_TA(char *filename) {
    FILE *fp;
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("File opening failed\n");
        exit(1);
    }
    char line[268];
    while (fgets(line, 268, fp) != NULL) {
        TA_len++;
    }
    static TA *TA_list;
    TA_list = malloc(sizeof(TA)*TA_len);
    if (TA_list== NULL)
    {
        printf("malloc failed in ta_list \n");
        return NULL;
    }

    fseek(fp, 0, SEEK_SET);
    int i = 0;
    while (fgets(line, 268, fp) != NULL) {
        int line_len = strlen(line);
        char *password;
        if (i+1!=TA_len) { //if we are not reading the last line we need to remove the newline character
            password = (char*)malloc((line_len-10)*sizeof(char));
            memset(password,0, line_len-10);
            strncpy(password, line+10, line_len-11);
        }
        else
        {
            password = (char*)malloc((line_len-9)*sizeof(char));
            memset(password,0, line_len-10);
            strncpy(password, line+10, line_len-10);
        }
        if (password == NULL)
        {
            printf("malloc failed in password \n");
            return NULL;
        }
        char *id = NULL;
        id = strtok(line, ":");
        char * temp_id = (char*)malloc((int)strlen(id)+1*sizeof(char));
        if (temp_id == NULL)
        {
            printf("malloc failed in temp_id \n");
            return NULL;
        }
        memset(temp_id,0, (int)strlen(id)+1);
        strcpy(temp_id , id);
        TA cur;
        cur.id = temp_id;
        cur.password = password;
        TA_list[i] = cur;
        i++;
    }
    fclose(fp);
    return TA_list;
}

student *read_students(const char *filename) {
    FILE *fp;
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("File opening failed\n");
        exit(1);
    }
    char line[268];
    while (fgets(line, 268, fp) != NULL) {
        student_len++;
    }
    static student *students_list;
    students_list = malloc(sizeof(student)*student_len);
    fseek(fp, 0, SEEK_SET);
    int i = 0;
    while (fgets(line, 268, fp) != NULL) {
        int line_len = strlen(line);
        char *password;
        if (i+1!=student_len) { //if we are not reading the last line we need to remove the newline character
            password = (char*)malloc((line_len-10)*sizeof(char));
            memset(password,0, line_len-10);
            strncpy(password, line+10, line_len-11);
        }
        else
        {
            password = (char*)malloc((line_len-9)*sizeof(char));
            memset(password,0, line_len-10);
            strncpy(password, line+10, line_len-10);
        }
        if (password == NULL)
        {
            printf("malloc failed in password \n");
            return NULL;
        }
        char *id = NULL;
        id = strtok(line, ":");
        char * temp_id = (char*)malloc((int)strlen(id)+1*sizeof(char));
        if (temp_id == NULL)
        {
            printf("malloc failed in temp_id \n");
            return NULL;
        }
        memset(temp_id,0, (int)strlen(id)+1);
        strcpy(temp_id , id);
        student cur;
        cur.id = temp_id;
        cur.password = password;
        cur.grade = "0";
        students_list[i] = cur;
        i++;
    }
    fclose(fp);
    return students_list;
}

void add_new_student(char *id, char *grade) {
    student *new_list = realloc(students_list, student_len+1);
    if (new_list!=NULL) {
        students_list = new_list;
        student new = {.id=id, .grade=grade, .password=""};
        students_list[student_len]=new;
        student_len++;
    }
}


int main(int argc, char *argv[]) {
    TA_list = read_TA("assistants.txt");
    students_list = read_students("students.txt");
    threadpool *server_tp = create_threadpool(THREAD_POOL_SIZE);
    int port;
    sscanf(argv[1], "%d", &port);
    server(port, server_tp);
    destroy_threadpool(server_tp);
    return 0;
}

