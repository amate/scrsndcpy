/**
*	@file	Socket.cpp
*	@brief	ソケットクラス
*/
/**
	this file is part of Proxydomo
	Copyright (C) amate 2013-

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
#include "stdafx.h"
#include "Socket.h"
#include <chrono>
#pragma comment(lib, "ws2_32.lib")

//#include "DebugWindow.h"
//#include "Logger.h"
//#include "Settings.h"


////////////////////////////////////////////////////////////////////
// IPv4Address

IPv4Address::IPv4Address() : current_addrinfo(nullptr)
{
	::SecureZeroMemory(&addr, sizeof(addr));
	addr.sin_family	= AF_INET;
#ifdef _DEBUG
	port = 0;
#endif
}

IPv4Address& IPv4Address::operator = (sockaddr_in sockaddr)
{
	addr = sockaddr;
#ifdef _DEBUG
	ip = ::inet_ntoa(addr.sin_addr);
	port = ::ntohs(sockaddr.sin_port);
#endif
	return *this;
}

void IPv4Address::SetPortNumber(uint16_t port)
{
	addr.sin_port = htons(port);
#ifdef _DEBUG
	this->port = port;
#endif
}


bool IPv4Address::SetService(const std::string& protocol)
{
	if (protocol == "http") {
		SetPortNumber(80);
		return true;
	} else if (protocol == "https") {
		SetPortNumber(443);
		return true;
	}
	try {
		SetPortNumber(boost::lexical_cast<uint16_t>(protocol));
		return true;
	} catch (...) {
		return false;
	}
}

bool IPv4Address::SetHostName(const std::string& IPorHost)
{
	auto ipret = ::inet_addr(IPorHost.c_str());
	if (ipret != INADDR_NONE) {
		addr.sin_addr.S_un.S_addr = ipret;
#ifdef _DEBUG
		ip = ::inet_ntoa(addr.sin_addr);
#endif
		return true;
	}
	std::string service = boost::lexical_cast<std::string>(GetPortNumber());
	ATLASSERT( !service.empty() );
	addrinfo* result = nullptr;
	addrinfo hinsts = {};
	hinsts.ai_socktype	= SOCK_STREAM;
	hinsts.ai_family	= AF_INET;
	hinsts.ai_protocol	= IPPROTO_TCP;
	for (int tryCount = 0;; ++tryCount) {
		int ret = ::getaddrinfo(IPorHost.c_str(), service.c_str(), &hinsts, &result);
		if (ret == 0) {
			break;
		} else if (ret == EAI_AGAIN) {
			if (tryCount >= 5) {
				WARN_LOG << L"getaddrinfo retry failed: " << IPorHost;
				return false;
			}
			::Sleep(50);
		} else {
			std::wstring strerror = gai_strerror(ret);
			return false;
		}
	}

	addrinfoList.reset(result, [](addrinfo* p) { ::freeaddrinfo(p); });
	current_addrinfo	= addrinfoList.get();

	sockaddr_in sockaddr = *(sockaddr_in*)addrinfoList->ai_addr;
	operator =(sockaddr);

#ifdef _DEBUG
		ip = ::inet_ntoa(addr.sin_addr);
#endif
	return true;
}	

bool IPv4Address::SetNextHost()
{
	if (current_addrinfo == nullptr)
		return false;
	current_addrinfo = current_addrinfo->ai_next;
	if (current_addrinfo) {
		sockaddr_in sockaddr = *(sockaddr_in*)current_addrinfo->ai_addr;
		operator =(sockaddr);
		return true;
	}	
	return false;
}

////////////////////////////////////////////////////////////////
// IPAddress

IPAddress::IPAddress()
{

}

bool	IPAddress::Set(const std::string& IPorHostName, const std::string& protocol)
{
	std::string strPortNumber;
	if (protocol == "http") {
		strPortNumber = "80";
	} else if (protocol == "https") {
		strPortNumber = "443";
	} else {
		strPortNumber = protocol;
	}

	auto ipret = ::inet_addr(IPorHostName.c_str());
	if (ipret != INADDR_NONE) {
		addrinfoList.reset(new addrinfo, [](addrinfo* p) { 
			delete (sockaddr_in*)p->ai_addr;
			delete p;
		});
		addrinfoList->ai_addr = (sockaddr*)new sockaddr_in;
		addrinfoList->ai_addrlen = sizeof(sockaddr_in);
		reinterpret_cast<sockaddr_in*>(addrinfoList->ai_addr)->sin_family = AF_INET;
		reinterpret_cast<sockaddr_in*>(addrinfoList->ai_addr)->sin_addr.S_un.S_addr = ipret;
		reinterpret_cast<sockaddr_in*>(addrinfoList->ai_addr)->sin_port = ::htons(boost::lexical_cast<uint16_t>(strPortNumber));

		addrinfoList->ai_family = AF_INET;     // IPv4
		addrinfoList->ai_socktype = SOCK_STREAM;
		addrinfoList->ai_protocol = IPPROTO_TCP;
		addrinfoList->ai_next = nullptr;

#ifdef _DEBUG
		//ip = ::inet_ntoa(addr.sin_addr);
#endif
		return true;

	} else {
		std::string service = strPortNumber;
		ATLASSERT(service.length());
		addrinfo* result = nullptr;
		addrinfo hints = {};
		hints.ai_family = AF_UNSPEC;     // IPv4/IPv6両対応
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_NUMERICSERV;	// serviceはポート番号
		for (int tryCount = 0;; ++tryCount) {
			int ret = ::getaddrinfo(IPorHostName.c_str(), service.c_str(), &hints, &result);
			if (ret == 0) {
				break;

			} else if (ret == EAI_AGAIN) {
				if (tryCount >= 5) {
					WARN_LOG << L"getaddrinfo retry failed: " << IPorHostName;
					return false;
				}
				::Sleep(50);
			} else {
				std::wstring strerror = gai_strerror(ret);
				WARN_LOG << L"getaddrinfo failed : [" << IPorHostName << L"] " << strerror;
				return false;
			}
		}

		addrinfoList.reset(result, [](addrinfo* p) { ::freeaddrinfo(p); });

#ifdef _DEBUG
		//ip = ::inet_ntoa(addr.sin_addr);
#endif
		return true;
	}
}


////////////////////////////////////////////////////////////////
// CSocket

CSocket::CSocket() : m_sock(0)
{
	m_writeStop = false;
}


bool CSocket::Init()
{
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD( 2, 2 );
	int nRet = ::WSAStartup( wVersionRequested, &wsaData );
	ATLASSERT( nRet == 0 );
	if (nRet == 0)
		return true;
	else
		return false;
}

void CSocket::Term()
{
	int nRet = ::WSACleanup();
	ATLASSERT( nRet == 0 );
}


IPv4Address CSocket::GetFromAddress() const
{
	ATLASSERT( m_sock );
	return m_addrFrom;
}

// ポートとソケットを関連付ける
void	CSocket::Bind(uint16_t port)
{
	m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_sock ==  INVALID_SOCKET)
		throw SocketException("Can`t open socket");

	SetBlocking(false);
	_SetReuse(true);

	sockaddr_in localAddr = { 0 };
	localAddr.sin_family = AF_INET;
	localAddr.sin_port = htons(port);
	if (true/*CSettings::s_privateConnection*/) {
		localAddr.sin_addr.s_addr = ::inet_addr("127.0.0.1");
	} else {
		localAddr.sin_addr.s_addr = INADDR_ANY;
	}
	if( ::bind(m_sock, (sockaddr *)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
		throw SocketException("Can`t bind socket");

	if (::listen(m_sock, SOMAXCONN) == SOCKET_ERROR)
		throw SocketException("Can`t listen",WSAGetLastError());
}

// ポートからの応答を待つ
std::unique_ptr<CSocket>	CSocket::Accept()
{
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(m_sock, &readfds);

	enum { kTimeout = 500 * 1000 };
	timeval timeout = {};
	timeout.tv_usec = kTimeout;
	int ret = ::select(static_cast<int>(m_sock) + 1, &readfds, nullptr, nullptr, &timeout);
	if (ret == SOCKET_ERROR)
		throw SocketException("select failed");

	if (FD_ISSET(m_sock, &readfds) == false)
		return nullptr;

	int fromSize = sizeof(sockaddr_in);
	sockaddr_in from;
	SOCKET conSock = ::accept(m_sock, (sockaddr *)&from, &fromSize);
	if (conSock ==  INVALID_SOCKET)
		return nullptr;

	// 接続があった
    CSocket*	pSocket = new CSocket;
	pSocket->m_sock = conSock;
	pSocket->m_addrFrom = from;
	//pSocket->_setBlocking(false);
	//pSocket->_setBufSize(65535);

	return std::unique_ptr<CSocket>(std::move(pSocket));;
}


enum {
	TEST_SELECT_FAIL,
	TEST_TIMEOUT,
	TEST_WRITE_READY,
	TEST_ERROR_READY
};

static int tcp_select_write(SOCKET socketfd, int to_sec)
{
	fd_set writefds, errfds;
	SOCKET nfds = socketfd + 1;
	struct timeval timeout = { (to_sec > 0) ? to_sec : 0, 0 };
	int result;

	FD_ZERO(&writefds);
	FD_SET(socketfd, &writefds);
	FD_ZERO(&errfds);
	FD_SET(socketfd, &errfds);

	result = select(nfds, NULL, &writefds, &errfds, &timeout);

	if (result == 0)
		return TEST_TIMEOUT;
	else if (result > 0) {
		if (FD_ISSET(socketfd, &writefds))
			return TEST_WRITE_READY;
		else if (FD_ISSET(socketfd, &errfds))
			return TEST_ERROR_READY;
	}

	return TEST_SELECT_FAIL;
}

bool	CSocket::Connect(IPAddress addr, std::atomic_bool& valid)
{
	ATLASSERT(m_sock == 0);

	for (addrinfo* ai = addr.addrinfoList.get(); ai != nullptr; ai = ai->ai_next) {
		m_sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (m_sock == INVALID_SOCKET) {
			m_sock = 0;
			throw SocketException("Can`t open socket");
		}

		SetBlocking(false);
		int ret = ::connect(m_sock, ai->ai_addr, ai->ai_addrlen);
		int error = ::WSAGetLastError();
		if (ret == SOCKET_ERROR && error == WSAEWOULDBLOCK) {
			int select_ret;
			do {
				const int timeout = 1;
				select_ret = tcp_select_write(m_sock, timeout);
				if (select_ret == TEST_WRITE_READY) {
					return true;	// success!
				}

				if (valid == false) {
					return false;
				}
				int optret = 0;
				int optretlen = sizeof(optret);
				int opterror = ::getsockopt(m_sock, SOL_SOCKET, SO_ERROR, (char*)&optret, &optretlen);
				ATLASSERT(opterror == 0);
				if (optret != 0) {
					ATLASSERT(FALSE);
					break;	// fail
				}
			} while (select_ret == TEST_TIMEOUT);

		}
		::closesocket(m_sock);
		m_sock = 0;
		continue;
	}
	return false;
#if 0
	ATLASSERT(m_sock == 0);

	for (addrinfo* ai = addr.addrinfoList.get(); ai != nullptr; ai = ai->ai_next) {
		m_sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (m_sock == INVALID_SOCKET) {
			m_sock = 0;
			throw SocketException("Can`t open socket");
		}
		int ret = ::connect(m_sock, ai->ai_addr, ai->ai_addrlen);
		if (ret == SOCKET_ERROR) {
			::closesocket(m_sock);
			m_sock = 0;
			continue;

		} else {
			return true;	// success!
		}
	}
	return false;
#endif
}
 
void	CSocket::Close()
{
	if (IsConnected()) {
		::shutdown(m_sock, SD_SEND);

		//setReadTimeout(2000);
		auto starttime = std::chrono::steady_clock::now();
		char c[1024];
		try {
			while (IsDataAvailable()) {
				int len = ::recv(m_sock, c, sizeof(c), 0);
				if (len == 0 || len == SOCKET_ERROR)
					break;
				if (std::chrono::steady_clock::now() - starttime > std::chrono::seconds(5))
					break;
			}
		} catch (SocketException& e) {
			ATLTRACE( e.what() );
			ERROR_LOG << L"CSocket::Close : " << e.what();
		}
		::shutdown(m_sock, SD_RECEIVE);
		if (::closesocket(m_sock) == SOCKET_ERROR) {
			m_sock = 0;
			//ATLASSERT( FALSE );
			throw SocketException("closesocket failed");
			//LOG_ERROR("closesocket() error");
		}
		m_sock = 0;
	}
}


bool	CSocket::IsDataAvailable()
{
	if (m_sock == 0)
		return false;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(m_sock, &readfds);
	TIMEVAL nonwait = {};
	int ret = ::select(m_sock + 1, &readfds, nullptr, nullptr, &nonwait);
	if (ret == SOCKET_ERROR)
		throw SocketException("IsDataAvailable failed");

	if (FD_ISSET(m_sock, &readfds)) {
		return true;
	} else {
		return false;
	}
}

int	 CSocket::Read(char* buffer, int length)
{
	if (m_sock == 0)
		return -1;

	//if (IsDataAvailable() == false) {
	//	return 0;
	//}

	ATLASSERT(length > 0);
	int ret = ::recv(m_sock, buffer, length, 0);
	if (ret == 0) {	// disconnect
		Close();
		return 0;

	} else {
		int wsaError = ::WSAGetLastError();
		if (ret == SOCKET_ERROR && wsaError == WSAEWOULDBLOCK) {
			return 0;	// pending
		}
		if (ret < 0) {
			Close();
			return -1;	// error

		} else {
			return ret;	// success!

		}
	}
}


int	CSocket::Write(const char* buffer, int length)
{
	if (m_sock == 0)
		return -1;

	ATLASSERT( length > 0 );
	int ret = 0;

	for (;;) {
		ret = ::send(m_sock, buffer, length, 0);
		int wsaError = ::WSAGetLastError();
		if (ret == SOCKET_ERROR && wsaError == WSAEWOULDBLOCK && m_writeStop == false) {
			::Sleep(10);	// async pending
		} else {
			break;
		}
	}

	if (ret != length) {
		Close();
		return -1;	// error

	} else {
		return ret;	// success!
	}
}


// --------------------------------------------------
void CSocket::SetBlocking(bool yes)
{
	unsigned long op = yes ? 0 : 1;
	if (ioctlsocket(m_sock, FIONBIO, &op) == SOCKET_ERROR)
		throw SocketException("Can`t set blocking");
}

// --------------------------------------------------
void CSocket::_SetReuse(bool yes)
{
	unsigned long op = yes ? 1 : 0;
	if (setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&op, sizeof(op)) == SOCKET_ERROR) 
		throw SocketException("Unable to set REUSE");
}

















