#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdint>

using namespace std;

#define PORT 21
#define BUFFER_SIZE 8192

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <filename>\n";
        return EXIT_FAILURE;
    }

    string filepath = argv[1];
    size_t pos = filepath.find_last_of('/');
    string filename = (pos == string::npos) ? filepath : filepath.substr(pos + 1);

    ifstream infile(filepath, ios::binary);
    if (!infile.is_open()) {
        cerr << "Error: Cannot open file " << filepath << "\n";
        return EXIT_FAILURE;
    }

    // 取得檔案真正的總大小
    infile.seekg(0, ios::end);           
    uint64_t total_file_size = infile.tellg(); 
    infile.seekg(0, ios::beg);           

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return EXIT_FAILURE;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        return EXIT_FAILURE;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return EXIT_FAILURE;
    }

    // 1. 送出檔名
    char name_buffer[256] = {0};
    strncpy(name_buffer, filename.c_str(), 255); 
    if (send(sock, name_buffer, sizeof(name_buffer), 0) < 0) {
        perror("Failed to send filename");
        return EXIT_FAILURE;
    }

    // 🌟 [斷線續傳功能 1] 接收 Server 告知的「目前已存在大小」
    uint64_t existing_size = 0;
    if (recv(sock, &existing_size, sizeof(existing_size), 0) <= 0) {
        perror("Failed to receive existing size from server");
        return EXIT_FAILURE;
    }

    // 🌟 [斷線續傳功能 2] 如果伺服器已經有部分檔案，跳轉到該位置！
    if (existing_size > 0 && existing_size <= total_file_size) {
        cout << "Resuming upload from byte: " << existing_size << "\n";
        infile.seekg(existing_size, ios::beg);
    } else if (existing_size > total_file_size) {
        cerr << "Error: Server file is larger than local file!\n";
        return EXIT_FAILURE;
    }

    // 3. 送出檔案總大小，讓 Server 知道何時結束
    if (send(sock, &total_file_size, sizeof(total_file_size), 0) < 0) {
        perror("Failed to send total file size");
        return EXIT_FAILURE;
    }

    cout << "Uploading file... (Total Size: " << total_file_size << " bytes)\n";

    // 4. 迴圈讀取檔案並送出 (如果有斷點，此時 infile 已經從斷點開始讀了)
    while (!infile.eof()) {
        infile.read(buffer, BUFFER_SIZE);
        streamsize bytes_read = infile.gcount(); 
        
        if (bytes_read > 0) {
            if (send(sock, buffer, bytes_read, 0) < 0) {
                // [錯誤處理] 網路斷線
                perror("\n[!] Network disconnected during upload");
                break;
            }
        }
    }

    cout << "Upload process finished!\n";

    infile.close();
    close(sock);
    return 0;
}