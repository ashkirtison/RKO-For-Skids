#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "Dependecies/server/httplib.h"
using namespace httplib;

#include "Dependecies/server/nlohmann/json.hpp"
using json = nlohmann::json;
#include <lz4.h>
#include <regex>
#include <filesystem>
#include <fstream>
#include <queue>
#include <mutex>
#include <thread>
#include <map>
#include <atomic>
#include <string>

#include <Windows.h>
#include "Utils/Process.hpp"
#include "Utils/Instance.hpp"
#include "Utils/Bytecode.hpp"

extern const char g_DrawingLibUA[];
extern const std::string g_HashLibUA;


class SimpleWebSocket {
public:
	SimpleWebSocket(const std::string& url) : url_(url) {
		ParseUrl(url, host_, port_, path_);
		use_ssl_ = (url.find("wss://") == 0);
		if (use_ssl_ && port_ == 80) port_ = 443;
	}
	~SimpleWebSocket() {
		Close();
	}
	std::string GetLastError() const { return lastError_; }
	bool Connect() {
		return ConnectInternal(host_, port_, path_, 0);
	}
	void Send(const std::string& message) {
		if (!running_ || sock_ == INVALID_SOCKET) return;

		std::vector<uint8_t> frame;
		frame.push_back(0x81); // Fin + Text
		if (message.size() < 126) {
			frame.push_back(0x80 | (uint8_t)message.size()); // Mask bit set
		}
		else if (message.size() < 65536) {
			frame.push_back(0x80 | 126);
			frame.push_back((message.size() >> 8) & 0xFF);
			frame.push_back(message.size() & 0xFF);
		}
		else {
			frame.push_back(0x80 | 127);
			for (int i = 7; i >= 0; i--) {
				frame.push_back((message.size() >> (i * 8)) & 0xFF);
			}
		}
		uint32_t maskKey = rand();
		uint8_t mask[4];
		memcpy(mask, &maskKey, 4);
		frame.push_back(mask[0]);
		frame.push_back(mask[1]);
		frame.push_back(mask[2]);
		frame.push_back(mask[3]);
		for (size_t i = 0; i < message.size(); i++) {
			frame.push_back(message[i] ^ mask[i % 4]);
		}
		std::lock_guard<std::mutex> lock(sendMutex_);
		if (WriteExact((char*)frame.data(), (int)frame.size()) < 0) {
			Close();
		}
	}
	void Close() {
		running_ = false;

		if (ssl_) {
			SSL_shutdown(ssl_);
			SSL_free(ssl_);
			ssl_ = nullptr;
		}
		if (ctx_) {
			SSL_CTX_free(ctx_);
			ctx_ = nullptr;
		}

		if (sock_ != INVALID_SOCKET) {
			closesocket(sock_);
			sock_ = INVALID_SOCKET;
		}
		SafePush("EVENT:CLOSE");
	}
	std::vector<std::string> Poll() {
		std::lock_guard<std::mutex> lock(queueMutex_);
		std::vector<std::string> msgs;
		while (!messageQueue_.empty()) {
			msgs.push_back(messageQueue_.front());
			messageQueue_.pop();
		}
		return msgs;
	}
private:
	std::string GetHeader(const std::string& response, const std::string& headerName) {
		std::string search = headerName;
		std::transform(search.begin(), search.end(), search.begin(), ::tolower);

		std::stringstream ss(response);
		std::string line;
		while (std::getline(ss, line)) {
			if (!line.empty() && line.back() == '\r') line.pop_back();

			size_t colon = line.find(':');
			if (colon != std::string::npos) {
				std::string key = line.substr(0, colon);
				std::string val = line.substr(colon + 1);

				// Trim key
				while (!key.empty() && isspace(key.back())) key.pop_back();
				while (!key.empty() && isspace(key.front())) key.erase(0, 1);
				std::transform(key.begin(), key.end(), key.begin(), ::tolower);

				if (key == search) {
					// Trim val
					while (!val.empty() && isspace(val.back())) val.pop_back();
					while (!val.empty() && isspace(val.front())) val.erase(0, 1);
					return val;
				}
			}
		}
		return "";
	}
	std::string ReadHandshakeResponse() {
		std::string response;
		char buffer[1024];
		while (response.find("\r\n\r\n") == std::string::npos && response.size() < 16384) {
			int r = ReadExact(buffer, sizeof(buffer), false);
			if (r <= 0) break;
			response.append(buffer, r);
		}
		return response;
	}
	bool ConnectInternal(std::string host, int port, std::string path, int redirectCount) {
		if (redirectCount > 5) {
			lastError_ = "Too many redirects";
			return false;
		}
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			lastError_ = "WSAStartup failed";
			return false;
		}
		struct addrinfo hints = {}, * result = nullptr;
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0) {
			lastError_ = "getaddrinfo failed for " + host;
			return false;
		}
		sock_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (sock_ == INVALID_SOCKET) {
			lastError_ = "Socket creation failed";
			freeaddrinfo(result);
			return false;
		}
		if (connect(sock_, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
			lastError_ = "Connect failed: " + std::to_string(WSAGetLastError());
			closesocket(sock_);
			sock_ = INVALID_SOCKET;
			freeaddrinfo(result);
			return false;
		}
		freeaddrinfo(result);
		// SSL Handling
		if (use_ssl_) {
			ctx_ = SSL_CTX_new(TLS_client_method());
			if (!ctx_) {
				lastError_ = "SSL_CTX_new failed";
				closesocket(sock_);
				return false;
			}
			ssl_ = SSL_new(ctx_);
			SSL_set_fd(ssl_, (int)sock_);

			// Set SNI
			SSL_set_tlsext_host_name(ssl_, host.c_str());
			if (SSL_connect(ssl_) != 1) {
				lastError_ = "SSL_connect failed";
				SSL_free(ssl_); ssl_ = nullptr;
				SSL_CTX_free(ctx_); ctx_ = nullptr;
				closesocket(sock_);
				return false;
			}
		}
		std::string key = GenerateKey();
		std::string handshake =
			"GET " + path + " HTTP/1.1\r\n"
			"Host: " + host + "\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: " + key + "\r\n"
			"Sec-WebSocket-Version: 13\r\n\r\n";
		if (WriteExact((char*)handshake.c_str(), (int)handshake.size()) < 0) {
			lastError_ = "Send handshake failed";
			Close();
			return false;
		}
		std::string response = ReadHandshakeResponse();
		if (response.empty()) {
			lastError_ = "Handshake read failed (empty)";
			Close();
			return false;
		}
		// Manual Parsing for robustness
		int statusCode = 0;
		size_t firstSpace = response.find(' ');
		if (firstSpace != std::string::npos) {
			size_t secondSpace = response.find(' ', firstSpace + 1);
			if (secondSpace != std::string::npos) {
				std::string codeStr = response.substr(firstSpace + 1, secondSpace - firstSpace - 1);
				try {
					statusCode = std::stoi(codeStr);
				}
				catch (...) {}
			}
		}
		// Check for redirects
		if (statusCode >= 300 && statusCode < 400) {
			std::string locUrl = GetHeader(response, "Location");
			if (!locUrl.empty()) {
				Close();
				std::string newHost, newPath;
				int newPort;
				ParseUrl(locUrl, newHost, newPort, newPath);

				host = newHost;
				port = newPort;
				path = newPath;
				use_ssl_ = (locUrl.find("wss://") == 0 || locUrl.find("https://") == 0);

				return ConnectInternal(host, port, path, redirectCount + 1);
			}
		}
		if (response.find("101 Switching Protocols") == std::string::npos) {
			lastError_ = "Handshake Failed (Status: " + std::to_string(statusCode) + ")\nResponse:\n" + response;
			Close();
			return false;
		}
		running_ = true;
		readThread_ = std::thread(&SimpleWebSocket::ReadLoop, this);
		readThread_.detach();
		return true;
	}
	void ReadLoop() {
		SafePush("EVENT:OPEN");
		while (running_) {
			uint8_t header[2];
			int r = ReadExact((char*)header, 2, true);
			if (r != 2) break;
			int opcode = header[0] & 0x0F;
			bool masked = (header[1] & 0x80) != 0;
			uint64_t payloadLen = header[1] & 0x7F;
			if (payloadLen == 126) {
				uint8_t lenBytes[2];
				if (ReadExact((char*)lenBytes, 2, true) != 2) break;
				payloadLen = (lenBytes[0] << 8) | lenBytes[1];
			}
			else if (payloadLen == 127) {
				uint8_t lenBytes[8];
				if (ReadExact((char*)lenBytes, 8, true) != 8) break;
				payloadLen = 0;
				for (int i = 0; i < 8; i++) payloadLen = (payloadLen << 8) | lenBytes[i];
			}
			uint8_t mask[4] = { 0 };
			if (masked) {
				if (ReadExact((char*)mask, 4, true) != 4) break;
			}
			std::vector<char> payload(payloadLen);
			if (payloadLen > 0) {
				if (ReadExact(payload.data(), (int)payloadLen, true) != (int)payloadLen) break;
			}
			if (masked) {
				for (size_t i = 0; i < payloadLen; i++) payload[i] ^= mask[i % 4];
			}
			if (opcode == 0x1) {
				SafePush("MSG:" + std::string(payload.begin(), payload.end()));
			}
			else if (opcode == 0x8) break;
		}
		Close();
	}
	int ReadExact(char* buf, int len, bool exact) {
		if (!running_ && exact) return -1;
		int total = 0;
		while (total < len) {
			int received = 0;
			if (use_ssl_ && ssl_) {
				received = SSL_read(ssl_, buf + total, len - total);
			}
			else {
				received = recv(sock_, buf + total, len - total, 0);
			}

			if (received <= 0) return -1;
			total += received;
			if (!exact) break;
		}
		return total;
	}

	int WriteExact(char* buf, int len) {
		if (use_ssl_ && ssl_) {
			return SSL_write(ssl_, buf, len);
		}
		else {
			return send(sock_, buf, len, 0);
		}
	}
	void SafePush(const std::string& msg) {
		std::lock_guard<std::mutex> lock(queueMutex_);
		messageQueue_.push(msg);
	}
	std::string GenerateKey() {
		return "dGhlIHNhbXBsZSBub25jZQ==";
	}
	void ParseUrl(const std::string& url, std::string& host, int& port, std::string& path) {
		std::string u = url;
		bool ssl = false;
		if (u.find("ws://") == 0) u = u.substr(5);
		else if (u.find("wss://") == 0) { u = u.substr(6); ssl = true; }
		else if (u.find("http://") == 0) u = u.substr(7);
		else if (u.find("https://") == 0) { u = u.substr(8); ssl = true; }
		size_t slash = u.find('/');
		std::string hostPort = (slash == std::string::npos) ? u : u.substr(0, slash);
		path = (slash == std::string::npos) ? "/" : u.substr(slash);
		size_t colon = hostPort.find(':');
		if (colon != std::string::npos) {
			host = hostPort.substr(0, colon);
			port = std::stoi(hostPort.substr(colon + 1));
		}
		else {
			host = hostPort;
			port = ssl ? 443 : 80;
		}
	}
	SOCKET sock_ = INVALID_SOCKET;
	SSL_CTX* ctx_ = nullptr;
	SSL* ssl_ = nullptr;
	std::string host_;
	int port_;
	std::string path_;
	std::string url_;
	std::string lastError_;
	bool use_ssl_ = false;
	bool running_ = false;
	std::thread readThread_;
	std::mutex queueMutex_;
	std::queue<std::string> messageQueue_;
	std::mutex sendMutex_;
};

