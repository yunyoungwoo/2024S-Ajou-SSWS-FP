#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char const *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char filename[BUFFER_SIZE];
    char operation;
    char combined_data[BUFFER_SIZE];
    char message[BUFFER_SIZE];

    // 소켓 생성
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 문자열 주소를 네트워크 주소로 변환
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    // 서버에 연결 요청
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    printf("Connected to server.\n");

    // 파일 이름 및 작업 입력
    printf("Enter filename: ");
    fgets(filename, BUFFER_SIZE, stdin);

    // 개행 문자 제거
    size_t len = strlen(filename);
    if (len > 0 && filename[len-1] == '\n') {
        filename[len-1] = '\0';
    }

    // 파일 이름의 길이 제한
    if (strlen(filename) >= BUFFER_SIZE - 2) {
        fprintf(stderr, "Filename too long.\n");
        return -1;
    }

    printf("Enter operation (r for read, w for write): ");
    scanf(" %c", &operation); // Note the leading space before %c to consume trailing newline

    // 파일 이름과 작업을 결합한 문자열 생성
    snprintf(combined_data, BUFFER_SIZE, "%s,%c", filename, operation);

    // 서버에 결합한 문자열 전송
    send(sock, combined_data, strlen(combined_data), 0);

    // 서버로부터 결과 수신 및 출력
    int bytes_received;
    if (operation == 'r') {
    	// 읽기 작업인 경우
    	while ((bytes_received = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        	// 받은 데이터를 출력
        	printf("%.*s", bytes_received, buffer);
        	printf("\nRead operation completed.\n");
    	}
    } 
    else if (operation == 'w') {
  
    	// 쓰기 작업인 경우
    	printf("Enter message to write: ");
    	scanf("%*c"); // 버퍼에 남아있는 개행 문자를 소비
    	fgets(message, BUFFER_SIZE, stdin);

    	// 개행 문자 제거
    	len = strlen(message);
    	if (len > 0 && message[len-1] == '\n') {
        	message[len-1] = '\0';
    	}

    	// 메시지의 길이 제한
    	if (strlen(message) >= BUFFER_SIZE - 2) {
        	fprintf(stderr, "Message too long.\n");
        	return -1;
    	}

    	// 메시지 전송
    	send(sock, message, strlen(message), 0);
    	printf("Write operation completed.\n");
    }
    
    return 0;
}

