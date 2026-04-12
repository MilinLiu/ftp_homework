#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdint>
#include <thread>   // 🌟 新增：多執行緒支援
#include <atomic>   // 🌟 新增：執行緒安全的計數器

using namespace std;

#define PORT 21
#define BUFFER_SIZE 8192
#define MAX_USERS 2  // 🌟 [上限人數限制] 為了方便測試，我們先設為 2 人

// 執行緒安全的全域變數，紀錄目前連線人數
atomic<int> current_users(0); 

// ==========================================
// 這是專屬服務生：處理單一 Client 所有要求的函式
// ==========================================
void handle_client(int client_socket) {
    cout << "[Thread " << this_thread::get_id() << "] Started handling new client.\n";
    char buffer[BUFFER_SIZE];
    // 1. 接收檔名
    char name_buffer[256] = {0};
    if (recv(client_socket, name_buffer, sizeof(name_buffer), 0) <= 0) {
        perror("Failed to receive filename");
        close(client_socket);
        current_users--; // 🌟 結束時人數減 1
        return;
    }
    string filename(name_buffer);
    
    // 🌟 [防衝突機制] 在檔名前面加上 socket 號碼，防止多人同時傳同名檔案互相覆蓋！
    string new_filename = "uploaded_" + to_string(client_socket) + "_" + filename;

    // 2. 檢查伺服器端是否已經有這個檔案 (斷線續傳)
    uint64_t existing_size = 0;
    ifstream check_file(new_filename, ios::binary | ios::ate);
    if (check_file.is_open()) {
        existing_size = check_file.tellg();
        check_file.close();
    }

    // 3. 把目前伺服器擁有的檔案大小告訴 Client
    if (send(client_socket, &existing_size, sizeof(existing_size), 0) < 0) {
        perror("Failed to send existing size to client");
        close(client_socket);
        current_users--;
        return;
    }

    // 4. 接收 Client 宣告的檔案總大小
    uint64_t total_file_size = 0;
    if (recv(client_socket, &total_file_size, sizeof(total_file_size), 0) <= 0) {
        perror("Failed to receive total file size");
        close(client_socket);
        current_users--;
        return;
    }

    // 5. 使用追加模式 (ios::app) 打開檔案
    ofstream outfile(new_filename, ios::binary | ios::app);
    if (!outfile.is_open()) {
        cerr << "Error: Failed to open file for writing.\n";
        close(client_socket);
        current_users--;
        return;
    }

    // 6. 開始接收資料
    uint64_t total_received = existing_size;
    while (total_received < total_file_size) {
        ssize_t bytes_read = recv(client_socket, buffer, BUFFER_SIZE, 0);
        
        if (bytes_read > 0) {
            outfile.write(buffer, bytes_read);
            total_received += bytes_read;
        } else if (bytes_read <= 0) {
            cerr << "\n[!] Connection lost! Transfer paused for [" << filename << "].\n";
            break; // 斷線跳出迴圈
        }
    }

    if (total_received == total_file_size) {
        cout << "File [" << new_filename << "] received successfully. (100%)\n";
    }

    // 清理資源並將線上人數減 1
    outfile.close();
    close(client_socket);
    current_users--; 
    cout << "[Thread " << this_thread::get_id() << "] Client disconnected. Current users: " << current_users << "/" << MAX_USERS << "\n";
}


// ==========================================
// 這是接待員 (主程式)：只負責在門口拉客
// ==========================================
int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed. (Did you run with sudo?)");
        return EXIT_FAILURE;
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        return EXIT_FAILURE;
    }
    
    cout << "==========================================\n";
    cout << "  Multi-Thread FTP Server Started! \n";
    cout << "  Listening on port " << PORT << "\n";
    cout << "  Max concurrent users: " << MAX_USERS << "\n";
    cout << "==========================================\n";

    // 🌟 接待員無窮迴圈
    while (true) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue; // 失敗就繼續等下一個
        }

        // 🌟 [人數上限限制] 檢查是否客滿
        if (current_users >= MAX_USERS) {
            cout << "Connection rejected: Server is full! (Current users: " << current_users << ")\n";
            close(client_socket); // 直接掛掉對方電話
            continue;
        }

        // 🌟 歡迎光臨，人數 +1
        current_users++;
        cout << "New client connected! Current users: " << current_users << "/" << MAX_USERS << "\n";

        // 🌟 呼叫專屬服務生 (開新的 Thread 處理這個 client)
        // 使用 detach() 讓它在背景自由執行，接待員不用等它結束
        thread client_thread(handle_client, client_socket);
        client_thread.detach(); 
    }

    close(server_fd);
    return 0;
}