# C++ Multi-Threaded FTP Server & Client
這是一個基於 Linux Socket API 與 C++11 標準實作的檔案傳輸系統 (FTP)。
<<<<<<< HEAD
本專案不僅達成基本檔案傳輸需求，更完整實作了**多執行緒併發處理**、**4GB 以上大檔案傳輸**，以及強大的**斷線續傳**與**檔案級互斥鎖 (File Lock)** 功能。

### 基本功能 (Basic Requirements)
- [x] **基礎檔案上傳 : Client 成功連線並上傳檔案，Server 端接收後加上 `uploaded_` 前綴存檔，並使用指定 Port 21。
- [x] **嚴謹的錯誤處理 (Error Handling)**: 對於所有 System Calls (如 `socket`, `bind`, `accept`, `recv`, `send`) 皆實作回傳值檢查，並使用 `perror` 輸出系統層級錯誤訊息。

### 進階選要功能 (Advanced Features - 40%)
- [x] **支援 4GB 以上大檔案傳輸 : 放棄傳統 `int`，全面改用 `<cstdint>` 之 `uint64_t` 型態精確紀錄檔案大小，並採用 8KB Buffer 分塊傳輸，徹底避免記憶體溢位 (OOM) 與整數溢位。
- [x] **網路斷線續傳功能 : 
  - 運用 C++ `<fstream>` 的 `ios::app` (追加模式) 與 `tellg()` / `seekg()` 精準控制讀寫指標。
  - Server 與 Client 在連線初期會自動核對檔案大小，精準跳轉 (Seek) 至斷點繼續傳輸，實測強制中斷 (Ctrl+C) 後重連可 100% 完美接關。
- [x] **支援多人連線 : 導入 C++11 `std::thread`，採用「Main Thread 接待 + Worker Thread 處理」架構，各 Client 傳檔互不阻塞。
- [x] **上限人數限制 : 使用 `std::atomic<int>` 實作執行緒安全的連線計數器，超過設定上限 (`MAX_USERS`) 時會安全拒絕連線。

---

## 🛠️ 開發與執行環境 (Environment)
* **OS:** Linux (Ubuntu / WSL2)
* **Compiler:** `g++` (支援 C++11)
* **Libraries:** `<thread>`, `<atomic>`, `<mutex>`, `<unordered_set>`, `<sys/socket.h>`, `<arpa/inet.h>`

---

## 💡 核心架構與防禦性設計重點說明 (Architecture & Security)

為了確保伺服器的高併發穩定性與資料正確性，本專案實作了以下企業級防護機制：

1. **檔案級互斥鎖 (File-level Mutex Lock & Set)**
   - **痛點：** 在多執行緒環境下，若兩位使用者同時上傳「同檔名」的檔案，會導致 Race Condition 與資料毀損。
   - **解法：** 導入 `std::mutex` 與 `std::unordered_set` 實作檔案黑名單機制。當某檔案正在被寫入時，該檔名會被鎖定；若有其他 Client 嘗試上傳同名檔案，Server 會主動攔截並印出 `Reject: File is currently being uploaded by another user`，確保檔案的絕對安全。
2. **優雅降級與自動解鎖 (Graceful Exit & Lambda Cleanup)**
   - 實作 Lambda 函數 `cleanup_and_exit()` 作為收尾機制。無論 Client 是順利傳完，或是遭遇網路瞬斷、強制關閉，Server 都能捕捉異常，並確保 `mutex` 解鎖、清空黑名單並遞減線上人數，達成 0 資源洩漏 (Zero Resource Leak)。
3. **TCP 黏包防護 (Stream Boundary Control)**
   - 規定 Client 連線後，依序傳送固定長度 256 bytes 的檔名與 8 bytes 的檔案大小，Server 確實解析 Header 後才開始接收本體，確保通訊協定絕對精確。
---

=======
本專案不僅達成基本檔案傳輸需求，更完整實作了多執行緒併發處理、4GB 以上大檔案傳輸，以及斷線續傳功能。
🌟 實作功能清單 (Features)
基本功能 (Basic Requirements)
- [x] 基礎檔案上傳 (60%): Client 成功連線並上傳檔案，Server 端接收後維持原檔名 (附加 `uploaded_` 前綴與 socket ID 以防衝突)，並使用指定 Port 21。
- [x] 嚴謹的錯誤處理 (Error Handling): 對於所有 System Calls (如 `socket`, `bind`, `accept`, `recv`, `send`) 皆實作回傳值檢查，並使用 `perror` 輸出系統層級錯誤訊息。
- [ ] 
進階選要功能 (Advanced Features)
- [x] 支援 4GB 以上大檔案傳輸 : 放棄傳統 `int`，全面改用 `<cstdint>` 之 `uint64_t` 型態精確紀錄檔案大小，並採用 8KB Buffer 分塊傳輸 (Chunking)，避免記憶體溢位 (OOM)。
- [x] 網路斷線續傳功能 : 
  - 運用 C++ `<fstream>` 的 `ios::app` (追加模式) 與 `tellg()` / `seekg()` 精準控制讀寫指標。
  - 雙方針對斷點進行溝通，Client 可從網路中斷處繼續上傳，無需重新傳送整個檔案。
- [x] 支援多人連線 : 導入 C++11 `std::thread`，採用「Main Thread 監聽 + Worker Thread 處理」架構，各 Client 互不阻塞。
- [x] 上限人數限制 : 使用 `std::atomic<int>` 實作執行緒安全 (Thread-safe) 的連線計數器，超過設定上限 (`MAX_USERS`) 時會安全拒絕連線。
---
 🛠️ 開發與執行環境 (Environment)
* OS: Linux (Ubuntu 22.04 / WSL2)
* Compiler: `g++` (支援 C++11 或以上)
* Libraries: `<thread>`, `<atomic>`, `<sys/socket.h>`, `<arpa/inet.h>`
---
>>>>>>> d6bcc6cb924887d56cbd92a95ec17f328a70d81c
 🚀 編譯與執行說明 (Usage)
 1. 編譯程式碼 (Compilation)
由於 Server 端使用了多執行緒，編譯時**必須**加上 `-pthread` 參數：
```bash
g++ server.cpp -o server -pthread
g++ client.cpp -o client
