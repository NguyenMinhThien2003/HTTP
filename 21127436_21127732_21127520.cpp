#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <fstream>
#include <netdb.h>
#include <vector>

using namespace std;

const int PORT = 80; // Default HTTP port

int hexCharToDecimal(char hexChar) {
    if (hexChar >= '0' && hexChar <= '9') {
        return hexChar - '0';
    } else if (hexChar >= 'A' && hexChar <= 'F') {
        return hexChar - 'A' + 10;
    } else if (hexChar >= 'a' && hexChar <= 'f') {
        return hexChar - 'a' + 10;
    } else {
        // Xử lý lỗi: Ký tự không hợp lệ trong chuỗi thập lục phân
        throw invalid_argument("Ký tự không hợp lệ trong chuỗi thập lục phân");
    }
}

int hexToDec(const string& hexStr) {
    int decimalValue = 0;
    for (char hexChar : hexStr) {
        decimalValue = decimalValue * 16 + hexCharToDecimal(hexChar);
    }
    return decimalValue;
}

void saveChunk(vector<char> response_data, ofstream& file){
    int length = response_data.size();
    string str(response_data.begin(), response_data.end());
    int start = str.find("\r\n\r\n") + 4;

    string body(response_data.begin() + start, response_data.end());
    ofstream outFile("response.txt", ios::binary);
    outFile.write(body.data(), body.size());
    
    
    int temp;
    while(body != "0\r\n\r\n"){
        temp = body.find("\r\n");
        string hex = body.substr(0, temp);
        int l = hexToDec(hex);
        //cout<<l<<endl;
        string data = body.substr(temp + 2, l);
        body = body.substr(temp + l + 4);
        file<<data;
        // for (int i = 0; i<strlen(data);i++)
        //     file<<data[i];
    }
}

void saveContentLength(vector<char> response_data, ofstream& file){
    int length = response_data.size();
    string str(response_data.begin(), response_data.end());
    int start = str.find("\r\n\r\n") + 4;

    string body(response_data.begin() + start, response_data.end());
    file<<body;
}

int main(int argc, char* argv[]) {
    string hname(argv[1]);
    
    string HOSTNAME = hname.substr(7);
    int idx = HOSTNAME.find("/");
    string path = HOSTNAME.substr(idx + 1);
    HOSTNAME = HOSTNAME.substr(0, idx);
    
    string filename = argv[2];
    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return 1;
    }

    // Set up server address structure
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(PORT);

    // Resolve hostname to IP address using gethostbyname
    struct hostent *host = gethostbyname(HOSTNAME.c_str());
    if (host == NULL) {
        cerr << "Error: Could not resolve hostname" << endl;
        close(sockfd);
        return 1;
    }
    serveraddr.sin_addr = *(struct in_addr*) host->h_addr_list[0];

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    // Build the HTTP request
    string request = "GET /" + path + " HTTP/1.1\r\nHost: "+ HOSTNAME + "\r\nConnection: keep-alive\r\n\r\n"; // Replace with desired method and path

    // Send the request
    if (send(sockfd, request.c_str(), request.length(), 0) == -1) {
        perror("send");
        close(sockfd);
        return 1;
    }

    // Receive response
    vector<char> buffer(4096);
    vector<char> response_data;
    bool chunked = false;
    int content_length = -1;

    while (true) {
        int bytes_received = recv(sockfd, buffer.data(), buffer.size(), 0);
        if (bytes_received == 0) {
            break; // Server closed connection (check for error codes)
        } else if (bytes_received == -1) {
            perror("recv");
            close(sockfd);
            return 1;
        }

        response_data.insert(response_data.end(), buffer.begin(), buffer.begin() + bytes_received);

        // Check for Transfer-Encoding: chunked
        if (!chunked && strstr(response_data.data(), "Transfer-Encoding: chunked") != nullptr) {
            chunked = true;
        }

        // Check for Content-Length
        if (content_length == -1 && strstr(response_data.data(), "Content-Length:") != nullptr) {
            string content_length_str = string(strstr(response_data.data(), "Content-Length: ") + 16); // Skip "Content-Length: "
            content_length = stoi(content_length_str); // Convert string to integer
        }

        // Check if we have received all the data
        if (!chunked && content_length != -1 && response_data.size() >= content_length) {
            break;
        }

        // Check for end of chunked data
        if (chunked && strstr(response_data.data(), "0\r\n\r\n") != nullptr) {
            break;
        }
    }

    // Write response data to file
    ofstream outFile(filename, ios::binary);
    if (!outFile) {
        cerr << "Error: Could not open file for writing" << endl;
        close(sockfd);
        return 1;
    }
    // outFile.write(response_data.data(), response_data.size());
    if(chunked)
        saveChunk(response_data, outFile);
    else
        saveContentLength(response_data, outFile);
    outFile.close();

    // Close the socket
    close(sockfd);

    cout << "Response body saved to " << filename << endl;

    return 0;
}
