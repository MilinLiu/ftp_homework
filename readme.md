# C++ Multi-Threaded FTP Server & Client
這是一個基於 Linux Socket API 與 C++11 標準實作的檔案傳輸系統 (FTP)。
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
 🚀 編譯與執行說明 (Usage)
 1. 編譯程式碼 (Compilation)
由於 Server 端使用了多執行緒，編譯時**必須**加上 `-pthread` 參數：
```bash
g++ server.cpp -o server -pthread
g++ client.cpp -o client