class WebSocketManager {
public:
	static WebSocketManager& Get() {
		static WebSocketManager instance;
		return instance;
	}
	std::string Connect(const std::string& url) {
		std::string id = std::to_string(std::rand()); // Simple ID
		auto ws = std::make_shared<SimpleWebSocket>(url);
		if (ws->Connect()) {
			std::lock_guard<std::mutex> lock(mutex_);
			sockets_[id] = ws;
			return id;
		}
		return "ERROR: " + ws->GetLastError();
	}
	void Send(const std::string& id, const std::string& msg) {
		std::shared_ptr<SimpleWebSocket> ws;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (sockets_.count(id)) ws = sockets_[id];
		}
		if (ws) ws->Send(msg);
	}
	void Close(const std::string& id) {
		std::shared_ptr<SimpleWebSocket> ws;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (sockets_.count(id)) {
				ws = sockets_[id];
				sockets_.erase(id);
			}
		}
		if (ws) ws->Close();
	}
	std::vector<std::string> Poll(const std::string& id) {
		std::shared_ptr<SimpleWebSocket> ws;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (sockets_.count(id)) ws = sockets_[id];
		}
		if (ws) return ws->Poll();
		return {};
	}
private:
	std::map<std::string, std::shared_ptr<SimpleWebSocket>> sockets_;
	std::mutex mutex_;
};

