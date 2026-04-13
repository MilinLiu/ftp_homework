#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstdint>
#include <thread>
#include <atomic>
#include <mutex>          // 🌟 新增：互斥鎖 (保護共享資源)
#include <unordered_set>  // 🌟 新增：用來紀錄「正在上傳中的檔案」

using namespace std;

#define PORT 21
#define BUFFER_SIZE 8192
#define MAX_USERS 2 

atomic<int> current_users(0); 

// ==========================================
// 🌟 終極防衝突機制：全域檔案鎖
// ==========================================
mutex file_mutex;
unordered_set<string> active_uploads;

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
        current_users--; 
        return;
    }
    string filename(name_buffer);
    
    // 🌟 拿掉 Socket ID！改回乾淨的檔名，讓斷線續傳能順利找到舊檔案！
    string new_filename = "uploaded_" + filename;

    // ==========================================
    // 🌟 真正的多執行緒防衝突：檢查檔案是否正在被別人寫入
    // ==========================================
    {
        lock_guard<mutex> lock(file_mutex); // 上鎖
        if (active_uploads.find(new_filename) != active_uploads.end()) {
            // 如果檔案已經在黑名單裡，代表有人正在傳！
            cerr << "[!] Reject: File [" << filename << "] is currently being uploaded by another user.\n";
            close(client_socket);
            current_users--;
            return; // 直接踢掉這個客人，保護檔案
        }
        // 如果沒有人傳，就把這個檔案加入「正在上傳黑名單」
        active_uploads.insert(new_filename);
    } // 自動解鎖

    // 💡 建立一個「收尾小幫手 (Lambda)」，確保不論是傳完還是發生錯誤，離開時一定會幫檔案解鎖
    auto cleanup_and_exit = [&]() {
        {
            lock_guard<mutex> lock(file_mutex);
            active_uploads.erase(new_filename); // 從黑名單移除
        }
        close(client_socket);
        current_users--;
    };

    // 2. 檢查伺服器端是否已經有這個檔案 (斷線續傳)
// 2. 檢查伺服器端是否已經有這個檔案 (斷線續傳)
    uint64_t existing_size = 0;
    ifstream check_file(new_filename, ios::binary | ios::ate);
    if (check_file.is_open()) {
        existing_size = check_file.tellg();
        check_file.close();
        
        // 讓 Server 宣告重連到的檔案大小，並且印出來給管理員看
        cout << "Found existing file. Current size: " << existing_size << " bytes.\n"; 
    }
    // 3. 把目前伺服器擁有的檔案大小告訴 Client
    if (send(client_socket, &existing_size, sizeof(existing_size), 0) < 0) {
        perror("Failed to send existing size to client");
        cleanup_and_exit(); // 🌟 使用小幫手安全退出
        return;
    }

    // 4. 接收 Client 宣告的檔案總大小
    uint64_t total_file_size = 0;
    if (recv(client_socket, &total_file_size, sizeof(total_file_size), 0) <= 0) {
        perror("Failed to receive total file size");
        cleanup_and_exit();
        return;
    }

    // 5. 使用追加模式 (ios::app) 打開檔案
    ofstream outfile(new_filename, ios::binary | ios::app);
    if (!outfile.is_open()) {
        cerr << "Error: Failed to open file for writing.\n";
        cleanup_and_exit();
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

    // 🌟 正常傳完，或是斷線暫停，執行安全清理與解鎖
    outfile.close();
    cleanup_and_exit(); 
    cout << "[Thread " << this_thread::get_id() << "] Client disconnected. Current users: " << current_users << "/" << MAX_USERS << "\n";
}

// ==========================================
// 這是接待員 (主程式)：只負責在門口拉客
// ==========================================
int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
// 1. 建立 Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
// 2. 綁定 IP 和 Port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
// 3. 開始監聽
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed. (Did you run with sudo?)");
        return EXIT_FAILURE;
    }
// 4. 開始聽門口，等待客人來訪
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        return EXIT_FAILURE;
    }
    // 🌟 印出伺服器資訊，讓管理員一目了然
    cout << "==========================================\n";
    cout << "  Multi-Thread FTP Server Started! \n";
    cout << "  Listening on port " << PORT << "\n";
    cout << "  Max concurrent users: " << MAX_USERS << "\n";
    cout << "==========================================\n";

    while (true) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_socket < 0) {
            perror("Accept failed");
            continue; 
        }

        if (current_users >= MAX_USERS) {
            cout << "Connection rejected: Server is full! (Current users: " << current_users << ")\n";
            close(client_socket); 
            continue;
        }

        current_users++;
        cout << "New client connected! Current users: " << current_users << "/" << MAX_USERS << "\n";

        thread client_thread(handle_client, client_socket);
        client_thread.detach(); 
    }

    close(server_fd);
    return 0;
}