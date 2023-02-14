/**
*	@file	Socket.h
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
#pragma once

#include <WinSock2.h>
#include <ws2tcpip.h>
#include <memory>
#include <string>
#include <atomic>
#include <boost\lexical_cast.hpp>
#include <boost\format.hpp>


class GeneralException : public std::runtime_error
{
public:
	GeneralException(const char* errmsg, int e = 0) 
		: runtime_error((boost::format("%1% : %2%") % errmsg % e).str()), err(e)
	{ }

	int		err;
};

class SocketException : public GeneralException
{
public:
	SocketException(const char* errmsg = "socket error", int e = ::WSAGetLastError()) : GeneralException(errmsg, e)
	{ }
};

// 
struct IPv4Address
{
	sockaddr_in	addr;
#ifdef _DEBUG
	std::string	ip;
	uint16_t	port;	
#endif
	std::shared_ptr<addrinfo>	addrinfoList;
	addrinfo*	current_addrinfo;

	IPv4Address();

	IPv4Address& operator = (sockaddr_in sockaddr);

	operator sockaddr*() { return (sockaddr*)&addr; }

	void		SetPortNumber(uint16_t port);
	uint16_t	GetPortNumber() const { return ::ntohs(addr.sin_port); }

	bool SetService(const std::string& protocol);

	bool SetHostName(const std::string& IPorHost);
	bool SetNextHost();
};

struct IPAddress
{
	std::shared_ptr<addrinfo>	addrinfoList;

	IPAddress();

	bool	Set(const std::string& IPorHostName, const std::string& protocol);

};


class SocketIF
{
public:
	virtual ~SocketIF() {}

	virtual bool	IsConnected() = 0;
	virtual bool	IsSecureSocket() = 0;

	virtual SOCKET	GetSocket() = 0;

	virtual void	Close() = 0;
	virtual void	WriteStop() = 0;

	virtual int		Read(char* buffer, int length) = 0;
	virtual int		Write(const char* buffer, int length) = 0;
};


class CSocket : public SocketIF
{
public:
	CSocket();

	// プログラムの開始と終了前に必ず呼び出してください
	static bool Init();
	static void Term();

	bool	IsConnected() override { return m_sock != 0 && m_sock != INVALID_SOCKET; }
	bool	IsSecureSocket() override { return false; }
	SOCKET	GetSocket() override { return m_sock; }
	IPv4Address GetFromAddress() const;

	void	Bind(uint16_t port);
	std::unique_ptr<CSocket>	Accept();

	bool	Connect(IPAddress addr, std::atomic_bool& valid);

	void	Close() override;

	void	WriteStop() override { m_writeStop = true;  }

	bool	IsDataAvailable();

	int		Read(char* buffer, int length) override;
	int		Write(const char* buffer, int length) override;

	void	SetBlocking(bool yes);

private:
	
	void	_SetReuse(bool yes);

	// Data members
	SOCKET	m_sock;
	IPv4Address	m_addrFrom;
	std::atomic_bool	m_writeStop;
};

