inline std::string script = "";
inline uintptr_t order = 0;
inline std::unordered_map<DWORD, uintptr_t> orders;
inline std::unordered_map<DWORD, std::vector<std::string>> teleportQueues;
inline std::mutex teleportQueueMutex;

inline std::vector<std::string> SplitLines(const std::string& str) {
	std::stringstream ss(str);
	std::string line;
	std::vector<std::string> lines;
	while (std::getline(ss, line, '\n'))
		lines.push_back(line);
	return lines;
}

inline Instance GetPointerInstance(std::string name, DWORD ProcessID) {
	uintptr_t Base = Process::GetModuleBase(ProcessID);
	Instance Datamodel = FetchDatamodel(Base, ProcessID);
	Instance CoreGui = Datamodel.FindFirstChild("CoreGui");
	Instance RKO = CoreGui.FindFirstChild("RKO");
	Instance Pointers = RKO.FindFirstChild("Pointer");
	Instance Pointer = Pointers.FindFirstChild(name);
	uintptr_t Target = ReadMemory<uintptr_t>(Pointer.GetAddress() + offsets::Value, ProcessID);
	return Instance(Target, ProcessID);
}

inline std::unordered_map<std::string, std::function<std::string(std::string, nlohmann::json, DWORD)>> env;
inline void Load() {
	env["listen"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string res;
		if (orders.contains(pid)) {
			if (orders[pid] < order) {
				res = script;
			}
			else {
				res = "";
			}
			orders[pid] = order;
		}
		else {
			orders[pid] = order;
			res = script;
		}
		return res;
		};
	env["compile"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		if (set["enc"] == "true") {
			return Bytecode::Compile(dta);
		}
		return Bytecode::NormalCompile(dta);
		};
	env["setscriptbytecode"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		size_t Sized;
		auto Compressed = Bytecode::Sign(dta, Sized);

		Instance TheScript = GetPointerInstance(set["cn"], pid);
		TheScript.SetScriptBytecode(Compressed, Sized);

		return "";
		};

	env["GetBytecode"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		Instance TheScript = GetPointerInstance(set["cn"], pid);
		std::string compressedBytecode = TheScript.GetBytecode(TheScript.GetAddress());
		return compressedBytecode;
		};
	env["request"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string url = set["l"];
		std::string method = set["m"];
		std::string rBody = set["b"];
		json headersJ = set["h"];

		std::string scheme = "http";
		std::string host;
		std::string path = "/";
		int port = 0;

		if (url.rfind("https://", 0) == 0) {
			scheme = "https";
			url = url.substr(8);
		}
		else if (url.rfind("http://", 0) == 0) {
			scheme = "http";
			url = url.substr(7);
		}

		size_t slash = url.find('/');
		std::string hostPort = (slash == std::string::npos) ? url : url.substr(0, slash);
		if (slash != std::string::npos) path = url.substr(slash);

		size_t colon = hostPort.find(':');
		if (colon != std::string::npos) {
			host = hostPort.substr(0, colon);
			port = std::stoi(hostPort.substr(colon + 1));
		}
		else {
			host = hostPort;
		}

		if (host.empty()) return std::string("[]");
		if (port == 0) port = (scheme == "https") ? 443 : 80;

		Headers headers;
		for (auto it = headersJ.begin(); it != headersJ.end(); ++it) {
			headers.insert({ it.key(), it.value() });
		}

		auto perform = [&](auto& client) -> Result {
			client.set_follow_location(true);
			if (method == "GET") {
				return client.Get(path, headers);
			}
			if (method == "POST") {
				return client.Post(path, headers, rBody, "application/json");
			}
			if (method == "PUT") {
				return client.Put(path, headers, rBody, "application/json");
			}
			if (method == "DELETE") {
				return client.Delete(path, headers, rBody, "application/json");
			}
			if (method == "PATCH") {
				return client.Patch(path, headers, rBody, "application/json");
			}
			return Result();
			};

		Result proxiedRes;
		if (scheme == "https") {
			SSLClient client(host.c_str(), port);
			proxiedRes = perform(client);
		}
		else {
			Client client(host.c_str(), port);
			proxiedRes = perform(client);
		}
		if (!proxiedRes) return std::string("[]");

		json responseJ;
		responseJ["b"] = proxiedRes->body;
		responseJ["c"] = proxiedRes->status;
		responseJ["r"] = proxiedRes->reason;
		responseJ["v"] = proxiedRes->version;

		json rHeadersJ;
		for (const auto& header : proxiedRes->headers) {
			rHeadersJ[header.first] = header.second;
		}
		responseJ["h"] = rHeadersJ;

		return responseJ.dump();
		};


	auto get_path_from_set = [](std::string dta, nlohmann::json set) -> std::string {
		// For functions like appendfile/writefile, path is ALWAYS in set
		if (set.contains("path")) return set["path"];
		if (set.contains("p")) return set["p"];
		// For readfile/listfiles/isfile etc., path might be in dta
		return dta.empty() ? "" : dta;
		};
	auto get_workspace = []() -> std::filesystem::path {
		HMODULE hModule = NULL;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&Load, &hModule);

		std::filesystem::path root;
		if (hModule) {
			char buffer[MAX_PATH];
			GetModuleFileNameA(hModule, buffer, MAX_PATH);
			root = std::filesystem::path(buffer).parent_path();
		}
		else {
			root = std::filesystem::current_path();
		}

		std::filesystem::path ws = root / "workspace";
		if (!std::filesystem::exists(ws)) std::filesystem::create_directory(ws);
		return ws;
		};

	env["writefile"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string filename = set["name"];

		size_t slash = filename.find_last_of("/\\");
		if (slash != std::string::npos) {
			std::filesystem::create_directories(filename.substr(0, slash));
		}

		std::ofstream file(filename, std::ios::binary);
		if (file.is_open()) {
			file.write(dta.c_str(), dta.size());
			file.close();
			return "";
		}
		return "";
		};
	env["Lz4Compress"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		size_t len = dta.size();
		const char* source = dta.c_str();

		const int max_compressed_sz = LZ4_compressBound(len);

		const auto buffer = new char[max_compressed_sz];
		memset(buffer, 0, max_compressed_sz);

		const auto actual_sz = LZ4_compress_default(source, buffer, len, max_compressed_sz);
		return std::string(buffer, actual_sz);
		};
	env["Lz4Decompress"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		size_t len = dta.size();
		const char* source = dta.c_str();
		int data_sz = set["size"];

		char* buffer = new char[data_sz];

		memset(buffer, 0, data_sz);

		LZ4_decompress_safe(source, buffer, len, data_sz);

		return std::string(buffer, data_sz);
		};
	env["DecompileExternal"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		Instance scriptPath = GetPointerInstance(set["scriptPath"], pid);
		if (scriptPath.GetAddress() == 0)
			return std::string("Invalid script instance");
		std::string result = scriptPath.DecompileExternal(scriptPath.GetAddress());
		if (result.empty())
			return std::string("Decompile failed");
		return result;
		};
	env["readfile"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		std::string path = get_path_from_set(dta, set);
		std::filesystem::path filePath(path);
		std::filesystem::path file;
		if (filePath.is_absolute()) {
			file = filePath;
		}
		else {
			std::filesystem::path ws = get_workspace();
			file = ws / path;
		}

		if (!std::filesystem::exists(file)) return std::string("");

		std::ifstream in(file, std::ios::binary);
		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return content;
		};

	env["drawinglib"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		return std::string(g_DrawingLibUA);
		};
	env["hashlib"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		return std::string(g_HashLibUA);
		};

	env["appendfile"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		try {
			std::string path;
			if (set.contains("path")) {
				path = set["path"];
			}
			else if (set.contains("p")) {
				path = set["p"];
			}
			else {
				return std::string("ERROR: No path provided in appendfile");
			}

			if (path.empty()) {
				return std::string("ERROR: Empty path in appendfile");
			}

			std::filesystem::path ws = get_workspace();
			std::filesystem::path file = ws / path;


			for (size_t i = 0; i < dta.size(); ++i) {
				printf("%02X ", (unsigned char)dta[i]);
			}


			// Create parent directories if needed
			if (file.has_parent_path() && !std::filesystem::exists(file.parent_path())) {
				std::filesystem::create_directories(file.parent_path());
			}

			// Open file in append mode (binary to preserve all characters)
			std::ofstream out(file, std::ios::binary | std::ios::app);
			if (!out.is_open()) {
				return std::string("ERROR: Failed to open file: ") + file.string();
			}

			out.write(dta.c_str(), dta.size());
			out.close();

			return std::string(""); // Success
		}
		catch (const std::exception& e) {
			return std::string("ERROR: ") + e.what();
		}
		catch (...) {
			return std::string("ERROR: Unknown error in appendfile");
		}
		};
	env["queue_on_teleport"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		if (dta.empty()) {
			return std::string("ERROR: Empty script");
		}
		{
			std::lock_guard<std::mutex> lock(teleportQueueMutex);
			teleportQueues[pid].push_back(dta);
		}
		return std::string("SUCCESS");
		};
	env["getteleportqueue"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		json out = json::array();
		{
			std::lock_guard<std::mutex> lock(teleportQueueMutex);
			auto it = teleportQueues.find(pid);
			if (it != teleportQueues.end()) {
				for (const auto& entry : it->second) {
					out.push_back(entry);
				}
				it->second.clear();
			}
		}
		return out.dump();
		};
	env["setfpscap"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		int fps = 0;
		try {
			fps = std::stoi(dta);
		}
		catch (...) {
			return std::string("ERROR: Invalid FPS value");
		}

		// Find Roblox installation path
		char* localAppData = nullptr;
		size_t len = 0;
		_dupenv_s(&localAppData, &len, "LOCALAPPDATA");
		std::string robloxPath;

		if (localAppData) {
			robloxPath = std::string(localAppData) + "\\Roblox\\Versions\\";
			free(localAppData);
		}
		else {
			return std::string("ERROR: LOCALAPPDATA not found");
		}

		// Look for the Roblox version directory
		std::filesystem::path targetDir;
		try {
			for (const auto& entry : std::filesystem::directory_iterator(robloxPath)) {
				if (entry.is_directory()) {
					// Check if it's a Roblox version directory
					std::string dirName = entry.path().filename().string();
					if (dirName.find("version-") == 0 ||
						std::filesystem::exists(entry.path() / "RobloxPlayerBeta.exe")) {
						targetDir = entry.path();
						break;
					}
				}
			}
		}
		catch (const std::exception& e) {
			return std::string("ERROR: Could not scan Roblox directory: ") + e.what();
		}

		if (targetDir.empty()) {
			return std::string("ERROR: Roblox directory not found");
		}

		// Create ClientSettings folder and JSON file
		std::filesystem::path clientSettingsDir = targetDir / "ClientSettings";
		std::filesystem::create_directories(clientSettingsDir);

		std::filesystem::path settingsFilePath = clientSettingsDir / "ClientAppSettings.json";
		std::ofstream settingsFile(settingsFilePath);

		if (!settingsFile.is_open()) {
			return std::string("ERROR: Could not create settings file");
		}

		if (fps > 0) {
			// Set a specific FPS cap
			settingsFile << "{\"DFIntTaskSchedulerTargetFps\": " << fps << "}";
		}
		else {
			// Setting to 0 or empty object lets the client decide
			settingsFile << "{}";
		}
		settingsFile.close();

		return std::string("SUCCESS");
		};

	env["getfpscap"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		// Find Roblox installation path
		char* localAppData = nullptr;
		size_t len = 0;
		_dupenv_s(&localAppData, &len, "LOCALAPPDATA");
		std::string robloxPath;

		if (localAppData) {
			robloxPath = std::string(localAppData) + "\\Roblox\\Versions\\";
			free(localAppData);
		}
		else {
			return std::string("ERROR: LOCALAPPDATA not found");
		}

		// Look for the Roblox version directory
		std::filesystem::path targetDir;
		try {
			for (const auto& entry : std::filesystem::directory_iterator(robloxPath)) {
				if (entry.is_directory()) {
					std::string dirName = entry.path().filename().string();
					if (dirName.find("version-") == 0 ||
						std::filesystem::exists(entry.path() / "RobloxPlayerBeta.exe")) {
						targetDir = entry.path();
						break;
					}
				}
			}
		}
		catch (const std::exception& e) {
			return std::string("ERROR: Could not scan Roblox directory: ") + e.what();
		}

		if (targetDir.empty()) {
			return std::string("0"); // Default to 0 (unlimited)
		}

		// Check for settings file
		std::filesystem::path jsonPath = targetDir / "ClientSettings" / "ClientAppSettings.json";

		if (std::filesystem::exists(jsonPath)) {
			try {
				std::ifstream file(jsonPath);
				if (file.is_open()) {
					nlohmann::json jsonData;
					file >> jsonData;
					file.close();

					if (jsonData.contains("DFIntTaskSchedulerTargetFps")) {
						return std::to_string(jsonData["DFIntTaskSchedulerTargetFps"].get<int>());
					}
				}
			}
			catch (const std::exception& e) {
				return std::string("0"); // Error reading, return default
			}
		}

		return std::string("0"); // No custom cap is set
		};

	env["GetCustomAsset"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		if (!set.contains("path")) {
			return std::string{};
		}

		std::string rel = set["path"];
		if (rel.empty()) {
			return std::string{};
		}

		std::filesystem::path relPath(rel);
		if (relPath.is_absolute()) {
			return std::string{};
		}

		std::filesystem::path ws = get_workspace();
		if (!withinDirectory(ws, relPath)) {
			return std::string{};
		}

		std::filesystem::path file = ws / relPath;
		if (!std::filesystem::is_regular_file(file)) {
			return std::string{};
		}

		std::string filename = file.filename().string();
		try {
			std::string robloxpath = GetRobloxFolderFromPID(pid);
			std::string texturesFolder = robloxpath + "\\content\\textures\\";
			if (!std::filesystem::exists(texturesFolder)) {
				return std::string{};
			}
			std::string CustomAssets = texturesFolder + "changething\\";
			if (!std::filesystem::exists(CustomAssets)) {
				std::filesystem::create_directories(CustomAssets);
			}
			std::string destPath = CustomAssets + filename;
			std::filesystem::copy_file(file, destPath, std::filesystem::copy_options::overwrite_existing);
			return std::string("rbxasset://textures/changething/" + filename);
		}
		catch (const std::filesystem::filesystem_error& e) {
			return std::string{};
		}
		return std::string{};
		};

	env["listfiles"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		std::string path = get_path_from_set(dta, set);
		std::filesystem::path ws = get_workspace();
		std::filesystem::path dir = ws / path;

		json files = json::array();
		if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
			for (const auto& entry : std::filesystem::directory_iterator(dir)) {
				files.push_back(entry.path().string());
			}
		}
		return files.dump();
		};

	env["isfile"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		std::string path = get_path_from_set(dta, set);
		std::filesystem::path ws = get_workspace();
		std::filesystem::path file = ws / path;
		return std::string(std::filesystem::exists(file) && std::filesystem::is_regular_file(file) ? "true" : "false");
		};

	env["isfolder"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		std::string path = get_path_from_set(dta, set);
		std::filesystem::path ws = get_workspace();
		std::filesystem::path file = ws / path;
		return std::string(std::filesystem::exists(file) && std::filesystem::is_directory(file) ? "true" : "false");
		};

	env["makefolder"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		std::string path = get_path_from_set(dta, set);
		std::filesystem::path ws = get_workspace();
		std::filesystem::path dir = ws / path;
		std::filesystem::create_directories(dir);
		return std::string("");
		};

	env["delfolder"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		std::string path = get_path_from_set(dta, set);
		std::filesystem::path ws = get_workspace();
		std::filesystem::path dir = ws / path;
		if (std::filesystem::exists(dir)) std::filesystem::remove_all(dir);
		return std::string("");
		};

	env["delfile"] = [&](std::string dta, nlohmann::json set, DWORD pid) {
		std::string path = get_path_from_set(dta, set);
		std::filesystem::path ws = get_workspace();
		std::filesystem::path file = ws / path;
		if (std::filesystem::exists(file)) std::filesystem::remove(file);
		return std::string("");
		};

	// WebSocket handlers using WebSocketManager
	env["websocket.connect"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string url = "";
		if (set.contains("url")) url = set["url"];
		else return std::string("ERROR: No URL provided");
		return WebSocketManager::Get().Connect(url);
		};

	env["websocket.send"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string id = set.contains("id") ? set["id"].get<std::string>() : "";
		std::string msg = dta;
		if (id.empty()) return std::string("ERROR: No ID");
		WebSocketManager::Get().Send(id, msg);
		return std::string("");
		};

	env["websocket.close"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string id = set.contains("id") ? set["id"].get<std::string>() : "";
		if (id.empty()) return std::string("ERROR: No ID");
		WebSocketManager::Get().Close(id);
		return std::string("");
		};

	env["websocket.poll"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		std::string id = set.contains("id") ? set["id"].get<std::string>() : "";
		json j;
		if (id.empty()) return j.dump();
		auto msgs = WebSocketManager::Get().Poll(id);
		std::vector<std::string> out;
		bool closed = false;
		for (const auto& msg : msgs) {
			if (msg == "EVENT:CLOSE") {
				closed = true;
			}
			else if (msg.rfind("MSG:", 0) == 0) {
				out.push_back(msg.substr(4));
			}
		}
		if (!out.empty()) j["m"] = out;
		if (closed) j["c"] = true;
		return j.dump();
		};
	// Add this in the Load() function where other env functions are defined
	env["setclipboard"] = [](std::string dta, nlohmann::json set, DWORD pid) {
		// Open the clipboard
		if (!OpenClipboard(nullptr)) {
			return std::string("ERROR: Failed to open clipboard");
		}

		// Empty the clipboard first
		EmptyClipboard();

		// Allocate global memory for the string
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dta.size() + 1);
		if (hMem == nullptr) {
			CloseClipboard();
			return std::string("ERROR: Failed to allocate memory");
		}

		// Copy the string to the allocated memory
		char* pMem = static_cast<char*>(GlobalLock(hMem));
		if (pMem == nullptr) {
			GlobalFree(hMem);
			CloseClipboard();
			return std::string("ERROR: Failed to lock memory");
		}

		memcpy(pMem, dta.c_str(), dta.size() + 1);
		GlobalUnlock(hMem);

		// Set the clipboard data
		if (!SetClipboardData(CF_TEXT, hMem)) {
			GlobalFree(hMem);
			CloseClipboard();
			return std::string("ERROR: Failed to set clipboard data");
		}

		// Clean up
		CloseClipboard();

		return std::string("SUCCESS");
		};

}

