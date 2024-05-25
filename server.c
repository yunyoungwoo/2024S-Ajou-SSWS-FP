#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CONNECTIONS 5
#define BUFFER_SIZE 1024

pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER; // reader-writer lock
pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER; // 클라이언트 ID를 위한 mutex
int client_id_counter = 1; // 클라이언트 ID counter

void log_event(const char *event, unsigned long thread_id, int client_id, const char *additional_info) {
    FILE *log_file = fopen("file_log.txt", "a"); // 로그 파일 열기 (이어쓰기 모드)
    if (log_file != NULL) {
        if (additional_info) {
            fprintf(log_file, "[Thread ID: %lu, Client ID: %d] %s - %s\n", thread_id, client_id, event, additional_info); // 이벤트를 로그 파일에 기록
        } else {
            fprintf(log_file, "[Thread ID: %lu, Client ID: %d] %s\n", thread_id, client_id, event); // 이벤트를 로그 파일에 기록
        }
        fclose(log_file); // 파일 닫기
    }
}

void process_request(int client_socket, char *filename, char operation, unsigned long thread_id, int client_id) {
    char buffer[BUFFER_SIZE];
    FILE *file;
    int bytes_read;

    // 로그 이벤트 작성
    char event[BUFFER_SIZE];
    char additional_info[BUFFER_SIZE] = {0};

    snprintf(event, BUFFER_SIZE, "Received %s operation for file: %s", (operation == 'r' ? "read" : "write"), filename);
    log_event(event, thread_id, client_id, NULL);

    // 파일 열기
    if (operation == 'r') {
        pthread_rwlock_rdlock(&rwlock); // 읽기 잠금

        // 파일 열기
        file = fopen(filename, "r");
        if (!file) {
            strcpy(buffer, "File not found");
            send(client_socket, buffer, strlen(buffer), 0);
            pthread_rwlock_unlock(&rwlock); // 잠금 해제
            return;
        }

        // 파일 읽기
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
            send(client_socket, buffer, bytes_read, 0);
            strncat(additional_info, buffer, bytes_read); // 읽은 내용을 추가 정보에 기록
        }

        fclose(file);
        pthread_rwlock_unlock(&rwlock); // 잠금 해제

        // 로그 이벤트 작성
        snprintf(event, BUFFER_SIZE, "Completed read operation for file: %s", filename);
        log_event(event, thread_id, client_id, additional_info);
    } else if (operation == 'w') {
        pthread_rwlock_wrlock(&rwlock); // 쓰기 잠금

        // 파일 열기
        file = fopen(filename, "w");
        if (!file) {
            strcpy(buffer, "Error writing to file");
            send(client_socket, buffer, strlen(buffer), 0);
            pthread_rwlock_unlock(&rwlock); // 잠금 해제
            return;
        }

        // 파일 쓰기
        while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, bytes_read, file);
            strncat(additional_info, buffer, bytes_read); // 쓴 내용을 추가 정보에 기록
        }

        fclose(file);
        pthread_rwlock_unlock(&rwlock); // 잠금 해제

        // 로그 이벤트 작성
        snprintf(event, BUFFER_SIZE, "Completed write operation for file: %s", filename);
        log_event(event, thread_id, client_id, additional_info);
    }
}

void *client_handler(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    int client_id;

    // 클라이언트 ID 할당
    pthread_mutex_lock(&id_mutex);
    client_id = client_id_counter++;
    pthread_mutex_unlock(&id_mutex);

    unsigned long thread_id = (unsigned long)pthread_self(); // 스레드 ID

    char buffer[BUFFER_SIZE] = {0};
    char *filename;
    char operation;

    printf("Client connected with ID: %d\n", client_id);

    // 클라이언트로부터 결합된 문자열 수신
    recv(client_socket, buffer, BUFFER_SIZE, 0);

    // 파일 이름과 작업 유형을 분리하여 추출
    filename = strtok(buffer, ",");
    operation = strtok(NULL, ",")[0];

    printf("Thread ID: %lu - Client ID: %d requested: %s, operation: %c\n", thread_id, client_id, filename, operation);

    // 파일 작업 처리 및 결과 전송
    process_request(client_socket, filename, operation, thread_id, client_id);

    printf("Thread ID: %lu - Client ID: %d Operation completed.\n", thread_id, client_id);

    // 클라이언트 소켓 닫기
    close(client_socket);

    free(socket_desc); // 동적 할당된 소켓 디스크립터 메모리 해제

    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // 소켓 파일 디스크립터 생성
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 소켓 옵션 설정
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 소켓을 특정 포트에 바인딩
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 클라이언트로부터의 연결 요청 대기
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // 연결 요청 수락
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // 클라이언트 핸들러를 위한 쓰레드 생성
        pthread_t thread;
        int *socket_desc = malloc(sizeof(int));
        *socket_desc = new_socket;

        if (pthread_create(&thread, NULL, client_handler, (void *)socket_desc) < 0) {
            perror("could not create thread");
            return 1;
        }

        pthread_detach(thread); // 클라이언트와의 연결을 담당하는 쓰레드를 분리
    }

    // 서버 소켓 닫기
    close(server_fd);

    return 0;
}