inline std::string Setup(std::string args) {
	auto lines = SplitLines(args);

	std::string typ = lines.size() > 0 ? lines[0] : "";
	DWORD pid = lines.size() > 1 ? std::stoul(lines[1]) : 0;
	nlohmann::json set = lines.size() > 2 ? nlohmann::json::parse(lines[2]) : nlohmann::json{};
	// Better: join with the original newlines
	std::string dta;
	if (lines.size() > 3) {
		// Find where the third line ends in the original string
		size_t pos = 0;
		for (int i = 0; i < 3; ++i) {
			pos = args.find('\n', pos);
			if (pos == std::string::npos) break;
			pos++; // Move past the newline
		}

		if (pos != std::string::npos && pos < args.length()) {
			dta = args.substr(pos); // Get everything after the third newline
		}
	}

	return env[typ] ? env[typ](dta, set, pid) : "";
}

inline void StartBridge()
{
	Load();
	Server Bridge;
	Bridge.Post("/handle", [](const Request& req, Response& res) {
		res.status = 200;
		res.set_content(Setup(req.body), "text/plain");
		});
	Bridge.set_exception_handler([](const Request& req, Response& res, std::exception_ptr ep) {
		std::string errorMessage;
		try {
			std::rethrow_exception(ep);
		}
		catch (std::exception& e) {
			errorMessage = e.what();
		}
		catch (...) {
			errorMessage = "Unknown Exception";
		}
		res.set_content("{\"error\":\"" + errorMessage + "\"}", "application/json");
		res.status = 500;
		});
	Bridge.listen("localhost", 9611);
}

inline void Execute(std::string source) {
	script = source;
	order += 1;
}
